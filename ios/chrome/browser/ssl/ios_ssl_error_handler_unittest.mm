// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_ssl_error_handler.h"

#include "base/bind.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"
#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper_delegate.h"
#include "ios/web/public/security/web_interstitial.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#include "net/http/http_status_code.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/test/test_url_loader_factory.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
const char kTestCertFileName[] = "ok_cert.pem";
const char kTestHostName[] = "https://chromium.test/";
}  // namespace

// Test fixture for IOSSSLErrorHandler class.
class IOSSSLErrorHandlerTest : public web::WebTestWithWebState {
 protected:
  IOSSSLErrorHandlerTest()
      : browser_state_(builder_.Build()),
        cert_(net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      kTestCertFileName)) {}

  // web::WebTestWithWebState overrides:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();

    test_loader_factory_.AddResponse("http://www.gstatic.com/generate_204", "",
                                     net::HTTP_NO_CONTENT);

    id captive_portal_detector_tab_helper_delegate = [OCMockObject
        mockForProtocol:@protocol(CaptivePortalDetectorTabHelperDelegate)];
    // Use a testing URLLoaderFactory so that these tests don't attempt to make
    // network requests.
    CaptivePortalDetectorTabHelper::CreateForWebState(
        web_state(), captive_portal_detector_tab_helper_delegate,
        &test_loader_factory_);
    ASSERT_TRUE(cert_);
    ASSERT_FALSE(web_state()->IsShowingWebInterstitial());

    // Transient item can only be added for pending non-app-specific loads.
    AddPendingItem(GURL(kTestHostName),
                   ui::PageTransition::PAGE_TRANSITION_TYPED);
  }
  web::BrowserState* GetBrowserState() override { return browser_state_.get(); }

  // Waits for and returns true if an interstitial is displayed. Returns false
  // otherwise.
  WARN_UNUSED_RESULT bool WaitForInterstitialDisplayed() {
    // Required in order for CaptivePortalDetector to receive simulated network
    // response from |test_loader_factory_|.
    base::RunLoop().RunUntilIdle();

    // Wait for the interstitial to be displayed.
    return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
      return web_state()->IsShowingWebInterstitial();
    });
  }

  // Returns certificate for testing.
  scoped_refptr<net::X509Certificate> cert() { return cert_; }

 private:
  network::TestURLLoaderFactory test_loader_factory_;
  TestChromeBrowserState::Builder builder_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  scoped_refptr<net::X509Certificate> cert_;
};

// Tests non-overridable error handling.
TEST_F(IOSSSLErrorHandlerTest, NonOverridable) {
  net::SSLInfo ssl_info;
  ssl_info.cert = cert();
  GURL url(kTestHostName);
  __block bool do_not_proceed_callback_called = false;
  base::OnceCallback<void(NSString*)> null_callback;
  IOSSSLErrorHandler::HandleSSLError(
      web_state(), net::ERR_CERT_AUTHORITY_INVALID, ssl_info, url, false, 0,
      base::BindRepeating(^(bool proceed) {
        EXPECT_FALSE(proceed);
        do_not_proceed_callback_called = true;
      }),
      std::move(null_callback));

  EXPECT_TRUE(WaitForInterstitialDisplayed());
  web::WebInterstitial* interstitial = web_state()->GetWebInterstitial();
  EXPECT_TRUE(interstitial);

  // Make sure callback is called on dismissal.
  interstitial->DontProceed();
  EXPECT_TRUE(do_not_proceed_callback_called);
}

// Tests proceed with overridable error.
// Flaky: http://crbug.com/660343.
TEST_F(IOSSSLErrorHandlerTest, DISABLED_OverridableProceed) {
  net::SSLInfo ssl_info;
  ssl_info.cert = cert();
  GURL url(kTestHostName);
  __block bool proceed_callback_called = false;
  base::OnceCallback<void(NSString*)> null_callback;
  IOSSSLErrorHandler::HandleSSLError(
      web_state(), net::ERR_CERT_AUTHORITY_INVALID, ssl_info, url, true, 0,
      base::BindRepeating(^(bool proceed) {
        EXPECT_TRUE(proceed);
        proceed_callback_called = true;
      }),
      std::move(null_callback));

  EXPECT_TRUE(WaitForInterstitialDisplayed());
  web::WebInterstitial* interstitial = web_state()->GetWebInterstitial();
  EXPECT_TRUE(interstitial);

  // Make sure callback is called on dismissal.
  interstitial->Proceed();
  EXPECT_TRUE(proceed_callback_called);
}

// Tests do not proceed with overridable error.
TEST_F(IOSSSLErrorHandlerTest, OverridableDontProceed) {
  net::SSLInfo ssl_info;
  ssl_info.cert = cert();
  GURL url(kTestHostName);
  __block bool do_not_proceed_callback_called = false;
  base::OnceCallback<void(NSString*)> null_callback;
  IOSSSLErrorHandler::HandleSSLError(
      web_state(), net::ERR_CERT_AUTHORITY_INVALID, ssl_info, url, true, 0,
      base::BindRepeating(^(bool proceed) {
        EXPECT_FALSE(proceed);
        do_not_proceed_callback_called = true;
      }),
      std::move(null_callback));

  EXPECT_TRUE(WaitForInterstitialDisplayed());
  web::WebInterstitial* interstitial = web_state()->GetWebInterstitial();
  EXPECT_TRUE(interstitial);

  // Make sure callback is called on dismissal.
  interstitial->DontProceed();
  EXPECT_TRUE(do_not_proceed_callback_called);
}

// Tests that error HTML is returned instead of calling the usual show
// interstitial logic when passed a non-null |blocking_page_callback|.
TEST_F(IOSSSLErrorHandlerTest, CommittedInterstitialErrorHtml) {
  net::SSLInfo ssl_info;
  ssl_info.cert = cert();
  GURL url(kTestHostName);
  __block bool blocking_page_callback_called = false;
  base::OnceCallback<void(bool)> null_callback;
  base::OnceCallback<void(NSString*)> blocking_page_callback =
      base::BindOnce(^(NSString* blocking_page_html) {
        EXPECT_NE(blocking_page_html, nil);
        blocking_page_callback_called = true;
      });
  IOSSSLErrorHandler::HandleSSLError(
      web_state(), net::ERR_CERT_AUTHORITY_INVALID, ssl_info, url, true, 0,
      std::move(null_callback), std::move(blocking_page_callback));
  EXPECT_FALSE(WaitForInterstitialDisplayed());
  EXPECT_TRUE(blocking_page_callback_called);
}
