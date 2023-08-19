// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/security/security_style.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/session_certificate_policy_cache.h"
#import "ios/web/public/test/error_test_util.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state_observer.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_state.h"
#import "net/cert/x509_certificate.h"
#import "net/ssl/ssl_info.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

// Test fixture for loading https pages with self signed certificate.
class BadSslResponseTest : public WebTestWithWebState {
 public:
  BadSslResponseTest(const BadSslResponseTest&) = delete;
  BadSslResponseTest& operator=(const BadSslResponseTest&) = delete;

 protected:
  BadSslResponseTest()
      : WebTestWithWebState(std::make_unique<FakeWebClient>()),
        https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    RegisterDefaultHandlers(&https_server_);
  }

  void SetUp() override {
    WebTestWithWebState::SetUp();

    web_state_observer_ = std::make_unique<FakeWebStateObserver>(web_state());
    ASSERT_TRUE(https_server_.Start());
  }

  FakeWebClient* web_client() {
    return static_cast<FakeWebClient*>(GetWebClient());
  }

  TestDidChangeVisibleSecurityStateInfo* security_state_info() {
    return web_state_observer_->did_change_visible_security_state_info();
  }

  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeWebStateObserver> web_state_observer_;
};

// Tests that an error page is shown for SSL cert errors when committed
// interstitials are enabled.
TEST_F(BadSslResponseTest, ShowSSLErrorPageCommittedInterstitial) {
  const GURL url = https_server_.GetURL("/");
  test::LoadUrl(web_state(), url);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !web_state()->IsLoading();
  }));
  NSError* error = testing::CreateErrorWithUnderlyingErrorChain(
      {{@"NSURLErrorDomain", NSURLErrorServerCertificateUntrusted},
       {@"kCFErrorDomainCFNetwork", kCFURLErrorServerCertificateUntrusted},
       {net::kNSErrorDomain, net::ERR_CERT_AUTHORITY_INVALID}});
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), testing::GetErrorText(
                       web_state(), url, error,
                       /*is_post=*/false, /*is_otr=*/false,
                       /*cert_status=*/net::CERT_STATUS_AUTHORITY_INVALID)));
  ASSERT_TRUE(security_state_info());
  ASSERT_TRUE(security_state_info()->visible_ssl_status);
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN,
            security_state_info()->visible_ssl_status->security_style);
}

}  // namespace web
