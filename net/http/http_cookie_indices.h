// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_COOKIE_INDICES_H_
#define NET_HTTP_HTTP_COOKIE_INDICES_H_

#include <optional>
#include <string>
#include <vector>

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

}  // namespace net

#endif  // NET_HTTP_HTTP_COOKIE_INDICES_H_
