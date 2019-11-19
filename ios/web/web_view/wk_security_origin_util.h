// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_VIEW_WK_SECURITY_ORIGIN_UTIL_H_
#define IOS_WEB_WEB_VIEW_WK_SECURITY_ORIGIN_UTIL_H_

#include "url/gurl.h"

@class WKSecurityOrigin;

namespace web {

// Converts WKSecurityOrigin to GURL origin.
// Returns empty url if |origin| is nil.
GURL GURLOriginWithWKSecurityOrigin(WKSecurityOrigin* origin);

}  // namespace web

#endif  // IOS_WEB_WEB_VIEW_WK_SECURITY_ORIGIN_UTIL_H_
