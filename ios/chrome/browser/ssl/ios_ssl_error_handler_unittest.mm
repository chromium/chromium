// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_ssl_error_handler.h"

#include "base/bind.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ssl/captive_portal_tab_helper.h"
#import "ios/chrome/browser/ssl/captive_portal_tab_helper_delegate.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/web/public/web_state.h"
#include "net/http/http_status_code.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
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

// Test fixture for IOSSSLErrorHander when used with a WebState that hasn't
// been inserted into a WebStateList and hence doesn't have the usual set of
// tab helpers.
class IOSSSLErrorHandlerWithoutTabHelpersTest : public ChromeWebTest {
 protected:
  IOSSSLErrorHandlerWithoutTabHelpersTest()
      : ChromeWebTest(web::WebTaskEnvironment::Options::IO_MAINLOOP),
        cert_(net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      kTestCertFileName)) {}

  // Returns certificate.
  scoped_refptr<net::X509Certificate> cert() { return cert_; }

  // ChromeWebTest overrides:
  void SetUp() override {
    ChromeWebTest::SetUp();

    GetBrowserState()->SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_loader_factory_));
    test_loader_factory_.AddResponse("http://www.gstatic.com/generate_204", "",
                                     net::HTTP_NO_CONTENT);
  }

 private:
  network::TestURLLoaderFactory test_loader_factory_;
  scoped_refptr<net::X509Certificate> cert_;
};

// Tests that error handling is short-circuited when the associated WebState
// isn't in a WebStateList.
TEST_F(IOSSSLErrorHandlerWithoutTabHelpersTest, HandleError) {
  net::SSLInfo ssl_info;
  ssl_info.cert = cert();
  GURL url(kTestHostName);
  __block bool blocking_page_callback_called = false;
  base::OnceCallback<void(NSString*)> blocking_page_callback =
      base::BindOnce(^(NSString* blocking_page) {
        blocking_page_callback_called = true;
      });
  IOSSSLErrorHandler::HandleSSLError(
      web_state(), net::ERR_CERT_AUTHORITY_INVALID, ssl_info, url, true, 0,
      std::move(blocking_page_callback));
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool() {
        base::RunLoop().RunUntilIdle();
        return blocking_page_callback_called;
      }));
}

// Test fixture for IOSSSLErrorHandler class.
class IOSSSLErrorHandlerTest : public IOSSSLErrorHandlerWithoutTabHelpersTest {
 protected:
  IOSSSLErrorHandlerTest() {}

  // IOSSSLErrorHandlerWithoutTabHelpersTest overrides:
  void SetUp() override {
    IOSSSLErrorHandlerWithoutTabHelpersTest::SetUp();

    id captive_portal_tab_helper_delegate = [OCMockObject
        mockForProtocol:@protocol(CaptivePortalTabHelperDelegate)];

    security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
        web_state());

    CaptivePortalTabHelper::CreateForWebState(
        web_state(), captive_portal_tab_helper_delegate);
    ASSERT_TRUE(cert());

    // Transient item can only be added for pending non-app-specific loads.
    AddPendingItem(GURL(kTestHostName),
                   ui::PageTransition::PAGE_TRANSITION_TYPED);
  }
};

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
      std::move(blocking_page_callback));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool() {
        base::RunLoop().RunUntilIdle();
        return blocking_page_callback_called;
      }));
}
