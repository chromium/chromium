// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_BROWSING_TOPICS_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_BROWSING_TOPICS_PARSER_H_

#include "base/component_export.h"
#include "net/http/http_response_headers.h"

namespace network {

// Parses the `Observe-Browsing-Topics` response headers. Returns true if the
// parsing succeeds and the parsed value is Boolean true; returns false
// otherwise. See
// https://patcg-individual-drafts.github.io/topics/#the-observe-browsing-topics-http-response-header-header.
COMPONENT_EXPORT(NETWORK_CPP)
bool ParseObserveBrowsingTopicsFromHeader(
    const net::HttpResponseHeaders& headers);
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_BROWSING_TOPICS_PARSER_H_
