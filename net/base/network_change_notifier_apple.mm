// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_apple.h"

#include <netinet/in.h>
#include <resolv.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_policy.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/network_interfaces_getifaddrs.h"
#include "net/dns/dns_config_service.h"
#include "net/log/net_log.h"

#if BUILDFLAG(IS_IOS)
#import <CoreTelephony/CTTelephonyNetworkInfo.h>
#endif

namespace net {
namespace {
// The maximum number of seconds to wait for the connection type to be
// determined.
const double kMaxWaitForConnectionTypeInSeconds = 2.0;

#if BUILDFLAG(IS_MAC)
std::string GetPrimaryInterfaceName(SCDynamicStoreRef store,
                                    CFStringRef entity) {
  base::apple::ScopedCFTypeRef<CFStringRef> netkey(
      SCDynamicStoreKeyCreateNetworkGlobalEntity(
          nullptr, kSCDynamicStoreDomainState, entity));
  base::apple::ScopedCFTypeRef<CFPropertyListRef> netdict_value(
      SCDynamicStoreCopyValue(store, netkey.get()));
  CFDictionaryRef netdict =
      base::apple::CFCast<CFDictionaryRef>(netdict_value.get());
  if (!netdict) {
    return "";
  }
  CFStringRef primary_if_name_ref =
      base::apple::GetValueFromDictionary<CFStringRef>(
          netdict, kSCDynamicStorePropNetPrimaryInterface);
  if (!primary_if_name_ref) {
    return "";
  }
  return base::SysCFStringRefToUTF8(primary_if_name_ref);
}

std::string GetIPv4PrimaryInterfaceName(SCDynamicStoreRef store) {
  return GetPrimaryInterfaceName(store, kSCEntNetIPv4);
}

std::string GetIPv6PrimaryInterfaceName(SCDynamicStoreRef store) {
  return GetPrimaryInterfaceName(store, kSCEntNetIPv6);
}

std::optional<net::NetworkInterfaceList>
GetNetworkInterfaceListForNetworkChangeCheck(
    base::RepeatingCallback<bool(net::NetworkInterfaceList*, int)>
        get_network_list_callback,
    const std::string& ipv6_primary_interface_name) {
  net::NetworkInterfaceList interfaces;
  if (!get_network_list_callback.Run(
          &interfaces, net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    return std::nullopt;
  }
  std::erase_if(interfaces, [&ipv6_primary_interface_name](
                                const net::NetworkInterface& interface) {
    return interface.address.IsIPv6() &&
           !interface.address.IsPubliclyRoutable() &&
           (interface.name != ipv6_primary_interface_name);
  });
  return interfaces;
}

base::Value::Dict GetNetworkInterfaceValueDict(
    const NetworkInterface& interface) {
  base::Value::Dict dict;
  dict.Set("name", interface.name);
  dict.Set("friendly_name", interface.friendly_name);
  dict.Set("interface_index", static_cast<int>(interface.interface_index));
  dict.Set("type", static_cast<int>(interface.type));
  dict.Set("address", interface.address.ToString());
  dict.Set("prefix_length", static_cast<int>(interface.prefix_length));
  dict.Set("ip_address_attributes", interface.ip_address_attributes);
  if (interface.mac_address) {
    dict.Set("mac_address", base::HexEncode(*interface.mac_address));
  }
  return dict;
}

base::Value::List GetNetworkInterfacesValueList(
    const NetworkInterfaceList& interfaces) {
  base::Value::List list;
  for (const NetworkInterface& interface : interfaces) {
    list.Append(GetNetworkInterfaceValueDict(interface));
  }
  return list;
}

base::Value::Dict NetLogOsConfigChangedParams(
    const std::string& result,
    bool net_ipv4_key_found,
    bool net_ipv6_key_found,
    bool net_interface_key_found,
    bool reduce_ip_address_change_notification,
    const std::string& old_ipv4_primary_interface_name,
    const std::string& old_ipv6_primary_interface_name,
    const std::string& new_ipv4_primary_interface_name,
    const std::string& new_ipv6_primary_interface_name,
    const std::optional<NetworkInterfaceList>& old_interfaces,
    const std::optional<NetworkInterfaceList>& new_interfaces) {
  base::Value::Dict dict;
  dict.Set("result", result);
  dict.Set("net_ipv4_key", net_ipv4_key_found);
  dict.Set("net_ipv6_key", net_ipv6_key_found);
  dict.Set("net_interface_key", net_interface_key_found);
  dict.Set("reduce_notification", reduce_ip_address_change_notification);
  dict.Set("old_ipv4_interface", old_ipv4_primary_interface_name);
  dict.Set("old_ipv6_interface", old_ipv6_primary_interface_name);
  dict.Set("new_ipv4_interface", new_ipv4_primary_interface_name);
  dict.Set("new_ipv6_interface", new_ipv6_primary_interface_name);
  if (old_interfaces) {
    dict.Set("old_interfaces", GetNetworkInterfacesValueList(*old_interfaces));
  }
  if (new_interfaces) {
    dict.Set("new_interfaces", GetNetworkInterfacesValueList(*new_interfaces));
  }
  return dict;
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace

static bool CalculateReachability(SCNetworkConnectionFlags flags) {
  bool reachable = flags & kSCNetworkFlagsReachable;
  bool connection_required = flags & kSCNetworkFlagsConnectionRequired;
  return reachable && !connection_required;
}

NetworkChangeNotifierApple::NetworkChangeNotifierApple()
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsMac()),
      initial_connection_type_cv_(&connection_type_lock_),
      forwarder_(this),
#if BUILDFLAG(IS_MAC)
      reduce_ip_address_change_notification_(base::FeatureList::IsEnabled(
          features::kReduceIPAddressChangeNotification)),
      get_network_list_callback_(base::BindRepeating(&GetNetworkList)),
      get_ipv4_primary_interface_name_callback_(
          base::BindRepeating(&GetIPv4PrimaryInterfaceName)),
      get_ipv6_primary_interface_name_callback_(
          base::BindRepeating(&GetIPv6PrimaryInterfaceName)),
#endif  // BUILDFLAG(IS_MAC)
      net_log_(net::NetLogWithSource::Make(
          net::NetLog::Get(),
          net::NetLogSourceType::NETWORK_CHANGE_NOTIFIER)) {
  // Must be initialized after the rest of this object, as it may call back into
  // SetInitialConnectionType().
  config_watcher_ = std::make_unique<NetworkConfigWatcherApple>(&forwarder_);
}

NetworkChangeNotifierApple::~NetworkChangeNotifierApple() {
  ClearGlobalPointer();
  // Delete the ConfigWatcher to join the notifier thread, ensuring that
  // StartReachabilityNotifications() has an opportunity to run to completion.
  config_watcher_.reset();

  // Now that StartReachabilityNotifications() has either run to completion or
  // never run at all, unschedule reachability_ if it was previously scheduled.
  if (reachability_.get() && run_loop_.get()) {
    SCNetworkReachabilityUnscheduleFromRunLoop(
        reachability_.get(), run_loop_.get(), kCFRunLoopCommonModes);
  }
}

// static
NetworkChangeNotifier::NetworkChangeCalculatorParams
NetworkChangeNotifierApple::NetworkChangeCalculatorParamsMac() {
  NetworkChangeCalculatorParams params;
  // Delay values arrived at by simple experimentation and adjusted so as to
  // produce a single signal when switching between network connections.
  params.ip_address_offline_delay_ = base::Milliseconds(500);
  params.ip_address_online_delay_ = base::Milliseconds(500);
  params.connection_type_offline_delay_ = base::Milliseconds(1000);
  params.connection_type_online_delay_ = base::Milliseconds(500);
  return params;
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierApple::GetCurrentConnectionType() const {
  // https://crbug.com/125097
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::AutoLock lock(connection_type_lock_);

  if (connection_type_initialized_)
    return connection_type_;

  // Wait up to a limited amount of time for the connection type to be
  // determined, to avoid blocking the main thread indefinitely. Since
  // ConditionVariables are susceptible to spurious wake-ups, each call to
  // TimedWait can spuriously return even though the connection type hasn't been
  // initialized and the timeout hasn't been reached; so TimedWait must be
  // called repeatedly until either the timeout is reached or the connection
  // type has been determined.
  base::TimeDelta remaining_time =
      base::Seconds(kMaxWaitForConnectionTypeInSeconds);
  base::TimeTicks end_time = base::TimeTicks::Now() + remaining_time;
  while (remaining_time.is_positive()) {
    initial_connection_type_cv_.TimedWait(remaining_time);
    if (connection_type_initialized_)
      return connection_type_;

    remaining_time = end_time - base::TimeTicks::Now();
  }

  return CONNECTION_UNKNOWN;
}

void NetworkChangeNotifierApple::Forwarder::Init() {
  net_config_watcher_->SetInitialConnectionType();
}

// static
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierApple::CalculateConnectionType(
    SCNetworkConnectionFlags flags) {
  bool reachable = CalculateReachability(flags);
  if (!reachable)
    return CONNECTION_NONE;

#if BUILDFLAG(IS_IOS)
  if (!(flags & kSCNetworkReachabilityFlagsIsWWAN)) {
    return CONNECTION_WIFI;
  }
  if (@available(iOS 12, *)) {
    CTTelephonyNetworkInfo* info = [[CTTelephonyNetworkInfo alloc] init];
    NSDictionary<NSString*, NSString*>*
        service_current_radio_access_technology =
            info.serviceCurrentRadioAccessTechnology;
    NSSet<NSString*>* technologies_2g = [NSSet
        setWithObjects:CTRadioAccessTechnologyGPRS, CTRadioAccessTechnologyEdge,
                       CTRadioAccessTechnologyCDMA1x, nil];
    NSSet<NSString*>* technologies_3g =
        [NSSet setWithObjects:CTRadioAccessTechnologyWCDMA,
                              CTRadioAccessTechnologyHSDPA,
                              CTRadioAccessTechnologyHSUPA,
                              CTRadioAccessTechnologyCDMAEVDORev0,
                              CTRadioAccessTechnologyCDMAEVDORevA,
                              CTRadioAccessTechnologyCDMAEVDORevB,
                              CTRadioAccessTechnologyeHRPD, nil];
    NSSet<NSString*>* technologies_4g =
        [NSSet setWithObjects:CTRadioAccessTechnologyLTE, nil];
    // TODO: Use constants from CoreTelephony once Cronet builds with XCode 12.1
    NSSet<NSString*>* technologies_5g =
        [NSSet setWithObjects:@"CTRadioAccessTechnologyNRNSA",
                              @"CTRadioAccessTechnologyNR", nil];
    int best_network = 0;
    for (NSString* service in service_current_radio_access_technology) {
      if (!service_current_radio_access_technology[service]) {
        continue;
      }
      int current_network = 0;

      NSString* network_type = service_current_radio_access_technology[service];

      if ([technologies_2g containsObject:network_type]) {
        current_network = 2;
      } else if ([technologies_3g containsObject:network_type]) {
        current_network = 3;
      } else if ([technologies_4g containsObject:network_type]) {
        current_network = 4;
      } else if ([technologies_5g containsObject:network_type]) {
        current_network = 5;
      } else {
        // New technology?
        NOTREACHED_IN_MIGRATION()
            << "Unknown network technology: " << network_type;
        return CONNECTION_UNKNOWN;
      }
      if (current_network > best_network) {
        // iOS is supposed to use the best network available.
        best_network = current_network;
      }
    }
    switch (best_network) {
      case 2:
        return CONNECTION_2G;
      case 3:
        return CONNECTION_3G;
      case 4:
        return CONNECTION_4G;
      case 5:
        return CONNECTION_5G;
      default:
        // Default to CONNECTION_3G to not change existing behavior.
        return CONNECTION_3G;
    }
  } else {
    return CONNECTION_3G;
  }

#else
  return ConnectionTypeFromInterfaces();
#endif
}

void NetworkChangeNotifierApple::Forwarder::StartReachabilityNotifications() {
  net_config_watcher_->StartReachabilityNotifications();
}

void NetworkChangeNotifierApple::Forwarder::SetDynamicStoreNotificationKeys(
    base::apple::ScopedCFTypeRef<SCDynamicStoreRef> store) {
  net_config_watcher_->SetDynamicStoreNotificationKeys(std::move(store));
}

void NetworkChangeNotifierApple::Forwarder::OnNetworkConfigChange(
    CFArrayRef changed_keys) {
  net_config_watcher_->OnNetworkConfigChange(changed_keys);
}

void NetworkChangeNotifierApple::Forwarder::CleanUpOnNotifierThread() {
  net_config_watcher_->CleanUpOnNotifierThread();
}

void NetworkChangeNotifierApple::SetInitialConnectionType() {
  // Called on notifier thread.

  // Try to reach 0.0.0.0. This is the approach taken by Firefox:
  //
  // http://mxr.mozilla.org/mozilla2.0/source/netwerk/system/mac/nsNetworkLinkService.mm
  //
  // From my (adamk) testing on Snow Leopard, 0.0.0.0
  // seems to be reachable if any network connection is available.
  struct sockaddr_in addr = {0};
  addr.sin_len = sizeof(addr);
  addr.sin_family = AF_INET;
  reachability_.reset(SCNetworkReachabilityCreateWithAddress(
      kCFAllocatorDefault, reinterpret_cast<struct sockaddr*>(&addr)));

  SCNetworkConnectionFlags flags;
  ConnectionType connection_type = CONNECTION_UNKNOWN;
  if (SCNetworkReachabilityGetFlags(reachability_.get(), &flags)) {
    connection_type = CalculateConnectionType(flags);
  } else {
    LOG(ERROR) << "Could not get initial network connection type,"
               << "assuming online.";
  }
  {
    base::AutoLock lock(connection_type_lock_);
    connection_type_ = connection_type;
    connection_type_initialized_ = true;
    initial_connection_type_cv_.Broadcast();
  }
}

void NetworkChangeNotifierApple::StartReachabilityNotifications() {
  // Called on notifier thread.
  run_loop_.reset(CFRunLoopGetCurrent(), base::scoped_policy::RETAIN);

  DCHECK(reachability_);
  SCNetworkReachabilityContext reachability_context = {
      0,        // version
      this,     // user data
      nullptr,  // retain
      nullptr,  // release
      nullptr   // description
  };
  if (!SCNetworkReachabilitySetCallback(
          reachability_.get(), &NetworkChangeNotifierApple::ReachabilityCallback,
          &reachability_context)) {
    LOG(DFATAL) << "Could not set network reachability callback";
    reachability_.reset();
  } else if (!SCNetworkReachabilityScheduleWithRunLoop(
                 reachability_.get(), run_loop_.get(), kCFRunLoopCommonModes)) {
    LOG(DFATAL) << "Could not schedule network reachability on run loop";
    reachability_.reset();
  }
}

void NetworkChangeNotifierApple::SetDynamicStoreNotificationKeys(
    base::apple::ScopedCFTypeRef<SCDynamicStoreRef> store) {
#if BUILDFLAG(IS_IOS)
  // SCDynamicStore API does not exist on iOS.
  NOTREACHED_IN_MIGRATION();
#elif BUILDFLAG(IS_MAC)
  NSArray* notification_keys = @[
    base::apple::CFToNSOwnershipCast(SCDynamicStoreKeyCreateNetworkGlobalEntity(
        nullptr, kSCDynamicStoreDomainState, kSCEntNetInterface)),
    base::apple::CFToNSOwnershipCast(SCDynamicStoreKeyCreateNetworkGlobalEntity(
        nullptr, kSCDynamicStoreDomainState, kSCEntNetIPv4)),
    base::apple::CFToNSOwnershipCast(SCDynamicStoreKeyCreateNetworkGlobalEntity(
        nullptr, kSCDynamicStoreDomainState, kSCEntNetIPv6)),
  ];

  // Set the notification keys.  This starts us receiving notifications.
  bool ret = SCDynamicStoreSetNotificationKeys(
      store.get(), base::apple::NSToCFPtrCast(notification_keys),
      /*patterns=*/nullptr);
  // TODO(willchan): Figure out a proper way to handle this rather than crash.
  CHECK(ret);

  if (reduce_ip_address_change_notification_) {
    store_ = std::move(store);
    ipv4_primary_interface_name_ =
        get_ipv4_primary_interface_name_callback_.Run(store_.get());
    ipv6_primary_interface_name_ =
        get_ipv6_primary_interface_name_callback_.Run(store_.get());
    interfaces_for_network_change_check_ =
        GetNetworkInterfaceListForNetworkChangeCheck(
            get_network_list_callback_, ipv6_primary_interface_name_);
  }
  if (initialized_callback_for_test_) {
    std::move(initialized_callback_for_test_).Run();
  }
#endif  // BUILDFLAG(IS_IOS) /  BUILDFLAG(IS_MAC)
}

void NetworkChangeNotifierApple::OnNetworkConfigChange(CFArrayRef changed_keys) {
#if BUILDFLAG(IS_IOS)
  // SCDynamicStore API does not exist on iOS.
  NOTREACHED_IN_MIGRATION();
#elif BUILDFLAG(IS_MAC)
  DCHECK_EQ(run_loop_.get(), CFRunLoopGetCurrent());

  bool net_ipv4_key_found = false;
  bool net_ipv6_key_found = false;
  bool net_interface_key_found = false;
  for (CFIndex i = 0; i < CFArrayGetCount(changed_keys); ++i) {
    CFStringRef key =
        static_cast<CFStringRef>(CFArrayGetValueAtIndex(changed_keys, i));
    if (CFStringHasSuffix(key, kSCEntNetIPv4)) {
      net_ipv4_key_found = true;
    }
    if (CFStringHasSuffix(key, kSCEntNetIPv6)) {
      net_ipv6_key_found = true;
    }
    if (net_ipv4_key_found || net_ipv6_key_found) {
      break;
    }
    if (CFStringHasSuffix(key, kSCEntNetInterface)) {
      net_interface_key_found = true;
      // TODO(willchan): Does not appear to be working.  Look into this.
      // Perhaps this isn't needed anyway.
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
  if (!(net_ipv4_key_found || net_ipv6_key_found)) {
    net_log_.AddEvent(net::NetLogEventType::NETWORK_MAC_OS_CONFIG_CHANGED, [&] {
      return NetLogOsConfigChangedParams(
          "DoNotNotify_NoIPAddressChange", net_ipv4_key_found,
          net_ipv6_key_found, net_interface_key_found,
          reduce_ip_address_change_notification_, ipv4_primary_interface_name_,
          ipv6_primary_interface_name_, "", "",
          interfaces_for_network_change_check_, std::nullopt);
    });
    return;
  }
  if (!reduce_ip_address_change_notification_) {
    net_log_.AddEvent(net::NetLogEventType::NETWORK_MAC_OS_CONFIG_CHANGED, [&] {
      return NetLogOsConfigChangedParams(
          "Notify_NoReduce", net_ipv4_key_found, net_ipv6_key_found,
          net_interface_key_found, reduce_ip_address_change_notification_,
          ipv4_primary_interface_name_, ipv6_primary_interface_name_, "", "",
          interfaces_for_network_change_check_, std::nullopt);
    });
    NotifyObserversOfIPAddressChange();
    return;
  }
  // When the ReduceIPAddressChangeNotification feature is enabled, we notifies
  // the IP address change only when:
  //  - The list of network interfaces has changed, excluding local IPv6
  //    addresses of non-primary interfaces.
  //  - or the primary interface name (for IPv4 and IPv6) has changed.
  std::string ipv4_primary_interface_name =
      get_ipv4_primary_interface_name_callback_.Run(store_.get());
  std::string ipv6_primary_interface_name =
      get_ipv6_primary_interface_name_callback_.Run(store_.get());
  std::optional<NetworkInterfaceList> interfaces =
      GetNetworkInterfaceListForNetworkChangeCheck(get_network_list_callback_,
                                                   ipv6_primary_interface_name);
  if (interfaces_for_network_change_check_ && interfaces &&
      interfaces_for_network_change_check_.value() == interfaces.value() &&
      ipv4_primary_interface_name_ == ipv4_primary_interface_name &&
      ipv6_primary_interface_name_ == ipv6_primary_interface_name) {
    net_log_.AddEvent(net::NetLogEventType::NETWORK_MAC_OS_CONFIG_CHANGED, [&] {
      return NetLogOsConfigChangedParams(
          "DoNotNotify_NoChange", net_ipv4_key_found, net_ipv6_key_found,
          net_interface_key_found, reduce_ip_address_change_notification_,
          ipv4_primary_interface_name_, ipv6_primary_interface_name_,
          ipv4_primary_interface_name, ipv6_primary_interface_name,
          interfaces_for_network_change_check_, interfaces);
    });
    return;
  }
  net_log_.AddEvent(net::NetLogEventType::NETWORK_MAC_OS_CONFIG_CHANGED, [&] {
    return NetLogOsConfigChangedParams(
        "Notify_Changed", net_ipv4_key_found, net_ipv6_key_found,
        net_interface_key_found, reduce_ip_address_change_notification_,
        ipv4_primary_interface_name_, ipv6_primary_interface_name_,
        ipv4_primary_interface_name, ipv6_primary_interface_name,
        interfaces_for_network_change_check_, interfaces);
  });
  ipv4_primary_interface_name_ = std::move(ipv4_primary_interface_name);
  ipv6_primary_interface_name_ = std::move(ipv6_primary_interface_name);
  interfaces_for_network_change_check_ = std::move(interfaces);
  NotifyObserversOfIPAddressChange();
#endif  // BUILDFLAG(IS_IOS)
}

void NetworkChangeNotifierApple::CleanUpOnNotifierThread() {
#if BUILDFLAG(IS_MAC)
  store_.reset();
#endif  // BUILDFLAG(IS_MAC)
}

// static
void NetworkChangeNotifierApple::ReachabilityCallback(
    SCNetworkReachabilityRef target,
    SCNetworkConnectionFlags flags,
    void* notifier) {
  NetworkChangeNotifierApple* notifier_apple =
      static_cast<NetworkChangeNotifierApple*>(notifier);

  DCHECK_EQ(notifier_apple->run_loop_.get(), CFRunLoopGetCurrent());

  ConnectionType new_type = CalculateConnectionType(flags);
  ConnectionType old_type;
  {
    base::AutoLock lock(notifier_apple->connection_type_lock_);
    old_type = notifier_apple->connection_type_;
    notifier_apple->connection_type_ = new_type;
  }
  if (old_type != new_type) {
    NotifyObserversOfConnectionTypeChange();
    double max_bandwidth_mbps =
        NetworkChangeNotifier::GetMaxBandwidthMbpsForConnectionSubtype(
            new_type == CONNECTION_NONE ? SUBTYPE_NONE : SUBTYPE_UNKNOWN);
    NotifyObserversOfMaxBandwidthChange(max_bandwidth_mbps, new_type);
  }

#if BUILDFLAG(IS_IOS)
  // On iOS, the SCDynamicStore API does not exist, and we use the reachability
  // API to detect IP address changes instead.
  NotifyObserversOfIPAddressChange();
#endif  // BUILDFLAG(IS_IOS)
}

#if BUILDFLAG(IS_MAC)
void NetworkChangeNotifierApple::SetCallbacksForTest(
    base::OnceClosure initialized_callback,
    base::RepeatingCallback<bool(NetworkInterfaceList*, int)>
        get_network_list_callback,
    base::RepeatingCallback<std::string(SCDynamicStoreRef)>
        get_ipv4_primary_interface_name_callback,
    base::RepeatingCallback<std::string(SCDynamicStoreRef)>
        get_ipv6_primary_interface_name_callback) {
  initialized_callback_for_test_ = std::move(initialized_callback);
  get_network_list_callback_ = std::move(get_network_list_callback);
  get_ipv4_primary_interface_name_callback_ =
      std::move(get_ipv4_primary_interface_name_callback);
  get_ipv6_primary_interface_name_callback_ =
      std::move(get_ipv6_primary_interface_name_callback);
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace net
