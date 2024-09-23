// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/model/ios_ssl_error_handler.h"

#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/ssl/model/captive_portal_tab_helper.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "net/http/http_status_code.h"
#import "net/ssl/ssl_info.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
const char kTestCertFileName[] = "ok_cert.pem";
const char kTestHostName[] = "https://chromium.test/";
}  // namespace

// Test fixture for IOSSSLErrorHander when used with a WebState that hasn't
// been inserted into a WebStateList and hence doesn't have the usual set of
// tab helpers.
class IOSSSLErrorHandlerWithoutTabHelpersTest : public PlatformTest {
 protected:
  IOSSSLErrorHandlerWithoutTabHelpersTest()
      : cert_(net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      kTestCertFileName)) {}

  // Returns certificate.
  scoped_refptr<net::X509Certificate> cert() { return cert_; }

  // PlatformTest overrides:
  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    web_state_->SetBrowserState(profile_.get());

    profile_->SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_loader_factory_));
    test_loader_factory_.AddResponse("http://www.gstatic.com/generate_204", "",
                                     net::HTTP_NO_CONTENT);
  }

  web::WebState* web_state() const { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_ =
      std::make_unique<web::FakeWebState>();

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

    security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
        web_state());

    CaptivePortalTabHelper::GetOrCreateForWebState(web_state());
    ASSERT_TRUE(cert());

    std::unique_ptr<web::FakeNavigationManager> fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    // Transient item can only be added for pending non-app-specific loads.
    fake_navigation_manager->AddItem(GURL(kTestHostName),
                                     ui::PageTransition::PAGE_TRANSITION_TYPED);
    web_state_->SetNavigationManager(std::move(fake_navigation_manager));
  }
};

// Tests that error HTML is returned instead of calling the usual show
// interstitial logic when passed a non-null `blocking_page_callback`.
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
