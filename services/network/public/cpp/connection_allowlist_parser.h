// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_PARSER_H_

#include "base/component_export.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/connection_allowlist.h"

class GURL;

namespace network {

// Parses `Connection-Allowlist` and `Connection-Allowlist-Report-Only` headers
// from a net::HttpResponseHeaders object, returning a `ConnectionAllowlists`
// object whose optional fields will be populated iff the relevant header
// is present.
//
// If the asserted lists of patterns contain the `response-origin` token, then
// the serialization of `response_url`'s origin will be added to the relevant
// allowlist.
//
// https://github.com/mikewest/anti-exfil
COMPONENT_EXPORT(NETWORK_CPP_CONNECTION_ALLOWLIST)
ConnectionAllowlists ParseConnectionAllowlistsFromHeaders(
    const net::HttpResponseHeaders& headers,
    const GURL& response_url);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_PARSER_H_
