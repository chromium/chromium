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
  connection_description_ = NetworkChangeNotifier::ConnectionTypeToString(type);
  if (connection_type_ != NetworkChangeNotifier::CONNECTION_UNKNOWN &&
      connection_type_ != NetworkChangeNotifier::CONNECTION_WIFI) {
    return;
  }

  // This function only seems usefully defined on Windows currently.
  WifiPHYLayerProtocol wifi_type = GetWifiPHYLayerProtocol();
  switch (wifi_type) {
    case WIFI_PHY_LAYER_PROTOCOL_NONE:
      // No wifi support or no associated AP.
      break;
    case WIFI_PHY_LAYER_PROTOCOL_ANCIENT:
      // An obsolete modes introduced by the original 802.11, e.g. IR, FHSS.
      connection_description_ = "CONNECTION_WIFI_ANCIENT";
      break;
    case WIFI_PHY_LAYER_PROTOCOL_A:
      // 802.11a, OFDM-based rates.
      connection_description_ = "CONNECTION_WIFI_802.11a";
      break;
    case WIFI_PHY_LAYER_PROTOCOL_B:
      // 802.11b, DSSS or HR DSSS.
      connection_description_ = "CONNECTION_WIFI_802.11b";
      break;
    case WIFI_PHY_LAYER_PROTOCOL_G:
      // 802.11g, same rates as 802.11a but compatible with 802.11b.
      connection_description_ = "CONNECTION_WIFI_802.11g";
      break;
    case WIFI_PHY_LAYER_PROTOCOL_N:
      // 802.11n, HT rates.
      connection_description_ = "CONNECTION_WIFI_802.11n";
      break;
    case WIFI_PHY_LAYER_PROTOCOL_AC:
      // 802.11ac
      connection_description_ = "CONNECTION_WIFI_802.11ac";
      break;
    case WIFI_PHY_LAYER_PROTOCOL_AD:
      // 802.11ad
      connection_description_ = "CONNECTION_WIFI_802.11ad";
      break;
    case WIFI_PHY_LAYER_PROTOCOL_AX:
      // 802.11ax
      connection_description_ = "CONNECTION_WIFI_802.11ax";
      break;
    case WIFI_PHY_LAYER_PROTOCOL_UNKNOWN:
      // Unclassified mode or failure to identify.
      break;
  }
}

}  // namespace net
