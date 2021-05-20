// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HEADER_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HEADER_UTIL_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace network {

// Checks if a single request header is safe to send.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsRequestHeaderSafe(const base::StringPiece& key,
                         const base::StringPiece& value);

// Checks if any single header in a set of request headers is not safe to send.
// When adding sets of headers together, it's safe to call this on each set
// individually.
COMPONENT_EXPORT(NETWORK_CPP)
bool AreRequestHeadersSafe(const net::HttpRequestHeaders& request_headers);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HEADER_UTIL_H_