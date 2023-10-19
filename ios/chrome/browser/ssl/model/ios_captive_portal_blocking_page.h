// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_MODEL_IOS_CAPTIVE_PORTAL_BLOCKING_PAGE_H_
#define IOS_CHROME_BROWSER_SSL_MODEL_IOS_CAPTIVE_PORTAL_BLOCKING_PAGE_H_

#include "base/functional/callback.h"
#include "ios/components/security_interstitials/ios_security_interstitial_page.h"
#include "url/gurl.h"

// This class is responsible for showing/hiding the interstitial page that is
// shown when a certificate error is caused by the user being behind a captive
// portal.
// It deletes itself when the interstitial page is closed.
class IOSCaptivePortalBlockingPage
    : public security_interstitials::IOSSecurityInterstitialPage {
 public:
  IOSCaptivePortalBlockingPage(const IOSCaptivePortalBlockingPage&) = delete;
  IOSCaptivePortalBlockingPage& operator=(const IOSCaptivePortalBlockingPage&) =
      delete;

  ~IOSCaptivePortalBlockingPage() override;

  // Creates a captive portal blocking page. If the blocking page isn't shown,
  // the caller is responsible for cleaning up the blocking page, otherwise the
  // interstitial takes ownership when shown. The `web_state` and `request_url`
  // of the request which this interstitial page is associated. `landing_url` is
  // the web page which allows the user to complete their connection to the
  // network.
  IOSCaptivePortalBlockingPage(
      web::WebState* web_state,
      const GURL& request_url,
      const GURL& landing_url,
      security_interstitials::IOSBlockingPageControllerClient* client);

 private:
  // IOSSecurityInterstitialPage overrides:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(base::Value::Dict& value) const override;
  void HandleCommand(
      security_interstitials::SecurityInterstitialCommand command) override;

  // The landing page url for the captive portal network.
  const GURL landing_url_;
};

#endif  // IOS_CHROME_BROWSER_SSL_MODEL_IOS_CAPTIVE_PORTAL_BLOCKING_PAGE_H_
