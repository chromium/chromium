// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_NETWORK_CONNECTION_H_
#define NET_QUIC_NETWORK_CONNECTION_H_

#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {

// This class stores information about the current network type and
// provides a textual description of it.
class NET_EXPORT NetworkConnection
    : public NetworkChangeNotifier::IPAddressObserver,
      public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  NetworkConnection();

  NetworkConnection(const NetworkConnection&) = delete;
  NetworkConnection& operator=(const NetworkConnection&) = delete;

  ~NetworkConnection() override;

  // Returns the underlying connection type.
  NetworkChangeNotifier::ConnectionType connection_type() {
    return connection_type_;
  }

  // NetworkChangeNotifier::IPAddressObserver methods:
  void OnIPAddressChanged() override;

  // NetworkChangeNotifier::ConnectionTypeObserver methods:
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) override;

 private:
  // Cache the connection type to avoid calling the potentially expensive
  // NetworkChangeNotifier::GetConnectionType() function.
  NetworkChangeNotifier::ConnectionType connection_type_ =
      NetworkChangeNotifier::CONNECTION_UNKNOWN;
};

}  // namespace net

#endif  // NET_QUIC_NETWORK_CONNECTION_H_
