// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_COOKIES_WK_COOKIE_UTIL_H_
#define IOS_WEB_NET_COOKIES_WK_COOKIE_UTIL_H_

#include "base/mac/availability.h"

@class WKHTTPCookieStore;

namespace web {

class BrowserState;

// Returns WKHTTPCookieStore for the given BrowserState. If BrowserState is
// OffTheRecord then the resulting WKHTTPCookieStore will be a part of
// ephemeral WKWebsiteDataStore.
WKHTTPCookieStore* WKCookieStoreForBrowserState(BrowserState* browser_state);

}  // namespace web

#endif  // IOS_WEB_NET_COOKIES_WK_COOKIE_UTIL_H_
