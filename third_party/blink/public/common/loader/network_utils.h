// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_NETWORK_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_NETWORK_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "net/http/http_response_headers.h"
#include "third_party/blink/public/common/common_export.h"
#include "url/gurl.h"

namespace blink {
namespace network_utils {

// Returns true if the headers indicate that this resource should always be
// revalidated or not cached.
BLINK_COMMON_EXPORT bool AlwaysAccessNetwork(
    const scoped_refptr<net::HttpResponseHeaders>& headers);

// Helper function to determine if a request for |url| refers to a network
// resource (as opposed to a local browser resource like files or blobs). Used
// when the Network Service is enabled.
//
// Note that this is not equivalent to the
// content::IsURLHandledByNetworkStack(), as several non-network schemes are
// handled by the network stack when the Network Service is disabled.
BLINK_COMMON_EXPORT bool IsURLHandledByNetworkService(const GURL& url);

// Returns true if |url|'s origin is trustworthy. There are two cases:
// a) it can be said that |url|'s contents were transferred to the browser in
// a way that a network attacker cannot tamper with or observe. (see
// https://www.w3.org/TR/powerful-features/#is-origin-trustworthy).
// b) IsWhitelistedAsSecureOrigin(url::Origin::Create(url)) returns true.
//
// Note that this is not equivalent to checking if an entire site is secure
// (i.e. no degraded security state UI is displayed to the user), since there
// may be insecure iframes present even if this method returns true.
//
// TODO(lukasza): Remove this function and use
// network::IsUrlPotentiallyTrustworthy instead.
BLINK_COMMON_EXPORT bool IsOriginSecure(const GURL& url);

}  // namespace network_utils
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_NETWORK_UTILS_H_
