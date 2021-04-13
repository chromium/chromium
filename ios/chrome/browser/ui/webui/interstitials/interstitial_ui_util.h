// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_UTIL_H_

#include <memory>

class GURL;
namespace web {
class WebInterstitialDelegate;
class WebState;
}  // namespace web

// Creates an interstitial delegate for chrome://interstitials/ssl.
std::unique_ptr<web::WebInterstitialDelegate> CreateSslBlockingPageDelegate(
    web::WebState* web_state,
    const GURL& url);

// Creates an interstitial delegate for chrome://interstitials/captiveportal.
std::unique_ptr<web::WebInterstitialDelegate>
CreateCaptivePortalBlockingPageDelegate(web::WebState* web_state);

// Creates an interstitial delegate for chrome://interstitials/safe_browsing.
std::unique_ptr<web::WebInterstitialDelegate>
CreateSafeBrowsingBlockingPageDelegate(web::WebState* web_state,
                                       const GURL& url);

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_UTIL_H_
