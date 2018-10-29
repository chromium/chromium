// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FEATURES_H_
#define IOS_WEB_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace web {
namespace features {

// Used to always allow scaling of the web page, regardless of author intent.
extern const base::Feature kIgnoresViewportScaleLimits;

// Used to enable API to send messages directly to frames of a webpage.
extern const base::Feature kWebFrameMessaging;

// Used to enable the WKBackForwardList based navigation manager.
extern const base::Feature kSlimNavigationManager;

// Used to enable using WKHTTPSystemCookieStore in main context URL requests.
extern const base::Feature kWKHTTPSystemCookieStore;

// Used to crash the browser if unexpected URL change is detected.
// https://crbug.com/841105.
extern const base::Feature kCrashOnUnexpectedURLChange;

// Used to make BrowserContainerViewController fullscreen.
extern const base::Feature kBrowserContainerFullscreen;

// Used to use the fullscreen implementation out of web.
extern const base::Feature kOutOfWebFullscreen;

}  // namespace features
}  // namespace web

#endif  // IOS_WEB_PUBLIC_FEATURES_H_
