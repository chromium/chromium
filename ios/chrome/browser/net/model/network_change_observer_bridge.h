// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MODEL_NETWORK_CHANGE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_NET_MODEL_NETWORK_CHANGE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "net/base/network_change_notifier.h"

// Protocol mirroring `net::NetworkChangeNotifier::NetworkChangeObserver`.
@protocol NetworkChangeObserving <NSObject>

// Called when the network changes, with the new connection `type`. See
// `net::NetworkChangeNotifier::NetworkChangeObserver::OnNetworkChanged`.
- (void)onNetworkChanged:(net::NetworkChangeNotifier::ConnectionType)type;

@end

// Bridge that forwards `net::NetworkChangeNotifier::NetworkChangeObserver`
// notifications to an Objective-C `NetworkChangeObserving` observer. The bridge
// registers itself with `net::NetworkChangeNotifier` on construction and
// unregisters on destruction.
class NetworkChangeObserverBridge
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // Creates a bridge forwarding notifications to `observer`, which is held
  // weakly and must not be nil.
  explicit NetworkChangeObserverBridge(id<NetworkChangeObserving> observer);

  NetworkChangeObserverBridge(const NetworkChangeObserverBridge&) = delete;
  NetworkChangeObserverBridge& operator=(const NetworkChangeObserverBridge&) =
      delete;

  ~NetworkChangeObserverBridge() override;

 private:
  // net::NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  __weak id<NetworkChangeObserving> observer_;
};

#endif  // IOS_CHROME_BROWSER_NET_MODEL_NETWORK_CHANGE_OBSERVER_BRIDGE_H_
