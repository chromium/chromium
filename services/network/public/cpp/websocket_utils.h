// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_WEBSOCKET_UTILS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_WEBSOCKET_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"

class GURL;

namespace net {
class IsolationInfo;
}  // namespace net

namespace network {

// Verifies the WebSocket connection parameters. Returns std::nullopt if they
// are valid. If not, returns the error message.
COMPONENT_EXPORT(NETWORK_CPP)
std::optional<std::string> VerifyWebSocketConnectParameters(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const net::IsolationInfo& isolation_info);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_WEBSOCKET_UTILS_H_
