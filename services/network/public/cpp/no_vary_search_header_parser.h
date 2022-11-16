// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NO_VARY_SEARCH_HEADER_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NO_VARY_SEARCH_HEADER_PARSER_H_

#include "base/component_export.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"

namespace network {

COMPONENT_EXPORT(NETWORK_CPP)
mojom::NoVarySearchPtr ParseNoVarySearch(
    const net::HttpResponseHeaders& headers);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NO_VARY_SEARCH_HEADER_PARSER_H_
