// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace base {
class SequencedTaskRunner;
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
class NET_EXPORT_PRIVATE NetworkChangeNotifierAndroid
    : public NetworkChangeNotifier,
      public NetworkChangeNotifierDelegateAndroid::Observer {
 public:
  ~NetworkChangeNotifierAndroid() override;

  // NetworkChangeNotifier:
  ConnectionType GetCurrentConnectionType() const override;
  // Requires ACCESS_WIFI_STATE permission in order to provide precise WiFi link
  // speed.
  void GetCurrentMaxBandwidthAndConnectionType(
      double* max_bandwidth_mbps,
      ConnectionType* connection_type) const override;
  bool AreNetworkHandlesCurrentlySupported() const override;
  void GetCurrentConnectedNetworks(NetworkList* network_list) const override;
  ConnectionType GetCurrentNetworkConnectionType(
      NetworkHandle network) const override;
  NetworkChangeNotifier::ConnectionSubtype GetCurrentConnectionSubtype()
      const override;
  NetworkHandle GetCurrentDefaultNetwork() const override;

  // NetworkChangeNotifierDelegateAndroid::Observer:
  void OnConnectionTypeChanged() override;
  void OnMaxBandwidthChanged(double max_bandwidth_mbps,
                             ConnectionType type) override;
  void OnNetworkConnected(NetworkHandle network) override;
  void OnNetworkSoonToDisconnect(NetworkHandle network) override;
  void OnNetworkDisconnected(NetworkHandle network) override;
  void OnNetworkMadeDefault(NetworkHandle network) override;

  // Promote GetMaxBandwidthMbpsForConnectionSubtype to public for the Android
  // delegate class.
  using NetworkChangeNotifier::GetMaxBandwidthMbpsForConnectionSubtype;

  static NetworkChangeCalculatorParams NetworkChangeCalculatorParamsAndroid();

 private:
  friend class NetworkChangeNotifierAndroidTest;
  friend class NetworkChangeNotifierFactoryAndroid;

  class BlockingThreadObjects;

  // Enable NetworkHandles support for tests.
  void ForceNetworkHandlesSupportedForTesting();

  explicit NetworkChangeNotifierAndroid(
      NetworkChangeNotifierDelegateAndroid* delegate);

  NetworkChangeNotifierDelegateAndroid* const delegate_;
  // |blocking_thread_objects_| will live on this runner.
  scoped_refptr<base::SequencedTaskRunner> blocking_thread_runner_;
  // A collection of objects that must live on blocking sequences. These objects
  // listen for notifications and relay the notifications to the registered
  // observers without posting back to the thread the object was created on.
  // Also used for DnsConfigService which also must live on blocking sequences.
  std::unique_ptr<BlockingThreadObjects, base::OnTaskRunnerDeleter>
      blocking_thread_objects_;
  bool force_network_handles_supported_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierAndroid);
};

}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
