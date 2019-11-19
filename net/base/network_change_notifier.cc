// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier.h"

#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier_factory.h"
#include "net/base/network_interfaces.h"
#include "net/base/url_util.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/system_dns_config_change_notifier.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "net/base/network_change_notifier_win.h"
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "net/base/network_change_notifier_linux.h"
#elif defined(OS_MACOSX)
#include "net/base/network_change_notifier_mac.h"
#elif defined(OS_CHROMEOS) || defined(OS_ANDROID)
#include "net/base/network_change_notifier_posix.h"
#elif defined(OS_FUCHSIA)
#include "net/base/network_change_notifier_fuchsia.h"
#endif

namespace net {

namespace {

// The process-wide singleton notifier.
NetworkChangeNotifier* g_network_change_notifier = nullptr;

// Class factory singleton.
NetworkChangeNotifierFactory* g_network_change_notifier_factory = nullptr;

// Lock to protect |g_network_change_notifier| during creation time. Since
// creation of the process-wide instance can happen on any thread, this lock is
// used to guarantee only one instance is created. Once the global instance is
// created, the owner is responsible for destroying it on the same thread. All
// the other calls to the NetworkChangeNotifier do not require this lock as
// the global instance is only destroyed when the process is getting killed.
base::Lock& NetworkChangeNotifierCreationLock() {
  static base::NoDestructor<base::Lock> instance;
  return *instance;
}

class MockNetworkChangeNotifier : public NetworkChangeNotifier {
 public:
  MockNetworkChangeNotifier(
      std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier)
      : NetworkChangeNotifier(NetworkChangeCalculatorParams(),
                              dns_config_notifier.get()),
        dns_config_notifier_(std::move(dns_config_notifier)) {}

  ~MockNetworkChangeNotifier() override { StopSystemDnsConfigNotifier(); }

  ConnectionType GetCurrentConnectionType() const override {
    return CONNECTION_UNKNOWN;
  }

 private:
  std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier_;
};

}  // namespace

// static
bool NetworkChangeNotifier::test_notifications_only_ = false;
// static
const NetworkChangeNotifier::NetworkHandle
    NetworkChangeNotifier::kInvalidNetworkHandle = -1;

NetworkChangeNotifier::NetworkChangeCalculatorParams::
    NetworkChangeCalculatorParams() = default;

// Calculates NetworkChange signal from IPAddress and ConnectionType signals.
class NetworkChangeNotifier::NetworkChangeCalculator
    : public ConnectionTypeObserver,
      public IPAddressObserver {
 public:
  explicit NetworkChangeCalculator(const NetworkChangeCalculatorParams& params)
      : params_(params),
        have_announced_(false),
        last_announced_connection_type_(CONNECTION_NONE),
        pending_connection_type_(CONNECTION_NONE) {}

  void Init() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(g_network_change_notifier);
    AddConnectionTypeObserver(this);
    AddIPAddressObserver(this);
  }

  ~NetworkChangeCalculator() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    RemoveConnectionTypeObserver(this);
    RemoveIPAddressObserver(this);
  }

  // NetworkChangeNotifier::IPAddressObserver implementation.
  void OnIPAddressChanged() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    pending_connection_type_ = GetConnectionType();
    base::TimeDelta delay = last_announced_connection_type_ == CONNECTION_NONE
        ? params_.ip_address_offline_delay_ : params_.ip_address_online_delay_;
    // Cancels any previous timer.
    timer_.Start(FROM_HERE, delay, this, &NetworkChangeCalculator::Notify);
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(ConnectionType type) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    pending_connection_type_ = type;
    base::TimeDelta delay = last_announced_connection_type_ == CONNECTION_NONE
        ? params_.connection_type_offline_delay_
        : params_.connection_type_online_delay_;
    // Cancels any previous timer.
    timer_.Start(FROM_HERE, delay, this, &NetworkChangeCalculator::Notify);
  }

 private:
  void Notify() {
    DCHECK(thread_checker_.CalledOnValidThread());
    // Don't bother signaling about dead connections.
    if (have_announced_ &&
        (last_announced_connection_type_ == CONNECTION_NONE) &&
        (pending_connection_type_ == CONNECTION_NONE)) {
      return;
    }
    have_announced_ = true;
    last_announced_connection_type_ = pending_connection_type_;
    // Immediately before sending out an online signal, send out an offline
    // signal to perform any destructive actions before constructive actions.
    if (pending_connection_type_ != CONNECTION_NONE)
      NetworkChangeNotifier::NotifyObserversOfNetworkChange(CONNECTION_NONE);
    NetworkChangeNotifier::NotifyObserversOfNetworkChange(
        pending_connection_type_);
  }

  const NetworkChangeCalculatorParams params_;

  // Indicates if NotifyObserversOfNetworkChange has been called yet.
  bool have_announced_;
  // Last value passed to NotifyObserversOfNetworkChange.
  ConnectionType last_announced_connection_type_;
  // Value to pass to NotifyObserversOfNetworkChange when Notify is called.
  ConnectionType pending_connection_type_;
  // Used to delay notifications so duplicates can be combined.
  base::OneShotTimer timer_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeCalculator);
};

class NetworkChangeNotifier::SystemDnsConfigObserver
    : public SystemDnsConfigChangeNotifier::Observer {
 public:
  virtual ~SystemDnsConfigObserver() = default;

  void OnSystemDnsConfigChanged(base::Optional<DnsConfig> config) override {
    NotifyObserversOfDNSChange();
  }
};

void NetworkChangeNotifier::ClearGlobalPointer() {
  if (!cleared_global_pointer_) {
    cleared_global_pointer_ = true;
    DCHECK_EQ(this, g_network_change_notifier);
    g_network_change_notifier = nullptr;
  }
}

NetworkChangeNotifier::~NetworkChangeNotifier() {
  network_change_calculator_.reset();
  ClearGlobalPointer();
  StopSystemDnsConfigNotifier();
}

// static
NetworkChangeNotifierFactory* NetworkChangeNotifier::GetFactory() {
  return g_network_change_notifier_factory;
}

// static
void NetworkChangeNotifier::SetFactory(
    NetworkChangeNotifierFactory* factory) {
  CHECK(!g_network_change_notifier_factory);
  g_network_change_notifier_factory = factory;
}

// static
std::unique_ptr<NetworkChangeNotifier> NetworkChangeNotifier::CreateIfNeeded(
    NetworkChangeNotifier::ConnectionType initial_type,
    NetworkChangeNotifier::ConnectionSubtype initial_subtype) {
  base::AutoLock auto_lock(NetworkChangeNotifierCreationLock());
  if (g_network_change_notifier)
    return nullptr;

  if (g_network_change_notifier_factory)
    return g_network_change_notifier_factory->CreateInstance();

#if defined(OS_WIN)
  std::unique_ptr<NetworkChangeNotifierWin> network_change_notifier =
      std::make_unique<NetworkChangeNotifierWin>();
  network_change_notifier->WatchForAddressChange();
  return network_change_notifier;
#elif defined(OS_ANDROID)
  // Fallback to use NetworkChangeNotifierPosix if NetworkChangeNotifierFactory
  // is not set. Currently used for tests and when running network
  // service in a separate process.
  return std::make_unique<NetworkChangeNotifierPosix>(initial_type,
                                                      initial_subtype);
#elif defined(OS_CHROMEOS)
  return std::make_unique<NetworkChangeNotifierPosix>(initial_type,
                                                      initial_subtype);
#elif defined(OS_LINUX)
  return std::make_unique<NetworkChangeNotifierLinux>(
      std::unordered_set<std::string>());
#elif defined(OS_MACOSX)
  return std::make_unique<NetworkChangeNotifierMac>();
#elif defined(OS_FUCHSIA)
  return std::make_unique<NetworkChangeNotifierFuchsia>(
      0 /* required_features */);
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

// static
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifier::GetConnectionType() {
  return g_network_change_notifier ?
      g_network_change_notifier->GetCurrentConnectionType() :
      CONNECTION_UNKNOWN;
}

// static
NetworkChangeNotifier::ConnectionSubtype
NetworkChangeNotifier::GetConnectionSubtype() {
  return g_network_change_notifier
             ? g_network_change_notifier->GetCurrentConnectionSubtype()
             : SUBTYPE_UNKNOWN;
}

// static
void NetworkChangeNotifier::GetMaxBandwidthAndConnectionType(
    double* max_bandwidth_mbps,
    ConnectionType* connection_type) {
  if (!g_network_change_notifier) {
    *connection_type = CONNECTION_UNKNOWN;
    *max_bandwidth_mbps =
        GetMaxBandwidthMbpsForConnectionSubtype(SUBTYPE_UNKNOWN);
    return;
  }

  g_network_change_notifier->GetCurrentMaxBandwidthAndConnectionType(
      max_bandwidth_mbps, connection_type);
}

// static
double NetworkChangeNotifier::GetMaxBandwidthMbpsForConnectionSubtype(
    ConnectionSubtype subtype) {
  switch (subtype) {
    case SUBTYPE_GSM:
      return 0.01;
    case SUBTYPE_IDEN:
      return 0.064;
    case SUBTYPE_CDMA:
      return 0.115;
    case SUBTYPE_1XRTT:
      return 0.153;
    case SUBTYPE_GPRS:
      return 0.237;
    case SUBTYPE_EDGE:
      return 0.384;
    case SUBTYPE_UMTS:
      return 2.0;
    case SUBTYPE_EVDO_REV_0:
      return 2.46;
    case SUBTYPE_EVDO_REV_A:
      return 3.1;
    case SUBTYPE_HSPA:
      return 3.6;
    case SUBTYPE_EVDO_REV_B:
      return 14.7;
    case SUBTYPE_HSDPA:
      return 14.3;
    case SUBTYPE_HSUPA:
      return 14.4;
    case SUBTYPE_EHRPD:
      return 21.0;
    case SUBTYPE_HSPAP:
      return 42.0;
    case SUBTYPE_LTE:
      return 100.0;
    case SUBTYPE_LTE_ADVANCED:
      return 100.0;
    case SUBTYPE_BLUETOOTH_1_2:
      return 1.0;
    case SUBTYPE_BLUETOOTH_2_1:
      return 3.0;
    case SUBTYPE_BLUETOOTH_3_0:
      return 24.0;
    case SUBTYPE_BLUETOOTH_4_0:
      return 1.0;
    case SUBTYPE_ETHERNET:
      return 10.0;
    case SUBTYPE_FAST_ETHERNET:
      return 100.0;
    case SUBTYPE_GIGABIT_ETHERNET:
      return 1000.0;
    case SUBTYPE_10_GIGABIT_ETHERNET:
      return 10000.0;
    case SUBTYPE_WIFI_B:
      return 11.0;
    case SUBTYPE_WIFI_G:
      return 54.0;
    case SUBTYPE_WIFI_N:
      return 600.0;
    case SUBTYPE_WIFI_AC:
      return 1300.0;
    case SUBTYPE_WIFI_AD:
      return 7000.0;
    case SUBTYPE_UNKNOWN:
      return std::numeric_limits<double>::infinity();
    case SUBTYPE_NONE:
      return 0.0;
    case SUBTYPE_OTHER:
      return std::numeric_limits<double>::infinity();
  }
  NOTREACHED();
  return std::numeric_limits<double>::infinity();
}

// static
bool NetworkChangeNotifier::AreNetworkHandlesSupported() {
  if (g_network_change_notifier) {
    return g_network_change_notifier->AreNetworkHandlesCurrentlySupported();
  }
  return false;
}

// static
void NetworkChangeNotifier::GetConnectedNetworks(NetworkList* network_list) {
  DCHECK(AreNetworkHandlesSupported());
  if (g_network_change_notifier) {
    g_network_change_notifier->GetCurrentConnectedNetworks(network_list);
  } else {
    network_list->clear();
  }
}

// static
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifier::GetNetworkConnectionType(NetworkHandle network) {
  DCHECK(AreNetworkHandlesSupported());
  return g_network_change_notifier
             ? g_network_change_notifier->GetCurrentNetworkConnectionType(
                   network)
             : CONNECTION_UNKNOWN;
}

// static
NetworkChangeNotifier::NetworkHandle
NetworkChangeNotifier::GetDefaultNetwork() {
  DCHECK(AreNetworkHandlesSupported());
  return g_network_change_notifier
             ? g_network_change_notifier->GetCurrentDefaultNetwork()
             : kInvalidNetworkHandle;
}

// static
SystemDnsConfigChangeNotifier*
NetworkChangeNotifier::GetSystemDnsConfigNotifier() {
  if (g_network_change_notifier)
    return g_network_change_notifier->GetCurrentSystemDnsConfigNotifier();
  return nullptr;
}

// static
const char* NetworkChangeNotifier::ConnectionTypeToString(
    ConnectionType type) {
  static const char* const kConnectionTypeNames[] = {
    "CONNECTION_UNKNOWN",
    "CONNECTION_ETHERNET",
    "CONNECTION_WIFI",
    "CONNECTION_2G",
    "CONNECTION_3G",
    "CONNECTION_4G",
    "CONNECTION_NONE",
    "CONNECTION_BLUETOOTH"
  };
  static_assert(base::size(kConnectionTypeNames) ==
                    NetworkChangeNotifier::CONNECTION_LAST + 1,
                "ConnectionType name count should match");
  if (type < CONNECTION_UNKNOWN || type > CONNECTION_LAST) {
    NOTREACHED();
    return "CONNECTION_INVALID";
  }
  return kConnectionTypeNames[type];
}

#if defined(OS_LINUX)
// static
const internal::AddressTrackerLinux*
NetworkChangeNotifier::GetAddressTracker() {
  return g_network_change_notifier ?
        g_network_change_notifier->GetAddressTrackerInternal() : NULL;
}
#endif

// static
bool NetworkChangeNotifier::IsOffline() {
  return GetConnectionType() == CONNECTION_NONE;
}

// static
bool NetworkChangeNotifier::IsConnectionCellular(ConnectionType type) {
  bool is_cellular = false;
  switch (type) {
    case CONNECTION_2G:
    case CONNECTION_3G:
    case CONNECTION_4G:
      is_cellular =  true;
      break;
    case CONNECTION_UNKNOWN:
    case CONNECTION_ETHERNET:
    case CONNECTION_WIFI:
    case CONNECTION_NONE:
    case CONNECTION_BLUETOOTH:
      is_cellular = false;
      break;
  }
  return is_cellular;
}

// static
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifier::ConnectionTypeFromInterfaces() {
  NetworkInterfaceList interfaces;
  if (!GetNetworkList(&interfaces, EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES))
    return CONNECTION_UNKNOWN;
  return ConnectionTypeFromInterfaceList(interfaces);
}

// static
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifier::ConnectionTypeFromInterfaceList(
    const NetworkInterfaceList& interfaces) {
  bool first = true;
  ConnectionType result = CONNECTION_NONE;
  for (size_t i = 0; i < interfaces.size(); ++i) {
#if defined(OS_WIN)
    if (interfaces[i].friendly_name == "Teredo Tunneling Pseudo-Interface")
      continue;
#endif
#if defined(OS_MACOSX)
    // Ignore link-local addresses as they aren't globally routable.
    // Mac assigns these to disconnected interfaces like tunnel interfaces
    // ("utun"), airdrop interfaces ("awdl"), and ethernet ports ("en").
    if (interfaces[i].address.IsLinkLocal())
      continue;
#endif

    // Remove VMware network interfaces as they're internal and should not be
    // used to determine the network connection type.
    if (base::ToLowerASCII(interfaces[i].friendly_name).find("vmnet") !=
        std::string::npos) {
      continue;
    }
    if (first) {
      first = false;
      result = interfaces[i].type;
    } else if (result != interfaces[i].type) {
      return CONNECTION_UNKNOWN;
    }
  }
  return result;
}

// static
std::unique_ptr<NetworkChangeNotifier>
NetworkChangeNotifier::CreateMockIfNeeded() {
  base::AutoLock auto_lock(NetworkChangeNotifierCreationLock());
  if (g_network_change_notifier)
    return nullptr;

  // Use an empty noop SystemDnsConfigChangeNotifier to disable actual system
  // DNS configuration notifications.
  return std::make_unique<MockNetworkChangeNotifier>(
      std::make_unique<SystemDnsConfigChangeNotifier>(
          nullptr /* task_runner */, nullptr /* dns_config_service */));
}

NetworkChangeNotifier::IPAddressObserver::IPAddressObserver() = default;
NetworkChangeNotifier::IPAddressObserver::~IPAddressObserver() = default;

NetworkChangeNotifier::ConnectionTypeObserver::ConnectionTypeObserver() =
    default;
NetworkChangeNotifier::ConnectionTypeObserver::~ConnectionTypeObserver() =
    default;

NetworkChangeNotifier::DNSObserver::DNSObserver() = default;
NetworkChangeNotifier::DNSObserver::~DNSObserver() = default;

NetworkChangeNotifier::NetworkChangeObserver::NetworkChangeObserver() = default;
NetworkChangeNotifier::NetworkChangeObserver::~NetworkChangeObserver() =
    default;

NetworkChangeNotifier::MaxBandwidthObserver::MaxBandwidthObserver() = default;
NetworkChangeNotifier::MaxBandwidthObserver::~MaxBandwidthObserver() = default;

NetworkChangeNotifier::NetworkObserver::NetworkObserver() = default;
NetworkChangeNotifier::NetworkObserver::~NetworkObserver() = default;

void NetworkChangeNotifier::AddIPAddressObserver(IPAddressObserver* observer) {
  if (g_network_change_notifier) {
    observer->observer_list_ =
        g_network_change_notifier->ip_address_observer_list_;
    observer->observer_list_->AddObserver(observer);
  }
}

void NetworkChangeNotifier::AddConnectionTypeObserver(
    ConnectionTypeObserver* observer) {
  if (g_network_change_notifier) {
    observer->observer_list_ =
        g_network_change_notifier->connection_type_observer_list_;
    observer->observer_list_->AddObserver(observer);
  }
}

void NetworkChangeNotifier::AddDNSObserver(DNSObserver* observer) {
  if (g_network_change_notifier) {
    observer->observer_list_ =
        g_network_change_notifier->resolver_state_observer_list_;
    observer->observer_list_->AddObserver(observer);
  }
}

void NetworkChangeNotifier::AddNetworkChangeObserver(
    NetworkChangeObserver* observer) {
  if (g_network_change_notifier) {
    observer->observer_list_ =
        g_network_change_notifier->network_change_observer_list_;
    observer->observer_list_->AddObserver(observer);
  }
}

void NetworkChangeNotifier::AddMaxBandwidthObserver(
    MaxBandwidthObserver* observer) {
  if (g_network_change_notifier) {
    observer->observer_list_ =
        g_network_change_notifier->max_bandwidth_observer_list_;
    observer->observer_list_->AddObserver(observer);
  }
}

void NetworkChangeNotifier::AddNetworkObserver(NetworkObserver* observer) {
  DCHECK(AreNetworkHandlesSupported());
  if (g_network_change_notifier) {
    observer->observer_list_ =
        g_network_change_notifier->network_observer_list_;
    observer->observer_list_->AddObserver(observer);
  }
}

void NetworkChangeNotifier::RemoveIPAddressObserver(
    IPAddressObserver* observer) {
  if (observer->observer_list_) {
    observer->observer_list_->RemoveObserver(observer);
    observer->observer_list_.reset();
  }
}

void NetworkChangeNotifier::RemoveConnectionTypeObserver(
    ConnectionTypeObserver* observer) {
  if (observer->observer_list_) {
    observer->observer_list_->RemoveObserver(observer);
    observer->observer_list_.reset();
  }
}

void NetworkChangeNotifier::RemoveDNSObserver(DNSObserver* observer) {
  if (observer->observer_list_) {
    observer->observer_list_->RemoveObserver(observer);
    observer->observer_list_.reset();
  }
}

void NetworkChangeNotifier::RemoveNetworkChangeObserver(
    NetworkChangeObserver* observer) {
  if (observer->observer_list_) {
    observer->observer_list_->RemoveObserver(observer);
    observer->observer_list_.reset();
  }
}

void NetworkChangeNotifier::RemoveMaxBandwidthObserver(
    MaxBandwidthObserver* observer) {
  if (observer->observer_list_) {
    observer->observer_list_->RemoveObserver(observer);
    observer->observer_list_.reset();
  }
}

void NetworkChangeNotifier::RemoveNetworkObserver(NetworkObserver* observer) {
  if (observer->observer_list_) {
    observer->observer_list_->RemoveObserver(observer);
    observer->observer_list_.reset();
  }
}

void NetworkChangeNotifier::TriggerNonSystemDnsChange() {
  NetworkChangeNotifier::NotifyObserversOfDNSChange();
}

// static
void NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests() {
  if (g_network_change_notifier)
    g_network_change_notifier->NotifyObserversOfIPAddressChangeImpl();
}

// static
void NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
    ConnectionType type) {
  if (g_network_change_notifier)
    g_network_change_notifier->NotifyObserversOfConnectionTypeChangeImpl(type);
}

// static
void NetworkChangeNotifier::NotifyObserversOfDNSChangeForTests() {
  if (g_network_change_notifier)
    g_network_change_notifier->NotifyObserversOfDNSChangeImpl();
}

// static
void NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
    ConnectionType type) {
  if (g_network_change_notifier)
    g_network_change_notifier->NotifyObserversOfNetworkChangeImpl(type);
}

// static
void NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChangeForTests(
    double max_bandwidth_mbps,
    ConnectionType type) {
  if (g_network_change_notifier) {
    g_network_change_notifier->NotifyObserversOfMaxBandwidthChangeImpl(
        max_bandwidth_mbps, type);
  }
}

// static
void NetworkChangeNotifier::SetTestNotificationsOnly(bool test_only) {
  DCHECK(!g_network_change_notifier);
  NetworkChangeNotifier::test_notifications_only_ = test_only;
}

NetworkChangeNotifier::NetworkChangeNotifier(
    const NetworkChangeCalculatorParams& params
    /*= NetworkChangeCalculatorParams()*/,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier /*= nullptr */)
    : ip_address_observer_list_(
          new base::ObserverListThreadSafe<IPAddressObserver>(
              base::ObserverListPolicy::EXISTING_ONLY)),
      connection_type_observer_list_(
          new base::ObserverListThreadSafe<ConnectionTypeObserver>(
              base::ObserverListPolicy::EXISTING_ONLY)),
      resolver_state_observer_list_(
          new base::ObserverListThreadSafe<DNSObserver>(
              base::ObserverListPolicy::EXISTING_ONLY)),
      network_change_observer_list_(
          new base::ObserverListThreadSafe<NetworkChangeObserver>(
              base::ObserverListPolicy::EXISTING_ONLY)),
      max_bandwidth_observer_list_(
          new base::ObserverListThreadSafe<MaxBandwidthObserver>(
              base::ObserverListPolicy::EXISTING_ONLY)),
      network_observer_list_(new base::ObserverListThreadSafe<NetworkObserver>(
          base::ObserverListPolicy::EXISTING_ONLY)),
      system_dns_config_notifier_(system_dns_config_notifier),
      system_dns_config_observer_(std::make_unique<SystemDnsConfigObserver>()),
      network_change_calculator_(new NetworkChangeCalculator(params)) {
  if (!system_dns_config_notifier_) {
    static base::NoDestructor<SystemDnsConfigChangeNotifier> singleton{};
    system_dns_config_notifier_ = singleton.get();
  }

  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = this;
  network_change_calculator_->Init();

  system_dns_config_notifier_->AddObserver(system_dns_config_observer_.get());
}

#if defined(OS_LINUX)
const internal::AddressTrackerLinux*
NetworkChangeNotifier::GetAddressTrackerInternal() const {
  return NULL;
}
#endif

NetworkChangeNotifier::ConnectionSubtype
NetworkChangeNotifier::GetCurrentConnectionSubtype() const {
  return SUBTYPE_UNKNOWN;
}

void NetworkChangeNotifier::GetCurrentMaxBandwidthAndConnectionType(
    double* max_bandwidth_mbps,
    ConnectionType* connection_type) const {
  // This default implementation conforms to the NetInfo V3 specification but
  // should be overridden to provide specific bandwidth data based on the
  // platform.
  *connection_type = GetCurrentConnectionType();
  *max_bandwidth_mbps =
      *connection_type == CONNECTION_NONE
          ? GetMaxBandwidthMbpsForConnectionSubtype(SUBTYPE_NONE)
          : GetMaxBandwidthMbpsForConnectionSubtype(SUBTYPE_UNKNOWN);
}

bool NetworkChangeNotifier::AreNetworkHandlesCurrentlySupported() const {
  return false;
}

void NetworkChangeNotifier::GetCurrentConnectedNetworks(
    NetworkList* network_list) const {
  network_list->clear();
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifier::GetCurrentNetworkConnectionType(
    NetworkHandle network) const {
  return CONNECTION_UNKNOWN;
}

NetworkChangeNotifier::NetworkHandle
NetworkChangeNotifier::GetCurrentDefaultNetwork() const {
  return kInvalidNetworkHandle;
}

SystemDnsConfigChangeNotifier*
NetworkChangeNotifier::GetCurrentSystemDnsConfigNotifier() {
  DCHECK(system_dns_config_notifier_);
  return system_dns_config_notifier_;
}

// static
void NetworkChangeNotifier::NotifyObserversOfIPAddressChange() {
  if (g_network_change_notifier &&
      !NetworkChangeNotifier::test_notifications_only_) {
    g_network_change_notifier->NotifyObserversOfIPAddressChangeImpl();
  }
}

// static
void NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange() {
  if (g_network_change_notifier &&
      !NetworkChangeNotifier::test_notifications_only_) {
    g_network_change_notifier->NotifyObserversOfConnectionTypeChangeImpl(
        GetConnectionType());
  }
}

// static
void NetworkChangeNotifier::NotifyObserversOfNetworkChange(
    ConnectionType type) {
  if (g_network_change_notifier &&
      !NetworkChangeNotifier::test_notifications_only_) {
    g_network_change_notifier->NotifyObserversOfNetworkChangeImpl(type);
  }
}

// static
void NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChange(
    double max_bandwidth_mbps,
    ConnectionType type) {
  if (g_network_change_notifier &&
      !NetworkChangeNotifier::test_notifications_only_) {
    g_network_change_notifier->NotifyObserversOfMaxBandwidthChangeImpl(
        max_bandwidth_mbps, type);
  }
}

// static
void NetworkChangeNotifier::NotifyObserversOfDNSChange() {
  if (g_network_change_notifier &&
      !NetworkChangeNotifier::test_notifications_only_) {
    g_network_change_notifier->NotifyObserversOfDNSChangeImpl();
  }
}

// static
void NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
    NetworkChangeType type,
    NetworkHandle network) {
  if (g_network_change_notifier &&
      !NetworkChangeNotifier::test_notifications_only_) {
    g_network_change_notifier->NotifyObserversOfSpecificNetworkChangeImpl(
        type, network);
  }
}

void NetworkChangeNotifier::StopSystemDnsConfigNotifier() {
  if (!system_dns_config_notifier_)
    return;

  system_dns_config_notifier_->RemoveObserver(
      system_dns_config_observer_.get());
  system_dns_config_observer_ = nullptr;
  system_dns_config_notifier_ = nullptr;
}

void NetworkChangeNotifier::NotifyObserversOfIPAddressChangeImpl() {
  ip_address_observer_list_->Notify(FROM_HERE,
                                    &IPAddressObserver::OnIPAddressChanged);
}

void NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeImpl(
    ConnectionType type) {
  connection_type_observer_list_->Notify(
      FROM_HERE, &ConnectionTypeObserver::OnConnectionTypeChanged, type);
}

void NetworkChangeNotifier::NotifyObserversOfNetworkChangeImpl(
    ConnectionType type) {
  network_change_observer_list_->Notify(
      FROM_HERE, &NetworkChangeObserver::OnNetworkChanged, type);
}

void NetworkChangeNotifier::NotifyObserversOfDNSChangeImpl() {
  resolver_state_observer_list_->Notify(FROM_HERE, &DNSObserver::OnDNSChanged);
}

void NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChangeImpl(
    double max_bandwidth_mbps,
    ConnectionType type) {
  max_bandwidth_observer_list_->Notify(
      FROM_HERE, &MaxBandwidthObserver::OnMaxBandwidthChanged,
      max_bandwidth_mbps, type);
}

void NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChangeImpl(
    NetworkChangeType type,
    NetworkHandle network) {
  switch (type) {
    case CONNECTED:
      network_observer_list_->Notify(
          FROM_HERE, &NetworkObserver::OnNetworkConnected, network);
      break;
    case DISCONNECTED:
      network_observer_list_->Notify(
          FROM_HERE, &NetworkObserver::OnNetworkDisconnected, network);
      break;
    case SOON_TO_DISCONNECT:
      network_observer_list_->Notify(
          FROM_HERE, &NetworkObserver::OnNetworkSoonToDisconnect, network);
      break;
    case MADE_DEFAULT:
      network_observer_list_->Notify(
          FROM_HERE, &NetworkObserver::OnNetworkMadeDefault, network);
      break;
  }
}

NetworkChangeNotifier::DisableForTest::DisableForTest()
    : network_change_notifier_(g_network_change_notifier) {
  DCHECK(g_network_change_notifier);
  g_network_change_notifier = nullptr;
}

NetworkChangeNotifier::DisableForTest::~DisableForTest() {
  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = network_change_notifier_;
}

}  // namespace net
