// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_UTIL_WK_SECURITY_ORIGIN_UTIL_H_
#define IOS_WEB_UTIL_WK_SECURITY_ORIGIN_UTIL_H_

#import "url/gurl.h"
#import "url/origin.h"

@class WKSecurityOrigin;

namespace web {

// Converts WKSecurityOrigin to GURL origin.
// Returns empty url if `origin` is nil.
GURL GURLOriginWithWKSecurityOrigin(WKSecurityOrigin* origin);

// Converts WKSecurityOrigin to url::Origin.
// Returns a default constructed opaque Origin if `origin` is nil.
url::Origin OriginWithWKSecurityOrigin(WKSecurityOrigin* origin);

}  // namespace web

#endif  // IOS_WEB_UTIL_WK_SECURITY_ORIGIN_UTIL_H_
