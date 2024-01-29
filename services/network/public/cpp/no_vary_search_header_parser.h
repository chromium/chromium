// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NO_VARY_SEARCH_HEADER_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NO_VARY_SEARCH_HEADER_PARSER_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {

COMPONENT_EXPORT(NETWORK_CPP)
mojom::NoVarySearchWithParseErrorPtr ParseNoVarySearch(
    const net::HttpResponseHeaders& headers);

COMPONENT_EXPORT(NETWORK_CPP)
std::optional<std::string> GetNoVarySearchConsoleMessage(
    const mojom::NoVarySearchParseError& error,
    const GURL& prefetched_url);

COMPONENT_EXPORT(NETWORK_CPP)
std::optional<std::string> GetNoVarySearchHintConsoleMessage(
    const mojom::NoVarySearchParseError& error);
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NO_VARY_SEARCH_HEADER_PARSER_H_
