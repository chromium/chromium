// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSING_DATA_COOKIE_BLOCKING_MODE_H_
#define IOS_WEB_PUBLIC_BROWSING_DATA_COOKIE_BLOCKING_MODE_H_

namespace web {

// Represents the cookie blocking mode for a given BrowserState. The setting
// will apply to all WebStates in that BrowserState.
enum class CookieBlockingMode {
  // Allow all cookies.
  kAllow,
  // Block all third-party cookies.
  kBlockThirdParty,
  // Block all cookies.
  kBlock,
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSING_DATA_COOKIE_BLOCKING_MODE_H_
