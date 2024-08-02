// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/strings/cstring_view.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "net/base/address_map_linux.h"
#endif

namespace net {

class NetworkChangeNotifierFactory;
struct NetworkInterface;
class SystemDnsConfigChangeNotifier;
typedef std::vector<NetworkInterface> NetworkInterfaceList;

namespace internal {
#if BUILDFLAG(IS_FUCHSIA)
class NetworkInterfaceCache;
#endif
}  // namespace internal

// NetworkChangeNotifier monitors the system for network changes, and notifies
// registered observers of those events.  Observers may register on any thread,
// and will be called back on the thread from which they registered.
// NetworkChangeNotifiers are threadsafe, though they must be created and
// destroyed on the same thread.
class NET_EXPORT NetworkChangeNotifier {
 public:
  // This is a superset of the connection types in the NetInfo v3 specification:
  // http://w3c.github.io/netinfo/.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
  //
  // New enum values should only be added to the end of the enum and no values
  // should be modified or reused, as this is reported via UMA.
  enum ConnectionType {
    CONNECTION_UNKNOWN = 0,  // A connection exists, but its type is unknown.
                             // Also used as a default value.
    CONNECTION_ETHERNET = 1,
    CONNECTION_WIFI = 2,
    CONNECTION_2G = 3,
    CONNECTION_3G = 4,
    CONNECTION_4G = 5,
    CONNECTION_NONE = 6,  // No connection.
    CONNECTION_BLUETOOTH = 7,
    CONNECTION_5G = 8,
    CONNECTION_LAST = CONNECTION_5G
  };

  // This is the NetInfo v3 set of connection technologies as seen in
  // http://w3c.github.io/netinfo/.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
  //
  // TODO(crbug.com/40148439): Introduce subtypes for 5G networks once they can
  // be detected.
  enum ConnectionSubtype {
    SUBTYPE_UNKNOWN = 0,
    SUBTYPE_NONE,
    SUBTYPE_OTHER,
    SUBTYPE_GSM,
    SUBTYPE_IDEN,
    SUBTYPE_CDMA,
    SUBTYPE_1XRTT,
    SUBTYPE_GPRS,
    SUBTYPE_EDGE,
    SUBTYPE_UMTS,
    SUBTYPE_EVDO_REV_0,
    SUBTYPE_EVDO_REV_A,
    SUBTYPE_HSPA,
    SUBTYPE_EVDO_REV_B,
    SUBTYPE_HSDPA,
    SUBTYPE_HSUPA,
    SUBTYPE_EHRPD,
    SUBTYPE_HSPAP,
    SUBTYPE_LTE,
    SUBTYPE_LTE_ADVANCED,
    SUBTYPE_BLUETOOTH_1_2,
    SUBTYPE_BLUETOOTH_2_1,
    SUBTYPE_BLUETOOTH_3_0,
    SUBTYPE_BLUETOOTH_4_0,
    SUBTYPE_ETHERNET,
    SUBTYPE_FAST_ETHERNET,
    SUBTYPE_GIGABIT_ETHERNET,
    SUBTYPE_10_GIGABIT_ETHERNET,
    SUBTYPE_WIFI_B,
    SUBTYPE_WIFI_G,
    SUBTYPE_WIFI_N,
    SUBTYPE_WIFI_AC,
    SUBTYPE_WIFI_AD,
    SUBTYPE_LAST = SUBTYPE_WIFI_AD
  };

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum ConnectionCost {
    CONNECTION_COST_UNKNOWN = 0,
    CONNECTION_COST_UNMETERED,
    CONNECTION_COST_METERED,
    CONNECTION_COST_LAST
  };

  // DEPRECATED. Please use NetworkChangeObserver instead. crbug.com/754695.
  class NET_EXPORT IPAddressObserver {
   public:
    IPAddressObserver(const IPAddressObserver&) = delete;
    IPAddressObserver& operator=(const IPAddressObserver&) = delete;

    // Will be called when the IP address of the primary interface changes.
    // This includes when the primary interface itself changes.
    virtual void OnIPAddressChanged() = 0;

   protected:
    IPAddressObserver();
    virtual ~IPAddressObserver();

   private:
    friend NetworkChangeNotifier;
    scoped_refptr<base::ObserverListThreadSafe<IPAddressObserver>>
        observer_list_;
  };

  // DEPRECATED. Please use NetworkChangeObserver instead. crbug.com/754695.
  class NET_EXPORT ConnectionTypeObserver {
   public:
    ConnectionTypeObserver(const ConnectionTypeObserver&) = delete;
    ConnectionTypeObserver& operator=(const ConnectionTypeObserver&) = delete;
    // Will be called when the connection type of the system has changed.
    // See NetworkChangeNotifier::GetConnectionType() for important caveats
    // about the unreliability of using this signal to infer the ability to
    // reach remote sites.
    virtual void OnConnectionTypeChanged(ConnectionType type) = 0;

   protected:
    ConnectionTypeObserver();
    virtual ~ConnectionTypeObserver();

   private:
    friend NetworkChangeNotifier;
    scoped_refptr<base::ObserverListThreadSafe<ConnectionTypeObserver>>
        observer_list_;
  };

  class NET_EXPORT DNSObserver {
   public:
    DNSObserver(const DNSObserver&) = delete;
    DNSObserver& operator=(const DNSObserver&) = delete;

    // Will be called when the DNS settings of the system may have changed.
    virtual void OnDNSChanged() = 0;

   protected:
    DNSObserver();
    virtual ~DNSObserver();

   private:
    friend NetworkChangeNotifier;
    scoped_refptr<base::ObserverListThreadSafe<DNSObserver>> observer_list_;
  };

  class NET_EXPORT NetworkChangeObserver {
   public:
    NetworkChangeObserver(const NetworkChangeObserver&) = delete;
    NetworkChangeObserver& operator=(const NetworkChangeObserver&) = delete;

    // OnNetworkChanged will be called when a change occurs to the host
    // computer's hardware or software that affects the route network packets
    // take to any network server. Some examples:
    //   1. A network connection becoming available or going away. For example
    //      plugging or unplugging an Ethernet cable, WiFi or cellular modem
    //      connecting or disconnecting from a network, or a VPN tunnel being
    //      established or taken down.
    //   2. An active network connection's IP address changes.
    //   3. A change to the local IP routing tables.
    // The signal shall only be produced when the change is complete.  For
    // example if a new network connection has become available, only give the
    // signal once we think the O/S has finished establishing the connection
    // (i.e. DHCP is done) to the point where the new connection is usable.
    // The signal shall not be produced spuriously as it will be triggering some
    // expensive operations, like socket pools closing all connections and
    // sockets and then re-establishing them.
    // |type| indicates the type of the active primary network connection after
    // the change.  Observers performing "constructive" activities like trying
    // to establish a connection to a server should only do so when
    // |type != CONNECTION_NONE|.  Observers performing "destructive" activities
    // like resetting already established server connections should only do so
    // when |type == CONNECTION_NONE|.  OnNetworkChanged will always be called
    // with CONNECTION_NONE immediately prior to being called with an online
    // state; this is done to make sure that destructive actions take place
    // prior to constructive actions.
    virtual void OnNetworkChanged(ConnectionType type) = 0;

   protected:
    NetworkChangeObserver();
    virtual ~NetworkChangeObserver();

   private:
    friend NetworkChangeNotifier;
    scoped_refptr<base::ObserverListThreadSafe<NetworkChangeObserver>>
        observer_list_;
  };

  class NET_EXPORT MaxBandwidthObserver {
   public:
    MaxBandwidthObserver(const MaxBandwidthObserver&) = delete;
    MaxBandwidthObserver& operator=(const MaxBandwidthObserver&) = delete;

    // Called when a change occurs to the network's maximum bandwidth as
    // defined in http://w3c.github.io/netinfo/. Also called on type change,
    // even if the maximum bandwidth doesn't change. See the documentation of
    // GetMaxBanwidthAndConnectionType for what to expect for the values of
    // |max_bandwidth_mbps|.
    virtual void OnMaxBandwidthChanged(double max_bandwidth_mbps,
                                       ConnectionType type) = 0;

   protected:
    MaxBandwidthObserver();
    virtual ~MaxBandwidthObserver();

   private:
    friend NetworkChangeNotifier;
    scoped_refptr<base::ObserverListThreadSafe<MaxBandwidthObserver>>
        observer_list_;
  };

  class NET_EXPORT ConnectionCostObserver {
   public:
    // Not copyable or movable
    ConnectionCostObserver(const ConnectionCostObserver&) = delete;
    ConnectionCostObserver& operator=(const ConnectionCostObserver&) = delete;

    // Will be called when the connection cost of the default network connection
    // of the system has changed. This will only fire if the connection cost
    // actually changes, regardless of any other network-related changes that
    // might have occurred (for example, changing from ethernet to wifi won't
    // update this unless that change also results in a cost change). The cost
    // is not tied directly to any other network-related states, as you could
    // simply change the current connection from unmetered to metered. It is
    // safe to assume that network traffic will default to this cost once this
    // has fired.
    virtual void OnConnectionCostChanged(ConnectionCost Cost) = 0;

   protected:
    ConnectionCostObserver();
    virtual ~ConnectionCostObserver();

   private:
    friend NetworkChangeNotifier;
    scoped_refptr<base::ObserverListThreadSafe<ConnectionCostObserver>>
        observer_list_;
  };

  // A list of networks.
  typedef std::vector<handles::NetworkHandle> NetworkList;

  // An interface that when implemented and added via AddNetworkObserver(),
  // provides notifications when networks come and go.
  // Only implemented for Android (Lollipop and newer), no callbacks issued when
  // unimplemented.
  class NET_EXPORT NetworkObserver {
   public:
    NetworkObserver(const NetworkObserver&) = delete;
    NetworkObserver& operator=(const NetworkObserver&) = delete;

    // Called when device connects to |network|. For example device associates
    // with a WiFi access point. This does not imply the network has Internet
    // access as it may well be behind a captive portal.
    virtual void OnNetworkConnected(handles::NetworkHandle network) = 0;
    // Called when device disconnects from |network|.
    virtual void OnNetworkDisconnected(handles::NetworkHandle network) = 0;
    // Called when device determines the connection to |network| is no longer
    // preferred, for example when a device transitions from cellular to WiFi
    // it might deem the cellular connection no longer preferred. The device
    // will disconnect from |network| in a period of time (30s on Android),
    // allowing network communications via |network| to wrap up.
    virtual void OnNetworkSoonToDisconnect(handles::NetworkHandle network) = 0;
    // Called when |network| is made the default network for communication.
    virtual void OnNetworkMadeDefault(handles::NetworkHandle network) = 0;

   protected:
    NetworkObserver();
    virtual ~NetworkObserver();

   private:
    friend NetworkChangeNotifier;
    scoped_refptr<base::ObserverListThreadSafe<NetworkObserver>> observer_list_;
  };

  // An interface that when implemented and added via
  // AddDefaultNetworkActiveObserver(), provides notifications when the system
  // default network has gone in to a high power state.
  // Only implemented for Android (Lollipop and newer), no callbacks issued when
  // unimplemented.
  class NET_EXPORT DefaultNetworkActiveObserver {
   public:
    DefaultNetworkActiveObserver(const DefaultNetworkActiveObserver&) = delete;
    DefaultNetworkActiveObserver& operator=(
        const DefaultNetworkActiveObserver&) = delete;

    // Called when device default network goes in to a high power state.
    virtual void OnDefaultNetworkActive() = 0;

   protected:
    DefaultNetworkActiveObserver();
    virtual ~DefaultNetworkActiveObserver();

   private:
    friend NetworkChangeNotifier;
    scoped_refptr<base::ObserverListThreadSafe<DefaultNetworkActiveObserver>>
        observer_list_;
  };

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/40232923): Remove this section and align the behavior
  // with other platforms or confirm that Lacros needs to be separated.
  static constexpr ConnectionType kDefaultInitialConnectionType =
      CONNECTION_UNKNOWN;
  static constexpr ConnectionSubtype kDefaultInitialConnectionSubtype =
      SUBTYPE_UNKNOWN;
#else
  static constexpr ConnectionType kDefaultInitialConnectionType =
      CONNECTION_NONE;
  static constexpr ConnectionSubtype kDefaultInitialConnectionSubtype =
      SUBTYPE_NONE;
#endif

  NetworkChangeNotifier(const NetworkChangeNotifier&) = delete;
  NetworkChangeNotifier& operator=(const NetworkChangeNotifier&) = delete;
  virtual ~NetworkChangeNotifier();

  // Returns the factory or nullptr if it is not set.
  static NetworkChangeNotifierFactory* GetFactory();

  // Replaces the default class factory instance of NetworkChangeNotifier class.
  // The method will take over the ownership of |factory| object.
  static void SetFactory(NetworkChangeNotifierFactory* factory);

  // Creates the process-wide, platform-specific NetworkChangeNotifier if it
  // hasn't been created. The caller owns the returned pointer.  You may call
  // this on any thread. If the process-wide NetworkChangeNotifier already
  // exists, this call will return a nullptr. Otherwise, it will guaranteed
  // to return a valid instance. You may also avoid creating this entirely
  // (in which case nothing will be monitored), but if you do create it, you
  // must do so before any other threads try to access the API below, and it
  // must outlive all other threads which might try to use it.
  static std::unique_ptr<NetworkChangeNotifier> CreateIfNeeded(
      ConnectionType initial_type = kDefaultInitialConnectionType,
      ConnectionSubtype initial_subtype = kDefaultInitialConnectionSubtype);

  // Returns the most likely cost attribute for the default network connection.
  // The value does not indicate with absolute certainty if using the connection
  // will or will not incur a monetary cost to the user. It is a best guess
  // based on Operating System information and network interface type.
  static ConnectionCost GetConnectionCost();

  // Returns the connection type.
  // A return value of |CONNECTION_NONE| is a pretty strong indicator that the
  // user won't be able to connect to remote sites. However, another return
  // value doesn't imply that the user will be able to connect to remote sites;
  // even if some link is up, it is uncertain whether a particular connection
  // attempt to a particular remote site will be successful.
  // The returned value only describes the first-hop connection, for example if
  // the device is connected via WiFi to a 4G hotspot, the returned value will
  // be CONNECTION_WIFI, not CONNECTION_4G.
  static ConnectionType GetConnectionType();

  // Returns the device's current default active network connection's subtype.
  // The returned value only describes the first-hop connection, for example if
  // the device is connected via WiFi to a 4G hotspot, the returned value will
  // reflect WiFi, not 4G. This method may return SUBTYPE_UNKNOWN even if the
  // connection type is known.
  static ConnectionSubtype GetConnectionSubtype();

  // Sets |max_bandwidth_mbps| to a theoretical upper limit on download
  // bandwidth, potentially based on underlying connection type, signal
  // strength, or some other signal. If the network subtype is unknown then
  // |max_bandwidth_mbps| is set to +Infinity and if there is no network
  // connection then it is set to 0.0. The circumstances in which a more
  // specific value is given are: when an Android device is connected to a
  // cellular or WiFi network, and when a ChromeOS device is connected to a
  // cellular network. See the NetInfo spec for the mapping of
  // specific subtypes to bandwidth values: http://w3c.github.io/netinfo/.
  // |connection_type| is set to the current active default network's connection
  // type.
  static void GetMaxBandwidthAndConnectionType(double* max_bandwidth_mbps,
                                               ConnectionType* connection_type);

  // Returns a theoretical upper limit (in Mbps) on download bandwidth given a
  // connection subtype. The mapping of connection type to maximum bandwidth is
  // provided in the NetInfo spec: http://w3c.github.io/netinfo/.
  static double GetMaxBandwidthMbpsForConnectionSubtype(
      ConnectionSubtype subtype);

  // Returns true if the platform supports use of APIs based on
  // handles::NetworkHandles. Public methods that use handles::NetworkHandles
  // are GetNetworkConnectionType(), GetNetworkConnectionType(),
  // GetDefaultNetwork(), AddNetworkObserver(), RemoveNetworkObserver(), and all
  // public NetworkObserver methods.
  static bool AreNetworkHandlesSupported();

  // Sets |network_list| to a list of all networks that are currently connected.
  // Only implemented for Android (Lollipop and newer), leaves |network_list|
  // empty when unimplemented. Requires handles::NetworkHandles support, see
  // AreNetworkHandlesSupported().
  static void GetConnectedNetworks(NetworkList* network_list);

  // Returns the type of connection |network| uses. Note that this may vary
  // slightly over time (e.g. CONNECTION_2G to CONNECTION_3G). If |network|
  // is no longer connected, it will return CONNECTION_UNKNOWN.
  // Only implemented for Android (Lollipop and newer), returns
  // CONNECTION_UNKNOWN when unimplemented. Requires handles::NetworkHandles
  // support, see AreNetworkHandlesSupported().
  static ConnectionType GetNetworkConnectionType(
      handles::NetworkHandle network);

  // Returns the device's current default network connection. This is the
  // network used for newly created socket communication for sockets that are
  // not explicitly bound to a particular network (e.g. via
  // DatagramClientSocket.BindToNetwork). Returns |kInvalidNetworkHandle| if
  // there is no default connected network.
  // Only implemented for Android (Lollipop and newer), returns
  // |kInvalidNetworkHandle| when unimplemented.
  // Requires handles::NetworkHandles support, see AreNetworkHandlesSupported().
  static handles::NetworkHandle GetDefaultNetwork();

  // Get the underlying SystemDnsConfigChangeNotifier, or null if there is none.
  // Only intended for code building HostResolverManagers. Other code intending
  // to watch for DNS config changes should use
  // NetworkChangeNotifier::AddDNSObserver to receive notifications about both
  // underlying system config changes and effective changes added on top by
  // Chrome net code.
  static SystemDnsConfigChangeNotifier* GetSystemDnsConfigNotifier();

  // Returns true if the device default network is currently in a high power
  // state.
  // Only implemented for Android (Lollipop and newer). Always returns true
  // when unimplemented, required in order to avoid indefinitely batching
  // packets sent lazily.
  static bool IsDefaultNetworkActive();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Returns the AddressTrackerLinux if present.
  static AddressMapOwnerLinux* GetAddressMapOwner();
#endif

#if BUILDFLAG(IS_FUCHSIA)
  // Returns the NetworkInterfaceCache if present.
  static const internal::NetworkInterfaceCache* GetNetworkInterfaceCache();
#endif

  // Convenience method to determine if the user is offline.
  // Returns true if there is currently no internet connection.
  //
  // A return value of |true| is a pretty strong indicator that the user
  // won't be able to connect to remote sites. However, a return value of
  // |false| is inconclusive; even if some link is up, it is uncertain
  // whether a particular connection attempt to a particular remote site
  // will be successfully.
  static bool IsOffline();

  // Returns true if |type| is a cellular connection.
  // Returns false if |type| is CONNECTION_UNKNOWN, and thus, depending on the
  // implementation of GetConnectionType(), it is possible that
  // IsConnectionCellular(GetConnectionType()) returns false even if the
  // current connection is cellular.
  static bool IsConnectionCellular(ConnectionType type);

  // Gets the current connection type based on |interfaces|. Returns
  // CONNECTION_NONE if there are no interfaces, CONNECTION_UNKNOWN if two
  // interfaces have different connection types or the connection type of all
  // interfaces if they have the same interface type.
  static ConnectionType ConnectionTypeFromInterfaceList(
      const NetworkInterfaceList& interfaces);

  // Like CreateIfNeeded(), but for use in tests. The mock object doesn't
  // monitor any events, it merely rebroadcasts notifications when requested.
  static std::unique_ptr<NetworkChangeNotifier> CreateMockIfNeeded();

  // Registers |observer| to receive notifications of network changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.  This is safe to call if Create() has not
  // been called (as long as it doesn't race the Create() call on another
  // thread), in which case it will add the observers to the static observer
  // list and be notified once the network change notifier is created.

  // DEPRECATED. IPAddressObserver is deprecated. Please use
  // NetworkChangeObserver instead. crbug.com/754695.
  static void AddIPAddressObserver(IPAddressObserver* observer);
  // DEPRECATED. ConnectionTypeObserver is deprecated. Please use
  // NetworkChangeObserver instead. crbug.com/754695.
  static void AddConnectionTypeObserver(ConnectionTypeObserver* observer);
  static void AddDNSObserver(DNSObserver* observer);
  static void AddNetworkChangeObserver(NetworkChangeObserver* observer);
  static void AddMaxBandwidthObserver(MaxBandwidthObserver* observer);
  static void AddNetworkObserver(NetworkObserver* observer);
  static void AddConnectionCostObserver(ConnectionCostObserver* observer);
  static void AddDefaultNetworkActiveObserver(
      DefaultNetworkActiveObserver* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.  Like AddObserver(),
  // this is safe to call if Create() has not been called (as long as it doesn't
  // race the Create() call on another thread), in which case it will simply do
  // nothing.  Technically, it's also safe to call after the notifier object has
  // been destroyed, if the call doesn't race the notifier's destruction, but
  // there's no reason to use the API in this risky way, so don't do it.

  // DEPRECATED. IPAddressObserver is deprecated. Please use
  // NetworkChangeObserver instead. crbug.com/754695.
  static void RemoveIPAddressObserver(IPAddressObserver* observer);
  // DEPRECATED. ConnectionTypeObserver is deprecated. Please use
  // NetworkChangeObserver instead. crbug.com/754695.
  static void RemoveConnectionTypeObserver(ConnectionTypeObserver* observer);
  static void RemoveDNSObserver(DNSObserver* observer);
  static void RemoveNetworkChangeObserver(NetworkChangeObserver* observer);
  static void RemoveMaxBandwidthObserver(MaxBandwidthObserver* observer);
  static void RemoveNetworkObserver(NetworkObserver* observer);
  static void RemoveConnectionCostObserver(ConnectionCostObserver* observer);
  static void RemoveDefaultNetworkActiveObserver(
      DefaultNetworkActiveObserver* observer);

  // Called to signify a non-system DNS config change.
  static void TriggerNonSystemDnsChange();

  // Allows unit tests to trigger notifications.
  static void NotifyObserversOfIPAddressChangeForTests();
  static void NotifyObserversOfConnectionTypeChangeForTests(
      ConnectionType type);
  static void NotifyObserversOfDNSChangeForTests();
  static void NotifyObserversOfNetworkChangeForTests(ConnectionType type);
  static void NotifyObserversOfMaxBandwidthChangeForTests(
      double max_bandwidth_mbps,
      ConnectionType type);
  static void NotifyObserversOfConnectionCostChangeForTests(
      ConnectionCost cost);
  static void NotifyObserversOfDefaultNetworkActiveForTests();

  // Enables or disables notifications from the host. After setting to true, be
  // sure to pump the RunLoop until idle to finish any preexisting
  // notifications. To use this, it must must be called before a
  // NetworkChangeNotifier is created.
  static void SetTestNotificationsOnly(bool test_only);

  // Returns true if `test_notifications_only_` is set to true.
  static bool IsTestNotificationsOnly() { return test_notifications_only_; }

  // Returns a string equivalent to |type|.
  static base::cstring_view ConnectionTypeToString(ConnectionType type);

  // Allows a second NetworkChangeNotifier to be created for unit testing, so
  // the test suite can create a MockNetworkChangeNotifier, but platform
  // specific NetworkChangeNotifiers can also be created for testing.  To use,
  // create an DisableForTest object, and then create the new
  // NetworkChangeNotifier object.  The NetworkChangeNotifier must be
  // destroyed before the DisableForTest object, as its destruction will restore
  // the original NetworkChangeNotifier.
  class NET_EXPORT DisableForTest {
   public:
    DisableForTest();
    ~DisableForTest();

   private:
    // The original NetworkChangeNotifier to be restored on destruction.
    raw_ptr<NetworkChangeNotifier> network_change_notifier_;
  };

 protected:
  // Types of network changes specified to
  // NotifyObserversOfSpecificNetworkChange.
  enum class NetworkChangeType {
    kConnected,
    kDisconnected,
    kSoonToDisconnect,
    kMadeDefault
  };

  // NetworkChanged signal is calculated from the IPAddressChanged and
  // ConnectionTypeChanged signals. Delay parameters control how long to delay
  // producing NetworkChanged signal after particular input signals so as to
  // combine duplicates.  In other words if an input signal is repeated within
  // the corresponding delay period, only one resulting NetworkChange signal is
  // produced.
  struct NET_EXPORT NetworkChangeCalculatorParams {
    NetworkChangeCalculatorParams();
    // Controls delay after OnIPAddressChanged when transitioning from an
    // offline state.
    base::TimeDelta ip_address_offline_delay_;
    // Controls delay after OnIPAddressChanged when transitioning from an
    // online state.
    base::TimeDelta ip_address_online_delay_;
    // Controls delay after OnConnectionTypeChanged when transitioning from an
    // offline state.
    base::TimeDelta connection_type_offline_delay_;
    // Controls delay after OnConnectionTypeChanged when transitioning from an
    // online state.
    base::TimeDelta connection_type_online_delay_;
  };

  // If |system_dns_config_notifier| is null (the default), a shared singleton
  // will be used that will be leaked on shutdown. If
  // |omit_observers_in_constructor_for_testing| is true, internal observers
  // aren't added during construction - this is used to skip registering
  // observers from MockNetworkChangeNotifier, and allow its construction when
  // SingleThreadTaskRunner::CurrentDefaultHandle isn't set.
  explicit NetworkChangeNotifier(
      const NetworkChangeCalculatorParams& params =
          NetworkChangeCalculatorParams(),
      SystemDnsConfigChangeNotifier* system_dns_config_notifier = nullptr,
      bool omit_observers_in_constructor_for_testing = false);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Returns the AddressMapOwnerLinux if present.
  virtual AddressMapOwnerLinux* GetAddressMapOwnerInternal();
#endif

#if BUILDFLAG(IS_FUCHSIA)
  virtual const internal::NetworkInterfaceCache*
  GetNetworkInterfaceCacheInternal() const;
#endif

  // These are the actual implementations of the static queryable APIs.
  // See the description of the corresponding functions named without "Current".
  // Implementations must be thread-safe. Implementations must also be
  // cheap as they are called often.
  virtual ConnectionCost GetCurrentConnectionCost();
  virtual ConnectionType GetCurrentConnectionType() const = 0;
  virtual ConnectionSubtype GetCurrentConnectionSubtype() const;
  virtual void GetCurrentMaxBandwidthAndConnectionType(
      double* max_bandwidth_mbps,
      ConnectionType* connection_type) const;
  virtual bool AreNetworkHandlesCurrentlySupported() const;
  virtual void GetCurrentConnectedNetworks(NetworkList* network_list) const;
  virtual ConnectionType GetCurrentNetworkConnectionType(
      handles::NetworkHandle network) const;
  virtual handles::NetworkHandle GetCurrentDefaultNetwork() const;
  virtual SystemDnsConfigChangeNotifier* GetCurrentSystemDnsConfigNotifier();

  virtual bool IsDefaultNetworkActiveInternal();

  // Broadcasts a notification to all registered observers.  Note that this
  // happens asynchronously, even for observers on the current thread, even in
  // tests.
  static void NotifyObserversOfIPAddressChange();
  static void NotifyObserversOfConnectionTypeChange();
  static void NotifyObserversOfDNSChange();
  static void NotifyObserversOfNetworkChange(ConnectionType type);
  static void NotifyObserversOfMaxBandwidthChange(double max_bandwidth_mbps,
                                                  ConnectionType type);
  static void NotifyObserversOfSpecificNetworkChange(
      NetworkChangeType type,
      handles::NetworkHandle network);
  static void NotifyObserversOfConnectionCostChange();
  static void NotifyObserversOfDefaultNetworkActive();

  // Infer connection type from |GetNetworkList|. If all network interfaces
  // have the same type, return it, otherwise return CONNECTION_UNKNOWN.
  static ConnectionType ConnectionTypeFromInterfaces();

  // Unregisters and clears |system_dns_config_notifier_|. Useful if a subclass
  // owns the notifier and is destroying it before |this|'s destructor is called
  void StopSystemDnsConfigNotifier();

  // Clears the global NetworkChangeNotifier pointer.  This should be called
  // as early as possible in the destructor to prevent races.
  void ClearGlobalPointer();

  // Listening for notifications of this type is expensive as they happen
  // frequently. For this reason, we report {de}registration to the
  // implementation class, so that it can decide to only listen to this type of
  // Android system notifications when there are observers interested.
  virtual void DefaultNetworkActiveObserverAdded() {}
  virtual void DefaultNetworkActiveObserverRemoved() {}

 private:
  friend class HostResolverManagerDnsTest;
  friend class NetworkChangeNotifierAndroidTest;
  friend class NetworkChangeNotifierLinuxTest;
  friend class NetworkChangeNotifierWinTest;

  class NetworkChangeCalculator;
  class SystemDnsConfigObserver;
  class ObserverList;

  static ObserverList& GetObserverList();

  void NotifyObserversOfIPAddressChangeImpl();
  void NotifyObserversOfConnectionTypeChangeImpl(ConnectionType type);
  void NotifyObserversOfDNSChangeImpl();
  void NotifyObserversOfNetworkChangeImpl(ConnectionType type);
  void NotifyObserversOfMaxBandwidthChangeImpl(double max_bandwidth_mbps,
                                               ConnectionType type);
  void NotifyObserversOfSpecificNetworkChangeImpl(
      NetworkChangeType type,
      handles::NetworkHandle network);
  void NotifyObserversOfConnectionCostChangeImpl(ConnectionCost cost);
  void NotifyObserversOfDefaultNetworkActiveImpl();

  raw_ptr<SystemDnsConfigChangeNotifier> system_dns_config_notifier_;
  std::unique_ptr<SystemDnsConfigObserver> system_dns_config_observer_;

  // Computes NetworkChange signal from IPAddress and ConnectionType signals.
  std::unique_ptr<NetworkChangeCalculator> network_change_calculator_;

  // Set true to disable non-test notifications (to prevent flakes in tests).
  static bool test_notifications_only_;

  // Indicates if this instance cleared g_network_change_notifier_ yet.
  bool cleared_global_pointer_ = false;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
