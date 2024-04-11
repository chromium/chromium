// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DOCUMENT_ISOLATION_POLICY_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DOCUMENT_ISOLATION_POLICY_PARSER_H_

#include "base/component_export.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {

struct DocumentIsolationPolicy;

// Parses the Document-Isolation-Policy and
// Document-Isolation-Policy-Report-Only headers.
COMPONENT_EXPORT(NETWORK_CPP)
DocumentIsolationPolicy ParseDocumentIsolationPolicy(
    const net::HttpResponseHeaders& headers);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DOCUMENT_ISOLATION_POLICY_PARSER_H_
