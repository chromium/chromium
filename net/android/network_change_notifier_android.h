// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"

namespace base {
struct OnTaskRunnerDeleter;
}  // namespace base

namespace net {

class NetworkChangeNotifierAndroidTest;
class NetworkChangeNotifierFactoryAndroid;

// NetworkChangeNotifierAndroid observes network events from the Android
// notification system and forwards them to observers.
//
// The implementation is complicated by the differing lifetime and thread
// affinity requirements of Android notifications and of NetworkChangeNotifier.
//
// High-level overview:
// NetworkChangeNotifier.java - Receives notifications from Android system, and
// notifies native code via JNI (on the main application thread).
// NetworkChangeNotifierDelegateAndroid ('Delegate') - Listens for notifications
//   sent via JNI on the main application thread, and forwards them to observers
//   on their threads. Owned by Factory, lives exclusively on main application
//   thread.
// NetworkChangeNotifierFactoryAndroid ('Factory') - Creates the Delegate on the
//   main thread to receive JNI events, and vends Notifiers. Lives exclusively
//   on main application thread, and outlives all other classes.
// NetworkChangeNotifierAndroid ('Notifier') - Receives event notifications from
//   the Delegate. Processes and forwards these events to the
//   NetworkChangeNotifier observers on their threads. May live on any thread
//   and be called by any thread.
//
// For more details, see the implementation file.
//
// Note: Alongside of NetworkChangeNotifier.java there is
// NetworkActiveNotifier.java, which handles notifications for when the system
// default network goes in to a high power state. These are handled separately
// since listening to them is expensive (they are fired often) and currently
// only bidi streams connection status check uses them.
class NET_EXPORT_PRIVATE NetworkChangeNotifierAndroid
    : public NetworkChangeNotifier,
      public NetworkChangeNotifierDelegateAndroid::Observer {
 public:
  NetworkChangeNotifierAndroid(const NetworkChangeNotifierAndroid&) = delete;
  NetworkChangeNotifierAndroid& operator=(const NetworkChangeNotifierAndroid&) =
      delete;
  ~NetworkChangeNotifierAndroid() override;

  // NetworkChangeNotifier:
  ConnectionType GetCurrentConnectionType() const override;
  ConnectionCost GetCurrentConnectionCost() override;
  // Requires ACCESS_WIFI_STATE permission in order to provide precise WiFi link
  // speed.
  void GetCurrentMaxBandwidthAndConnectionType(
      double* max_bandwidth_mbps,
      ConnectionType* connection_type) const override;
  bool AreNetworkHandlesCurrentlySupported() const override;
  void GetCurrentConnectedNetworks(NetworkList* network_list) const override;
  ConnectionType GetCurrentNetworkConnectionType(
      handles::NetworkHandle network) const override;
  NetworkChangeNotifier::ConnectionSubtype GetCurrentConnectionSubtype()
      const override;
  handles::NetworkHandle GetCurrentDefaultNetwork() const override;
  bool IsDefaultNetworkActiveInternal() override;

  // NetworkChangeNotifierDelegateAndroid::Observer:
  void OnConnectionTypeChanged() override;
  void OnConnectionCostChanged() override;
  void OnMaxBandwidthChanged(double max_bandwidth_mbps,
                             ConnectionType type) override;
  void OnNetworkConnected(handles::NetworkHandle network) override;
  void OnNetworkSoonToDisconnect(handles::NetworkHandle network) override;
  void OnNetworkDisconnected(handles::NetworkHandle network) override;
  void OnNetworkMadeDefault(handles::NetworkHandle network) override;
  void OnDefaultNetworkActive() override;

  // Promote GetMaxBandwidthMbpsForConnectionSubtype to public for the Android
  // delegate class.
  using NetworkChangeNotifier::GetMaxBandwidthMbpsForConnectionSubtype;

  static NetworkChangeCalculatorParams NetworkChangeCalculatorParamsAndroid();

  void DefaultNetworkActiveObserverAdded() override;
  void DefaultNetworkActiveObserverRemoved() override;

 private:
  friend class NetworkChangeNotifierAndroidTest;
  friend class NetworkChangeNotifierFactoryAndroid;

  class BlockingThreadObjects;

  // Enable handles::NetworkHandles support for tests.
  void ForceNetworkHandlesSupportedForTesting();

  explicit NetworkChangeNotifierAndroid(
      NetworkChangeNotifierDelegateAndroid* delegate);

  const raw_ptr<NetworkChangeNotifierDelegateAndroid> delegate_;
  // A collection of objects that must live on blocking sequences. These objects
  // listen for notifications and relay the notifications to the registered
  // observers without posting back to the thread the object was created on.
  // Also used for DnsConfigService which also must live on blocking sequences.
  std::unique_ptr<BlockingThreadObjects, base::OnTaskRunnerDeleter>
      blocking_thread_objects_;
  bool force_network_handles_supported_for_testing_ = false;
};

}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
