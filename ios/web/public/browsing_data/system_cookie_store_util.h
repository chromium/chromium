// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSING_DATA_SYSTEM_COOKIE_STORE_UTIL_H_
#define IOS_WEB_PUBLIC_BROWSING_DATA_SYSTEM_COOKIE_STORE_UTIL_H_

#include <memory>

namespace net {
class SystemCookieStore;
}  // namespace net

namespace web {

class BrowserState;

// Returns SystemCookieStore for the given BrowserState.
std::unique_ptr<net::SystemCookieStore> CreateSystemCookieStore(
    BrowserState* browser_state);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSING_DATA_SYSTEM_COOKIE_STORE_UTIL_H_
