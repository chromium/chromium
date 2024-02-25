// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SUPPORTS_LOADING_MODE_SUPPORTS_LOADING_MODE_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SUPPORTS_LOADING_MODE_SUPPORTS_LOADING_MODE_PARSER_H_

#include <string_view>

#include "base/component_export.h"
#include "services/network/public/mojom/supports_loading_mode.mojom-forward.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {

// This parser is intended to run in a relatively unprivileged process, such as
// the network process or a renderer process.

// Parse one or more supported loading modes from a string.
//
// Returns nullptr if the header syntax was invalid.
COMPONENT_EXPORT(NETWORK_CPP)
mojom::SupportsLoadingModePtr ParseSupportsLoadingMode(
    std::string_view header_value);

// Parse Supports-Loading-Modes from HTTP response headers. If multiple headers
// are found, they are assumed to be canonicalized by joining them with commas,
// as typical for HTTP.
//
// Returns nullptr if the header syntax was invalid.
COMPONENT_EXPORT(NETWORK_CPP)
mojom::SupportsLoadingModePtr ParseSupportsLoadingMode(
    const net::HttpResponseHeaders& headers);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SUPPORTS_LOADING_MODE_SUPPORTS_LOADING_MODE_PARSER_H_
