// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/model/network_change_observer_bridge.h"

#import "base/check.h"

NetworkChangeObserverBridge::NetworkChangeObserverBridge(
    id<NetworkChangeObserving> observer)
    : observer_(observer) {
  CHECK(observer_);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

NetworkChangeObserverBridge::~NetworkChangeObserverBridge() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void NetworkChangeObserverBridge::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  [observer_ onNetworkChanged:type];
}
