// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_HANDLE_H_
#define NET_BASE_NETWORK_HANDLE_H_

#include <stdint.h>

namespace net::handles {

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

}  // namespace net::handles

#endif  // NET_BASE_NETWORK_HANDLE_H_
