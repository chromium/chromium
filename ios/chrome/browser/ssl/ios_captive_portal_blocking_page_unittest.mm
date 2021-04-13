// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/ios_captive_portal_blocking_page.h"

#include "base/bind.h"
#import "base/test/ios/wait_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/web/public/security/web_interstitial.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture to test captive portal blocking page.
typedef ChromeWebTest IOSCaptivePortalBlockingPageTest;

// Tests display of captive portal blocking page.
TEST_F(IOSCaptivePortalBlockingPageTest, PresentAndDismiss) {
  GURL url("http://request");
  LoadHtml(@"html", url);

  __block bool do_not_proceed_callback_called = false;
  security_interstitials::MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = "test";
  IOSCaptivePortalBlockingPage* page = new IOSCaptivePortalBlockingPage(
      web_state(), url, GURL("http://landing"), base::BindOnce(^(bool proceed) {
        EXPECT_FALSE(proceed);
        do_not_proceed_callback_called = true;
      }),
      new security_interstitials::IOSBlockingPageControllerClient(
          web_state(),
          std::make_unique<security_interstitials::MetricsHelper>(
              GURL(), report_details, nullptr),
          "en-US"));
  page->Show();

  // Make sure that interstitial is displayed.
  EXPECT_TRUE(web_state()->IsShowingWebInterstitial());
  web::WebInterstitial* interstitial = web_state()->GetWebInterstitial();
  ASSERT_TRUE(interstitial);

  // Make sure callback is called on dismissal.
  interstitial->DontProceed();
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return do_not_proceed_callback_called;
      }));
}
