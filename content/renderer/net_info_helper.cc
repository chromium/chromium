// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/net_info_helper.h"

namespace content {

blink::WebConnectionType
NetConnectionTypeToWebConnectionType(
    net::NetworkChangeNotifier::ConnectionType net_type) {
  switch (net_type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return blink::kWebConnectionTypeUnknown;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return blink::kWebConnectionTypeEthernet;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return blink::kWebConnectionTypeWifi;
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return blink::kWebConnectionTypeNone;
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return blink::kWebConnectionTypeCellular2G;
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return blink::kWebConnectionTypeCellular3G;
    case net::NetworkChangeNotifier::CONNECTION_4G:
    // TODO(crbug.com/40148439): Introduce a new WebConnectionType for 5G.
    case net::NetworkChangeNotifier::CONNECTION_5G:
      return blink::kWebConnectionTypeCellular4G;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return blink::kWebConnectionTypeBluetooth;
  }

  NOTREACHED_IN_MIGRATION();
  return blink::kWebConnectionTypeNone;
}

}  // namespace content
