// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INTERSTITIALS_INTERSTITIAL_UI_UTIL_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INTERSTITIALS_INTERSTITIAL_UI_UTIL_H_

#include <memory>

class GURL;
namespace security_interstitials {
class IOSSecurityInterstitialPage;
}  // namespace security_interstitials

namespace web {
class WebState;
}  // namespace web

// Creates an interstitial page for chrome://interstitials/ssl.
std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage>
CreateSslBlockingPage(web::WebState* web_state, const GURL& url);

// Creates an interstitial page for chrome://interstitials/captiveportal.
std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage>
CreateCaptivePortalBlockingPage(web::WebState* web_state);

// Creates an interstitial page for chrome://interstitials/safe_browsing.
std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage>
CreateSafeBrowsingBlockingPage(web::WebState* web_state, const GURL& url);

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INTERSTITIALS_INTERSTITIAL_UI_UTIL_H_
