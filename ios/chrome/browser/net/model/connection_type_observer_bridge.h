// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MODEL_CONNECTION_TYPE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_NET_MODEL_CONNECTION_TYPE_OBSERVER_BRIDGE_H_

#include "net/base/network_change_notifier.h"

// Protocol mirroring net::NetworkChangeNotifier::ConnectionTypeObserver.
@protocol CRConnectionTypeObserverBridge
- (void)connectionTypeChanged:(net::NetworkChangeNotifier::ConnectionType)type;
@end

// A C++ bridge class to handle receiving notifications from the C++ class
// that observes the connection type.
class ConnectionTypeObserverBridge
    : public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  explicit ConnectionTypeObserverBridge(
      id<CRConnectionTypeObserverBridge> delegate);

  ConnectionTypeObserverBridge(const ConnectionTypeObserverBridge&) = delete;
  ConnectionTypeObserverBridge& operator=(const ConnectionTypeObserverBridge&) =
      delete;

  ~ConnectionTypeObserverBridge() override;

 private:
  // net::NetworkChangeNotifier::ConnectionTypeObserver implementation:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  __weak id<CRConnectionTypeObserverBridge> delegate_;
};

#endif  // IOS_CHROME_BROWSER_NET_MODEL_CONNECTION_TYPE_OBSERVER_BRIDGE_H_
