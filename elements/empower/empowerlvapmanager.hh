#ifndef CLICK_EMPOWERLVAPMANAGER_HH
#define CLICK_EMPOWERLVAPMANAGER_HH
#include <click/config.h>
#include <click/element.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/hashtable.hh>
#include <clicknet/wifi.h>
#include <click/sync.hh>
#include <elements/wifi/minstrel.hh>
#include "empowerrxstats.hh"
#include "empowerpacket.hh"
#include "igmppacket.hh"
#include "empowermulticasttable.hh"
CLICK_DECLS

/*
=c

EmpowerLVAPManager(HWADDR, EBS, DEBUGFS[, I<KEYWORDS>])

=s EmPOWER

Handles LVAPs and communication with the Access Controller.

=d

Keyword arguments are:

=over 8

=item HWADDR
The raw wireless interface hardware address

=item EBS
An EmpowerBeaconSource element

=item EAUTHR
An EmpowerOpenAuthResponder element

=item EASSOR
An EmpowerAssociationResponder element

=item DEBUGFS
The path to the bssid_extra file

=item EPSB
An EmpowerPowerSaveBuffer element

=item PERIOD
Interval between hello messages to the Access Controller (in msec), default is 5000

=item EDEAUTHR
An EmpowerDeAuthResponder element

=item EDISASSOR
An EmpowerDisassocResponder element

=item DEBUG
Turn debug on/off

=back 8

=a EmpowerLVAPManager
*/

enum empower_port_flags {
    EMPOWER_STATUS_PORT_NOACK = (1<<0),
};

enum empower_lvap_flags {
    EMPOWER_STATUS_LVAP_AUTHENTICATED = (1<<0),
    EMPOWER_STATUS_LVAP_ASSOCIATED = (1<<1),
    EMPOWER_STATUS_LVAP_SET_MASK = (1<<2),
};

enum empower_bands_types {
    EMPOWER_BT_L20 = 0x0,
    EMPOWER_BT_HT20 = 0x1,
};

enum empower_aggregation_flags {
	EMPOWER_AMSDU_AGGREGATION = (1<<0)
};

enum empower_slice_scheduleruler {
	EMPOWER_AIRTIME_ROUND_ROBIN = 0x0,
	EMPOWER_AIRTIME_FAIRNESS = 0x1,
};

enum empower_regmon_types {
    EMPOWER_REGMON_TX = 0x0,
	EMPOWER_REGMON_RX = 0x1,
	EMPOWER_REGMON_ED = 0x2,
};

typedef HashTable<uint16_t, uint32_t> CBytes;
typedef CBytes::iterator CBytesIter;

class Minstrel;
class EmpowerQOSManager;
class EmpowerRegmon;

class NetworkPort {
public:
	EtherAddress _hwaddr;
	String _iface;
	uint16_t _port_id;
	NetworkPort() :
			_hwaddr(EtherAddress()), _port_id(0) {
	}
	NetworkPort(EtherAddress hwaddr, String iface, uint16_t port_id) :
			_hwaddr(hwaddr), _iface(iface), _port_id(port_id) {
	}
	String unparse() {
		StringAccum sa;
		sa << _hwaddr.unparse() << ' ' << _iface << ' ' << _port_id;
		return sa.take_string();
	}
};

// An EmPOWER Virtual Access Point or VAP. This is an AP than
// can be used by multiple clients (unlike the LVAP that is
// specific to each client).
class EmpowerVAPState {
public:
	EtherAddress _bssid;
	String _ssid;
	EtherAddress _hwaddr;
	int _channel;
	int _band;
	int _iface_id;
};

// An EmPOWER Network. This is a tuple BSSID/SSID to be advertised
// the LVAP
class EmpowerNetwork {
public:
	EtherAddress _bssid;
	String _ssid;
	EmpowerNetwork(EtherAddress bssid, String ssid) :
			_bssid(bssid), _ssid(ssid) {
	}
	String unparse() {
		StringAccum sa;
		sa << '<' << _bssid.unparse() << ", " << _ssid << '>';
		return sa.take_string();
	}
};

// An EmPOWER Light Virtual Access Point or LVAP. This is an
// AP that is defined for a specific client.
class EmpowerStationState {
public:
	EtherAddress _sta;
	EtherAddress _bssid;
	String _ssid;
	EtherAddress _encap;
	Vector<EmpowerNetwork> _networks;
	int _assoc_id;
	EtherAddress _hwaddr;
	int _channel;
	empower_bands_types _band;
	empower_bands_types _supported_band;
	int _iface_id;
	bool _set_mask;
	bool _authentication_status;
	bool _association_status;
	// CSA entries
	bool _csa_active;
	int _csa_switch_mode;
	int _csa_switch_count;
	int _csa_switch_channel;
	// ADD/DEL LVAP response entries
	uint32_t _module_id;
	bool is_valid(int iface_id) {
		if (_iface_id != iface_id) {
			return false;
		}
		if (!_set_mask) {
			return false;
		}
		if (!_authentication_status || !_association_status) {
			return false;
		}
		if (!_ssid || !_bssid) {
			return false;
		}
		return true;
	}
};

// Cross structure mapping bssids to list of associated
// station and to the interface id
class InfoBssid {
public:
	EtherAddress _bssid;
	Vector<EtherAddress> _stas;
	int _iface_id;
};

typedef HashTable<EtherAddress, InfoBssid> InfoBssids;
typedef InfoBssids::iterator IBIter;

typedef HashTable<EtherAddress, EmpowerVAPState> VAP;
typedef VAP::iterator VAPIter;

typedef HashTable<EtherAddress, EmpowerStationState> LVAP;
typedef LVAP::iterator LVAPIter;

typedef HashTable<int, NetworkPort> Ports;
typedef Ports::iterator PortsIter;

class ResourceElement {
public:

	EtherAddress _hwaddr;
	int _channel;
	empower_bands_types _band;

	ResourceElement() :
			_hwaddr(EtherAddress()), _channel(1), _band(EMPOWER_BT_L20) {
	}

	ResourceElement(EtherAddress hwaddr, int channel, empower_bands_types band) :
			_hwaddr(hwaddr), _channel(channel), _band(band) {
	}

	inline size_t hashcode() const {
		return _hwaddr.hashcode();
	}

	inline String unparse() const {
		StringAccum sa;
		sa << "(";
		sa << _hwaddr.unparse();
		sa << ", ";
		sa << _channel;
		sa << ", ";
		switch (_band) {
		case EMPOWER_BT_L20:
			sa << "L20";
			break;
		case EMPOWER_BT_HT20:
			sa << "HT20";
			break;
		}
		sa << ")";
		return sa.take_string();
	}

};

inline bool operator==(const ResourceElement &a, const ResourceElement &b) {
	return a._hwaddr == b._hwaddr && a._channel == b._channel && a._band == b._band;
}

typedef HashTable<int, ResourceElement *> RETable;
typedef RETable::const_iterator REIter;

class EmpowerLVAPManager: public Element {
public:

	EmpowerLVAPManager();
	~EmpowerLVAPManager();

	const char *class_name() const { return "EmpowerLVAPManager"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();
	void run_timer(Timer *);
	void reset();

	void push(int, Packet *);

	int handle_add_lvap(Packet *, uint32_t);
	int handle_del_lvap(Packet *, uint32_t);
	int handle_add_vap(Packet *, uint32_t);
	int handle_del_vap(Packet *, uint32_t);
	int handle_probe_response(Packet *, uint32_t);
	int handle_auth_response(Packet *, uint32_t);
	int handle_assoc_response(Packet *, uint32_t);
	int handle_counters_request(Packet *, uint32_t);
	int handle_txp_counters_request(Packet *, uint32_t);
	int handle_add_rssi_trigger(Packet *, uint32_t);
	int handle_del_rssi_trigger(Packet *, uint32_t);
	int handle_del_summary_trigger(Packet *, uint32_t);
	int handle_add_summary_trigger(Packet *, uint32_t);
	int handle_uimg_request(Packet *, uint32_t);
	int handle_nimg_request(Packet *, uint32_t);
	int handle_set_port(Packet *, uint32_t);
	int handle_del_port(Packet *, uint32_t);
	int handle_frames_request(Packet *, uint32_t);
	int handle_lvap_stats_request(Packet *, uint32_t);
	int handle_wifi_stats_request(Packet *, uint32_t);
	int handle_caps_request(Packet *, uint32_t);
	int handle_lvap_status_request(Packet *, uint32_t);
	int handle_vap_status_request(Packet *, uint32_t);
	int handle_set_slice(Packet *, uint32_t);
	int handle_del_slice(Packet *, uint32_t);
	int handle_slice_queue_counters_request(Packet *, uint32_t);
	int handle_slice_status_request(Packet *, uint32_t);
	int handle_port_status_request(Packet *, uint32_t);
	//TFM Function prototipe
	int handle_wtp_channel_update_request(Packet *p, uint32_t offset);

	void send_hello();
	void send_probe_request(EtherAddress, String, EtherAddress, int, empower_bands_types, empower_bands_types);
	void send_auth_request(EtherAddress, EtherAddress);
	void send_association_request(EtherAddress, EtherAddress, String, EtherAddress, int, empower_bands_types, empower_bands_types);
	void send_status_lvap(EtherAddress);
	void send_status_vap(EtherAddress);
	void send_status_port(EtherAddress, int);
	void send_status_slice(String, int, int);
	void send_counters_response(EtherAddress, uint32_t);
	void send_txp_counters_response(uint32_t, EtherAddress, uint8_t, empower_bands_types, EtherAddress);
	void send_img_response(int, uint32_t, EtherAddress, uint8_t, empower_bands_types);
	void send_wifi_stats_response(uint32_t, EtherAddress, uint8_t, empower_bands_types);
	void send_caps();
	void send_rssi_trigger(uint32_t, uint32_t, uint8_t);
	void send_summary_trigger(SummaryTrigger *);
	void send_lvap_stats_response(EtherAddress, uint32_t);
	void send_incoming_mcast_address (EtherAddress, int);
	void send_igmp_report(EtherAddress, Vector<IPAddress>*, Vector<enum empower_igmp_record_type>*);
	void send_add_del_lvap_response(uint8_t, EtherAddress, uint32_t, uint32_t);
	void send_slice_queue_counters_response(uint32_t, EtherAddress, uint8_t, empower_bands_types, String, int);

	ReadWriteLock* lock() { return &_lock; }
	LVAP* lvaps() { return &_lvaps; }
	VAP* vaps() { return &_vaps; }
	EtherAddress wtp() { return _wtp; }

	uint32_t get_next_seq() { return ++_seq; }

	int remove_lvap(EtherAddress sta) {

		EmpowerStationState *ess = _lvaps.get_pointer(sta);

		// Forget station
		_rcs[ess->_iface_id]->tx_policies()->tx_table()->erase(ess->_sta);
		_rcs[ess->_iface_id]->forget_station(ess->_sta);

		// Erase lvap
		_lvaps.erase(_lvaps.find(ess->_sta));

		// Remove this VAP's BSSID from the mask
		compute_bssid_mask();

		return 0;

	}

	int element_to_iface(EtherAddress hwaddr, uint8_t channel, empower_bands_types band) {
		for (REIter iter = _ifaces_to_elements.begin(); iter.live(); iter++) {
			if (iter.value()->_hwaddr == hwaddr && iter.value()->_channel == channel && iter.value()->_band == band) {
				return iter.key();
			}
		}
		return -1;
	}

	ResourceElement* iface_to_element(int iface) {
		return _ifaces_to_elements.get(iface);
	}

	int num_ifaces() {
		return _rcs.size();
	}

	EmpowerStationState * get_ess(EtherAddress sta) {
		return _lvaps.get_pointer(sta);
	}

	TxPolicyInfo * get_txp(EtherAddress sta) {
		EmpowerStationState *ess = _lvaps.get_pointer(sta);
		if (!ess) {
			return 0;
		}
		Minstrel * rc = _rcs[ess->_iface_id];
		TxPolicyInfo * txp = rc->tx_policies()->lookup(ess->_sta);
		return txp;
	}

	TransmissionPolicies * get_tx_policies(int iface_id) {
		return _rcs[iface_id]->tx_policies();
	}

	Vector<EtherAddress> * get_mcast_receivers(EtherAddress sta) {
		return _mtbl->get_receivers(sta);
	}

	bool is_unique_lvap(EtherAddress sta) {
		EmpowerStationState *ess = _lvaps.get_pointer(sta);
		if (!ess) {
			return false;
		}
		EmpowerVAPState *vap = _vaps.get_pointer(ess->_bssid);
		if (vap) {
			return false;
		}
		return true;
	}

private:

	ReadWriteLock _lock;

	RETable _ifaces_to_elements;

	void compute_bssid_mask();
	void send_message(Packet *);

	class Empower11k *_e11k;
	class EmpowerBeaconSource *_ebs;
	class EmpowerOpenAuthResponder *_eauthr;
	class EmpowerAssociationResponder *_eassor;
	class EmpowerDeAuthResponder *_edeauthr;
	class EmpowerRXStats *_ers;
	class EmpowerMulticastTable * _mtbl;

	LVAP _lvaps;
	Ports _ports;
	VAP _vaps;
	Vector<EtherAddress> _masks;
	Vector<Minstrel *> _rcs;
	Vector<EmpowerRegmon *> _regmons;
	Vector<EmpowerQOSManager *> _eqms;
	Vector<String> _debugfs_strings;
	Timer _timer;
	uint32_t _seq;
	EtherAddress _wtp;
	uint8_t _dpid[8];
	unsigned int _period; // msecs
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
