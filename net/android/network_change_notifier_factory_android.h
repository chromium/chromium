// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_ANDROID_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_ANDROID_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
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

  // Must be called on the JNI thread.
  ~NetworkChangeNotifierFactoryAndroid() override;

  // NetworkChangeNotifierFactory:
  std::unique_ptr<NetworkChangeNotifier> CreateInstance() override;

 private:
  // Delegate passed to the instances created by this class.
  NetworkChangeNotifierDelegateAndroid delegate_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierFactoryAndroid);
};

}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_ANDROID_H_
