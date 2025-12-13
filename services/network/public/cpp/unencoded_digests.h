// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_UNENCODED_DIGESTS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_UNENCODED_DIGESTS_H_

#include "base/component_export.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/unencoded_digest.mojom.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {

// Parses the Unencoded-Digest response header.
//
// https://www.ietf.org/archive/id/draft-ietf-httpbis-unencoded-digest-00.html
COMPONENT_EXPORT(NETWORK_CPP)
mojom::UnencodedDigestsPtr ParseUnencodedDigestsFromHeaders(
    const net::HttpResponseHeaders& headers);

COMPONENT_EXPORT(NETWORK_CPP)
void ReportUnencodedDigestIssuesToDevtools(
    const mojom::UnencodedDigestsPtr& digests,
    const raw_ptr<mojom::DevToolsObserver> devtools_observer,
    const std::string& devtools_request_id,
    const GURL& request_url);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_UNENCODED_DIGESTS_H_
