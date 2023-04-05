// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_change_notifier_factory_android.h"

#include "base/memory/ptr_util.h"
#include "net/android/network_change_notifier_android.h"

namespace net {

NetworkChangeNotifierFactoryAndroid::NetworkChangeNotifierFactoryAndroid() =
    default;

NetworkChangeNotifierFactoryAndroid::~NetworkChangeNotifierFactoryAndroid() =
    default;

std::unique_ptr<NetworkChangeNotifier>
NetworkChangeNotifierFactoryAndroid::CreateInstanceWithInitialTypes(
    NetworkChangeNotifier::ConnectionType /*initial_type*/,
    NetworkChangeNotifier::ConnectionSubtype /*initial_subtype*/) {
  return base::WrapUnique(new NetworkChangeNotifierAndroid(&delegate_));
}

}  // namespace net
