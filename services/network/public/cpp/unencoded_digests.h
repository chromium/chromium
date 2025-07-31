// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_UNENCODED_DIGESTS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_UNENCODED_DIGESTS_H_

#include "base/component_export.h"
#include "services/network/public/mojom/unencoded_digest.mojom.h"

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

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_UNENCODED_DIGESTS_H_
