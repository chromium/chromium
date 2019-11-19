// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ORIGIN_UTIL_H_
#define CONTENT_PUBLIC_COMMON_ORIGIN_UTIL_H_

#include "content/common/content_export.h"
#include "url/origin.h"

class GURL;

namespace content {

// Returns true if |url|'s origin is trustworthy. There are two cases:
// a) it can be said that |url|'s contents were transferred to the browser in a
//    way that a network attacker cannot tamper with or observe. (see
//    https://www.w3.org/TR/powerful-features/#is-origin-trustworthy).
// b) IsWhitelistedAsSecureOrigin(url::Origin::Create(url)) returns true.
//
// Note that this is not equivalent to checking if an entire site is secure
// (i.e. no degraded security state UI is displayed to the user), since there
// may be insecure iframes present even if this method returns true.
//
// TODO(lukasza): Remove this function and use
// network::IsUrlPotentiallyTrustworthy instead.
bool CONTENT_EXPORT IsOriginSecure(const GURL& url);

// Returns true if the origin can register a service worker.  Scheme must be
// http (localhost only), https, or a custom-set secure scheme.
bool CONTENT_EXPORT OriginCanAccessServiceWorkers(const GURL& url);

// This is based on SecurityOrigin::isPotentiallyTrustworthy and tries to mimic
// its behavior.
//
// TODO(lukasza): Remove this function and use
// network::IsOriginPotentiallyTrustworthy instead.
bool CONTENT_EXPORT IsPotentiallyTrustworthyOrigin(const url::Origin& origin);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ORIGIN_UTIL_H_
