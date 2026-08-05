// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_PARSER_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/types/optional_ref.h"
#include "services/network/public/mojom/devtools_observer.mojom-forward.h"
#include "services/network/public/mojom/origin_or_wildcard_header_value.mojom-forward.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace network {

struct ConnectionAllowlist;
struct ConnectionAllowlists;

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
COMPONENT_EXPORT(NETWORK_CPP)
ConnectionAllowlists ParseConnectionAllowlistsFromHeaders(
    const net::HttpResponseHeaders& headers,
    const GURL& response_url);

// Parses a single `Connection-Allowlist` / `Sec-Required-Connection-Allowlist`
// structured-header value (a single header value rather than a full set of
// response headers), resolving the `response-origin` token against
// `response_url`. Returns nullopt only when `header_value` is empty; a
// malformed value yields a present allowlist carrying the relevant
// `ConnectionAllowlistIssue`s (callers decide how to treat those). Used by
// Connection-Allowlist embedded enforcement, where the required allowlist
// arrives as an iframe attribute / request header value rather than a response
// header.
//
// When `response_url` is provided, the `response-origin` token is resolved
// against it and added to the allowlist. When it is nullopt -- used when the
// origin to resolve against isn't known at parse time, e.g. parsing an iframe
// attribute in the renderer -- the token instead sets `match_response_origin`
// on the result, and the browser resolves it later.
COMPONENT_EXPORT(NETWORK_CPP)
std::optional<ConnectionAllowlist> ParseConnectionAllowlist(
    const std::string& header_value,
    base::optional_ref<const GURL> response_url);

// Parses the `Allow-Connection-Allowlist-From` response header, which lets a
// framed document opt in to having its embedder enforce a required
// Connection-Allowlist on it (Connection-Allowlist embedded enforcement). The
// grammar mirrors `Allow-CSP-From`: either the token `*` or a single serialized
// origin, so the parsed value reuses `mojom::OriginOrWildcardHeaderValue`.
// Returns null when the header is absent.
COMPONENT_EXPORT(NETWORK_CPP)
mojom::OriginOrWildcardHeaderValuePtr ParseAllowConnectionAllowlistFromHeader(
    const net::HttpResponseHeaders& headers);

// Returns true if a framed document loaded from `response_url` allows its
// embedder (at `request_origin`) to enforce an arbitrary required
// Connection-Allowlist on it without the frame having to assert a matching
// policy itself. This is the case for local schemes (which inherit their
// embedder's policies) and when the response carries an
// `Allow-Connection-Allowlist-From` header that names the embedder's origin (or
// `*`). `allow_connection_allowlist_from` may be null.
COMPONENT_EXPORT(NETWORK_CPP)
bool AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
    const url::Origin& request_origin,
    const GURL& response_url,
    const mojom::OriginOrWildcardHeaderValue* allow_connection_allowlist_from);

COMPONENT_EXPORT(NETWORK_CPP)
void ReportConnectionAllowlistIssuesToDevtools(
    const ConnectionAllowlists& allowlists,
    const raw_ptr<mojom::DevToolsObserver> devtools_observer,
    const std::string& devtools_request_id,
    const GURL& request_url);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_PARSER_H_
