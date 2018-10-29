// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/features.h"

namespace web {
namespace features {

const base::Feature kIgnoresViewportScaleLimits{
    "IgnoresViewportScaleLimits", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebFrameMessaging{"WebFrameMessaging",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSlimNavigationManager{"SlimNavigationManager",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWKHTTPSystemCookieStore{"WKHTTPSystemCookieStore",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCrashOnUnexpectedURLChange{
    "CrashOnUnexpectedURLChange", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBrowserContainerFullscreen{
    "BrowserContainerFullscreen", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOutOfWebFullscreen{"OutOfWebFullscreen",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace web
