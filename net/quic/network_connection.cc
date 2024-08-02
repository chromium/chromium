// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/network_connection.h"

#include "base/logging.h"
#include "net/base/network_interfaces.h"

namespace net {

NetworkConnection::NetworkConnection() {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
  OnIPAddressChanged();
}

NetworkConnection::~NetworkConnection() {
  NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void NetworkConnection::OnIPAddressChanged() {
  OnConnectionTypeChanged(NetworkChangeNotifier::GetConnectionType());
}

void NetworkConnection::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DVLOG(1) << "Updating NetworkConnection's Cached Data";

  connection_type_ = type;
  connection_description_ =
      NetworkChangeNotifier::ConnectionTypeToString(type).c_str();
}

}  // namespace net
