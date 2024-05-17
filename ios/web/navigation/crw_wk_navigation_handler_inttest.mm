// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_wk_navigation_handler.h"

#import "base/scoped_observation.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/https_upgrade_type.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/security/wk_web_view_security_util.h"
#import "ios/web/test/web_int_test.h"
#import "ios/web/web_view/error_translation_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using base::test::ios::kWaitForPageLoadTimeout;
using web::HttpsUpgradeType;

namespace {

// A WebStateObserver that observes that the navigation is finished and keeps
// track of the error type (SSL or net error).
class FailedWebStateObserver : public web::WebStateObserver {
 public:
  // Type of the error that caused the navigation to fail.
  enum class ErrorType {
    kNone,
    // The navigation failed due to an SSL error such as an invalid certificate.
    kSSLError,
    // The navigation failed due to a net error such as an invalid hostname.
    kNetError
  };

  FailedWebStateObserver() = default;
  FailedWebStateObserver(const FailedWebStateObserver&) = delete;
  FailedWebStateObserver& operator=(const FailedWebStateObserver&) = delete;

  void DidFinishNavigation(
      web::WebState* web_state,
      web::NavigationContext* navigation_context) override {
    did_finish_ = true;
    failed_https_upgrade_type_ =
        navigation_context->GetFailedHttpsUpgradeType();

    DCHECK_EQ(ErrorType::kNone, error_type_);
    NSError* error = navigation_context->GetError();
    if (web::IsWKWebViewSSLCertError(error)) {
      error_type_ = ErrorType::kSSLError;
    } else {
      int error_code = 0;
      if (!web::GetNetErrorFromIOSErrorCode(
              error.code, &error_code,
              net::NSURLWithGURL(navigation_context->GetUrl()))) {
        error_code = net::ERR_FAILED;
      }
      if (error_code != net::OK) {
        error_type_ = ErrorType::kNetError;
      }
    }
  }

  void WebStateDestroyed(web::WebState* web_state) override {
    NOTREACHED_IN_MIGRATION();
  }

  bool did_finish() const { return did_finish_; }
  web::HttpsUpgradeType failed_https_upgrade_type() const {
    return failed_https_upgrade_type_;
  }
  ErrorType error_type() const { return error_type_; }

 private:
  bool did_finish_ = false;
  web::HttpsUpgradeType failed_https_upgrade_type_ =
      web::HttpsUpgradeType::kNone;
  ErrorType error_type_ = ErrorType::kNone;
};

}  // namespace

namespace web {

class CRWKNavigationHandlerIntTest : public WebIntTest {
 protected:
  CRWKNavigationHandlerIntTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    net::test_server::RegisterDefaultHandlers(&server_);
    net::test_server::RegisterDefaultHandlers(&https_server_);
  }

  FakeWebClient* GetWebClient() override {
    return static_cast<FakeWebClient*>(WebIntTest::GetWebClient());
  }

  // Tests the failed HTTPS upgradestatus of a navigation. Navigates to `url`
  // using `https_upgrade_type` as the HTTPS upgrade type. Expects
  // GetFailedHTTPSUpgradeType() to be equal to `https_upgrade_type`.
  // Expects the navigation error to be of type `expected_error_type`.
  void TestFailedHttpsUpgrade(
      const GURL& url,
      HttpsUpgradeType https_upgrade_type,
      HttpsUpgradeType expected_failed_upgrade_type,
      FailedWebStateObserver::ErrorType expected_error_type) {
    web::NavigationManager::WebLoadParams params(url);
    params.transition_type = ui::PAGE_TRANSITION_TYPED;
    params.https_upgrade_type = https_upgrade_type;

    FailedWebStateObserver observer;
    base::ScopedObservation<WebState, WebStateObserver> scoped_observer(
        &observer);
    scoped_observer.Observe(web_state());
    web_state()->GetNavigationManager()->LoadURLWithParams(params);

    // Need to use a pointer to `observer` as the block wants to capture it by
    // value (even if marked with __block) which would not work.
    FailedWebStateObserver* observer_ptr = &observer;
    EXPECT_TRUE(
        base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
          // Run the event loop, otherwise the HTTPS connection times out
          // instead of failing with an SSL error.
          base::RunLoop().RunUntilIdle();
          return observer_ptr->did_finish() &&
                 (observer_ptr->failed_https_upgrade_type() ==
                  expected_failed_upgrade_type);
        }));
    EXPECT_EQ(expected_error_type, observer.error_type());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServer server_;
  net::EmbeddedTestServer https_server_;
};

// Tests that reloading a page with a different default User Agent updates the
// item.
TEST_F(CRWKNavigationHandlerIntTest, ReloadWithDifferentUserAgent) {
  FakeWebClient* web_client = GetWebClient();
  web_client->SetDefaultUserAgent(UserAgentType::MOBILE);

  ASSERT_TRUE(server_.Start());
  GURL url(server_.GetURL("/echo"));
  ASSERT_TRUE(LoadUrl(url));

  NavigationItem* item = web_state()->GetNavigationManager()->GetVisibleItem();
  EXPECT_EQ(UserAgentType::MOBILE, item->GetUserAgentType());

  web_client->SetDefaultUserAgent(UserAgentType::DESKTOP);

  web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                              /* check_for_repost = */ true);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        NavigationItem* item_after_reload =
            web_state()->GetNavigationManager()->GetVisibleItem();
        return item_after_reload->GetUserAgentType() == UserAgentType::DESKTOP;
      }));
}

// Tests that reloading a failed page that should not have a User Agent doesn't
// trigger a DCHECK (preventing crbug.com/1360567).
TEST_F(CRWKNavigationHandlerIntTest, ReloadNONEUserAgentErrorPage) {
  FakeWebClient* web_client = GetWebClient();
  web_client->SetDefaultUserAgent(UserAgentType::MOBILE);

  GURL url("testwebui://extensions");
  ASSERT_TRUE(LoadUrl(url));

  NavigationItem* item = web_state()->GetNavigationManager()->GetVisibleItem();
  EXPECT_EQ(UserAgentType::NONE, item->GetUserAgentType());

  web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                              /* check_for_repost = */ true);

  // Make sure the load has time to start.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(10));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return !web_state()->IsLoading();
      }));
}

// Tests that an SSL or net error on a navigation that wasn't upgraded to HTTPS
// doesn't set the IsFailedHTTPSUpgrade() bit on the navigation context.
TEST_F(CRWKNavigationHandlerIntTest, FailedHTTPSUpgrade_NotUpgraded_SSLError) {
  ASSERT_TRUE(https_server_.Start());
  GURL url(https_server_.GetURL("/"));
  TestFailedHttpsUpgrade(url, HttpsUpgradeType::kNone, HttpsUpgradeType::kNone,
                         FailedWebStateObserver::ErrorType::kSSLError);
}

// Tests that an SSL error on a navigation that was upgraded to HTTPS
// sets the IsFailedHTTPSUpgrade() bit on the navigation context.
TEST_F(CRWKNavigationHandlerIntTest, FailedHTTPSUpgrade_Upgraded_SSLError) {
  ASSERT_TRUE(https_server_.Start());
  GURL url(https_server_.GetURL("/"));
  TestFailedHttpsUpgrade(url, HttpsUpgradeType::kHttpsOnlyMode,
                         HttpsUpgradeType::kHttpsOnlyMode,
                         FailedWebStateObserver::ErrorType::kSSLError);
}

// Tests that a net error on a navigation that wasn't upgraded to HTTPS
// doesn't set the IsFailedHTTPSUpgrade() bit on the navigation context.
TEST_F(CRWKNavigationHandlerIntTest, FailedHTTPSUpgrade_NotUpgraded_NetError) {
  GURL url("https://site.test");
  TestFailedHttpsUpgrade(url, HttpsUpgradeType::kNone, HttpsUpgradeType::kNone,
                         FailedWebStateObserver::ErrorType::kNetError);
}

// Tests that a net error on a navigation that was upgraded to HTTPS
// sets the IsFailedHTTPSUpgrade() bit on the navigation context.
TEST_F(CRWKNavigationHandlerIntTest, FailedHTTPSUpgrade_Upgraded_NetError) {
  GURL url("https://site.test");
  TestFailedHttpsUpgrade(url, HttpsUpgradeType::kHttpsOnlyMode,
                         HttpsUpgradeType::kHttpsOnlyMode,
                         FailedWebStateObserver::ErrorType::kNetError);
}

}  // namespace web
