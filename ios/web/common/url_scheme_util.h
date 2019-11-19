// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_URL_SCHEME_UTIL_H_
#define IOS_WEB_COMMON_URL_SCHEME_UTIL_H_

class GURL;
@class NSURL;

namespace web {

// Returns true if the URL has a www content scheme, i.e. http, https or data.
bool UrlHasWebScheme(const GURL& url);

// NOTE: Use these methods only in an NSURL-only context. Otherwise, use
// GURL's IsScheme, or the GURL version of UrlHasWebScheme.
// These functions will always return the same thing that the equivalent
// GURL call would return (assuming the URL is a valid GURL).
// Returns true if the URL has a www content scheme, i.e. http, https or data.
bool UrlHasWebScheme(NSURL* url);

}  // namespace web

#endif  // IOS_WEB_COMMON_URL_SCHEME_UTIL_H_
