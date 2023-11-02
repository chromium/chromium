// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_mac.h"

#include <netinet/in.h>
#include <resolv.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "net/dns/dns_config_service.h"

#if BUILDFLAG(IS_IOS)
#import <CoreTelephony/CTTelephonyNetworkInfo.h>
#endif

namespace {
// The maximum number of seconds to wait for the connection type to be
// determined.
const double kMaxWaitForConnectionTypeInSeconds = 2.0;
}  // namespace

namespace net {

static bool CalculateReachability(SCNetworkConnectionFlags flags) {
  bool reachable = flags & kSCNetworkFlagsReachable;
  bool connection_required = flags & kSCNetworkFlagsConnectionRequired;
  return reachable && !connection_required;
}

NetworkChangeNotifierMac::NetworkChangeNotifierMac()
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsMac()),
      initial_connection_type_cv_(&connection_type_lock_),
      forwarder_(this) {
  // Must be initialized after the rest of this object, as it may call back into
  // SetInitialConnectionType().
  config_watcher_ = std::make_unique<NetworkConfigWatcherMac>(&forwarder_);
}

NetworkChangeNotifierMac::~NetworkChangeNotifierMac() {
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
NetworkChangeNotifierMac::NetworkChangeCalculatorParamsMac() {
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
NetworkChangeNotifierMac::GetCurrentConnectionType() const {
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

void NetworkChangeNotifierMac::Forwarder::Init() {
  net_config_watcher_->SetInitialConnectionType();
}

// static
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierMac::CalculateConnectionType(
    SCNetworkConnectionFlags flags) {
  bool reachable = CalculateReachability(flags);
  if (!reachable)
    return CONNECTION_NONE;

#if BUILDFLAG(IS_IOS)
  if (!(flags & kSCNetworkReachabilityFlagsIsWWAN)) {
    return CONNECTION_WIFI;
  }
  if (@available(iOS 12, *)) {
    CTTelephonyNetworkInfo* info =
        [[[CTTelephonyNetworkInfo alloc] init] autorelease];
    NSDictionary<NSString*, NSString*>*
        service_current_radio_access_technology =
            [info serviceCurrentRadioAccessTechnology];
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
        NOTREACHED() << "Unknown network technology: " << network_type;
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

void NetworkChangeNotifierMac::Forwarder::StartReachabilityNotifications() {
  net_config_watcher_->StartReachabilityNotifications();
}

void NetworkChangeNotifierMac::Forwarder::SetDynamicStoreNotificationKeys(
    SCDynamicStoreRef store) {
  net_config_watcher_->SetDynamicStoreNotificationKeys(store);
}

void NetworkChangeNotifierMac::Forwarder::OnNetworkConfigChange(
    CFArrayRef changed_keys) {
  net_config_watcher_->OnNetworkConfigChange(changed_keys);
}

void NetworkChangeNotifierMac::SetInitialConnectionType() {
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
  if (SCNetworkReachabilityGetFlags(reachability_, &flags)) {
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

void NetworkChangeNotifierMac::StartReachabilityNotifications() {
  // Called on notifier thread.
  run_loop_.reset(CFRunLoopGetCurrent());
  CFRetain(run_loop_.get());

  DCHECK(reachability_);
  SCNetworkReachabilityContext reachability_context = {
      0,        // version
      this,     // user data
      nullptr,  // retain
      nullptr,  // release
      nullptr   // description
  };
  if (!SCNetworkReachabilitySetCallback(
          reachability_, &NetworkChangeNotifierMac::ReachabilityCallback,
          &reachability_context)) {
    LOG(DFATAL) << "Could not set network reachability callback";
    reachability_.reset();
  } else if (!SCNetworkReachabilityScheduleWithRunLoop(reachability_, run_loop_,
                                                       kCFRunLoopCommonModes)) {
    LOG(DFATAL) << "Could not schedule network reachability on run loop";
    reachability_.reset();
  }
}

void NetworkChangeNotifierMac::SetDynamicStoreNotificationKeys(
    SCDynamicStoreRef store) {
#if BUILDFLAG(IS_IOS)
  // SCDynamicStore API does not exist on iOS.
  NOTREACHED();
#else
  base::ScopedCFTypeRef<CFMutableArrayRef> notification_keys(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  base::ScopedCFTypeRef<CFStringRef> key(
      SCDynamicStoreKeyCreateNetworkGlobalEntity(
          nullptr, kSCDynamicStoreDomainState, kSCEntNetInterface));
  CFArrayAppendValue(notification_keys.get(), key.get());
  key.reset(SCDynamicStoreKeyCreateNetworkGlobalEntity(
      nullptr, kSCDynamicStoreDomainState, kSCEntNetIPv4));
  CFArrayAppendValue(notification_keys.get(), key.get());
  key.reset(SCDynamicStoreKeyCreateNetworkGlobalEntity(
      nullptr, kSCDynamicStoreDomainState, kSCEntNetIPv6));
  CFArrayAppendValue(notification_keys.get(), key.get());

  // Set the notification keys.  This starts us receiving notifications.
  bool ret = SCDynamicStoreSetNotificationKeys(store, notification_keys.get(),
                                               nullptr);
  // TODO(willchan): Figure out a proper way to handle this rather than crash.
  CHECK(ret);
#endif  // BUILDFLAG(IS_IOS)
}

void NetworkChangeNotifierMac::OnNetworkConfigChange(CFArrayRef changed_keys) {
#if BUILDFLAG(IS_IOS)
  // SCDynamicStore API does not exist on iOS.
  NOTREACHED();
#else
  DCHECK_EQ(run_loop_.get(), CFRunLoopGetCurrent());

  for (CFIndex i = 0; i < CFArrayGetCount(changed_keys); ++i) {
    CFStringRef key =
        static_cast<CFStringRef>(CFArrayGetValueAtIndex(changed_keys, i));
    if (CFStringHasSuffix(key, kSCEntNetIPv4) ||
        CFStringHasSuffix(key, kSCEntNetIPv6)) {
      NotifyObserversOfIPAddressChange();
      return;
    }
    if (CFStringHasSuffix(key, kSCEntNetInterface)) {
      // TODO(willchan): Does not appear to be working.  Look into this.
      // Perhaps this isn't needed anyway.
    } else {
      NOTREACHED();
    }
  }
#endif  // BUILDFLAG(IS_IOS)
}

// static
void NetworkChangeNotifierMac::ReachabilityCallback(
    SCNetworkReachabilityRef target,
    SCNetworkConnectionFlags flags,
    void* notifier) {
  NetworkChangeNotifierMac* notifier_mac =
      static_cast<NetworkChangeNotifierMac*>(notifier);

  DCHECK_EQ(notifier_mac->run_loop_.get(), CFRunLoopGetCurrent());

  ConnectionType new_type = CalculateConnectionType(flags);
  ConnectionType old_type;
  {
    base::AutoLock lock(notifier_mac->connection_type_lock_);
    old_type = notifier_mac->connection_type_;
    notifier_mac->connection_type_ = new_type;
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

}  // namespace net
