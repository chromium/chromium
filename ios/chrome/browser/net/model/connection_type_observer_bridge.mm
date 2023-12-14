// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/model/connection_type_observer_bridge.h"

#import "base/check.h"

ConnectionTypeObserverBridge::ConnectionTypeObserverBridge(
    id<CRConnectionTypeObserverBridge> delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

ConnectionTypeObserverBridge::~ConnectionTypeObserverBridge() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void ConnectionTypeObserverBridge::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  [delegate_ connectionTypeChanged:type];
}
