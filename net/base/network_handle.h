// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_HANDLE_H_
#define NET_BASE_NETWORK_HANDLE_H_

#include <stdint.h>

#include "net/base/net_export.h"

namespace net {

class IPEndPoint;

namespace handles {

// Opaque handle for device-wide connection to a particular network. For
// example an association with a particular WiFi network with a particular
// SSID or a connection to particular cellular network.
// The meaning of this handle is target-dependent. On Android
// handles::NetworkHandles are equivalent to:
//   On Lollipop, the framework's concept of NetIDs (e.g. Network.netId), and
//   On Marshmallow and newer releases, network handles
//           (e.g. Network.getNetworkHandle()).
typedef int64_t NetworkHandle;

// An invalid NetworkHandle.
inline constexpr NetworkHandle kInvalidNetworkHandle{-1};

// If enabled, network binding emulation will be enabled. The following happens:
// 1. net::android::BindToNetwork() becomes a no-op.
// 2. The socket layer translates, behind the scenes, the 127.0.0.1 destination
//    address to 127.0.0.X (where X == target_network received by the socket).
//    See net::handles::MaybeTranslateEmulatedNetworkAddressForTesting().
// See chrome/browser/multi_network_browser_test.cc for more details.
NET_EXPORT void SetEmulateNetworkBindingForTesting(bool enabled);
NET_EXPORT bool GetEmulateNetworkBindingForTesting();

// If network binding emulation is enabled:
// - If `target_network` != kInvalidNetworkHandle, CHECKs that `target_network`
//   is a valid test handle and translates destination IPv4 loopback (127.0.0.1)
//   to 127.0.0.X (where X == target_network).
// - Otherwise, returns `address` unchanged.
NET_EXPORT IPEndPoint
MaybeTranslateEmulatedNetworkAddressForTesting(const IPEndPoint& address,
                                               NetworkHandle target_network);

}  // namespace handles
}  // namespace net

#endif  // NET_BASE_NETWORK_HANDLE_H_
