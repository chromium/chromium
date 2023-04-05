// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_ANDROID_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_ANDROID_H_

#include <memory>

#include "base/compiler_specific.h"
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier_factory.h"

namespace net {

class NetworkChangeNotifier;
class NetworkChangeNotifierDelegateAndroid;

// NetworkChangeNotifierFactory creates Android-specific specialization of
// NetworkChangeNotifier. See network_change_notifier_android.h for more
// details.
class NET_EXPORT NetworkChangeNotifierFactoryAndroid :
    public NetworkChangeNotifierFactory {
 public:
  // Must be called on the JNI thread.
  NetworkChangeNotifierFactoryAndroid();

  NetworkChangeNotifierFactoryAndroid(
      const NetworkChangeNotifierFactoryAndroid&) = delete;
  NetworkChangeNotifierFactoryAndroid& operator=(
      const NetworkChangeNotifierFactoryAndroid&) = delete;

  // Must be called on the JNI thread.
  ~NetworkChangeNotifierFactoryAndroid() override;

  // NetworkChangeNotifierFactory:
  std::unique_ptr<NetworkChangeNotifier> CreateInstanceWithInitialTypes(
      NetworkChangeNotifier::ConnectionType /*initial_type*/,
      NetworkChangeNotifier::ConnectionSubtype /*initial_subtype*/) override;

 private:
  // Delegate passed to the instances created by this class.
  NetworkChangeNotifierDelegateAndroid delegate_;
};

}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_ANDROID_H_
