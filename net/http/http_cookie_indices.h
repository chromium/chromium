// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_COOKIE_INDICES_H_
#define NET_HTTP_HTTP_COOKIE_INDICES_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "net/base/net_export.h"

namespace net {

class HttpResponseHeaders;

// Parse the Cookie-Indices response header, if present (even if the Vary header
// is not).
//
// Returns an empty optional if the header was absent, not a valid structured
// list, or contained an invalid/unrecognized item.
NET_EXPORT std::optional<std::vector<std::string>> ParseCookieIndices(
    const HttpResponseHeaders& headers);

// Processes the Cookie-Indices value (as presented above) and the cookies
// found in a request to produce a compact hash that can be compared later.
// Currently this is done with SHA-256, which is a cryptographic hash function.
// Comparing hashes computed with different |cookie_indices| arrays is
// unspecified -- don't do it.
//
// |cookie_indices| must be sorted and unique; |cookies| may appear in any
// order.
using CookieIndicesHash = std::array<uint8_t, 32>;
NET_EXPORT CookieIndicesHash HashCookieIndices(
    base::span<const std::string> cookie_indices,
    base::span<const std::pair<std::string, std::string>> cookies);

}  // namespace net

#endif  // NET_HTTP_HTTP_COOKIE_INDICES_H_
