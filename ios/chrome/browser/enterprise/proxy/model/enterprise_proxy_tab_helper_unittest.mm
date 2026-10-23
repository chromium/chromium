// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "components/enterprise/net/core/enterprise_proxy_error_data.h"
#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/log/net_log_with_source.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

constexpr char kStartURL[] = "https://start.example.com/";
constexpr char kRedirectURL[] = "https://redirect.example.com/";
constexpr char kOtherURL[] = "https://other.example.com/";
constexpr char kProxyURL[] = "https://proxy.example.com/";
constexpr int kDisguisedErrorCode = 403;

}  // namespace

class EnterpriseProxyTabHelperTest : public PlatformTest {
 protected:
  EnterpriseProxyTabHelperTest() {
    EnterpriseProxyTabHelper::CreateForWebState(&web_state_, &error_service_);
    tab_helper_ = EnterpriseProxyTabHelper::FromWebState(&web_state_);
  }

  // Returns a cross-document navigation context for `url`.
  std::unique_ptr<web::FakeNavigationContext> CreateNavigationContext(
      const GURL& url) {
    auto context = std::make_unique<web::FakeNavigationContext>();
    context->SetWebState(&web_state_);
    context->SetUrl(url);
    context->SetIsSameDocument(false);
    return context;
  }

  // Records a disguised error for `navigation_id` in `error_service_`.
  void RecordDisguisedError(int64_t navigation_id) {
    error_service_.RecordDisguisedError(
        navigation_id,
        enterprise_net::EnterpriseProxyErrorData(
            GURL(kStartURL), GURL(kProxyURL), kDisguisedErrorCode),
        net::NetLogWithSource());
  }

  testing::NiceMock<enterprise_net::MockEnterpriseProxyService> proxy_service_;
  enterprise_net::EnterpriseProxyErrorService error_service_{&proxy_service_};
  web::FakeWebState web_state_;
  raw_ptr<EnterpriseProxyTabHelper> tab_helper_ = nullptr;
};

// Test that no navigation is pending before any navigation starts.
TEST_F(EnterpriseProxyTabHelperTest, NoPendingNavigationInitially) {
  EXPECT_FALSE(tab_helper_->pending_navigation_id().has_value());
  EXPECT_TRUE(tab_helper_->pending_navigation_url().is_empty());
}

// Test that starting a cross-document navigation records its ID and URL.
TEST_F(EnterpriseProxyTabHelperTest, TracksNavigationOnStart) {
  std::unique_ptr<web::FakeNavigationContext> context =
      CreateNavigationContext(GURL(kStartURL));
  web_state_.OnNavigationStarted(context.get());

  EXPECT_EQ(tab_helper_->pending_navigation_id(), context->GetNavigationId());
  EXPECT_EQ(tab_helper_->pending_navigation_url(), GURL(kStartURL));
}

// Test that same-document navigations are not tracked.
TEST_F(EnterpriseProxyTabHelperTest, IgnoresSameDocumentNavigation) {
  std::unique_ptr<web::FakeNavigationContext> context =
      CreateNavigationContext(GURL(kStartURL));
  context->SetIsSameDocument(true);
  web_state_.OnNavigationStarted(context.get());

  EXPECT_FALSE(tab_helper_->pending_navigation_id().has_value());
  EXPECT_TRUE(tab_helper_->pending_navigation_url().is_empty());
}

// Test that a server redirect updates the pending URL and keeps the ID.
TEST_F(EnterpriseProxyTabHelperTest, FollowsRedirects) {
  std::unique_ptr<web::FakeNavigationContext> context =
      CreateNavigationContext(GURL(kStartURL));
  web_state_.OnNavigationStarted(context.get());

  context->SetUrl(GURL(kRedirectURL));
  web_state_.OnNavigationRedirected(context.get());

  EXPECT_EQ(tab_helper_->pending_navigation_id(), context->GetNavigationId());
  EXPECT_EQ(tab_helper_->pending_navigation_url(), GURL(kRedirectURL));
}

// Test that finishing the pending navigation drops its ID and URL.
TEST_F(EnterpriseProxyTabHelperTest, DropsNavigationOnFinish) {
  std::unique_ptr<web::FakeNavigationContext> context =
      CreateNavigationContext(GURL(kStartURL));
  web_state_.OnNavigationStarted(context.get());
  web_state_.OnNavigationFinished(context.get());

  EXPECT_FALSE(tab_helper_->pending_navigation_id().has_value());
  EXPECT_TRUE(tab_helper_->pending_navigation_url().is_empty());
}

// Test that a newer navigation replaces the pending one, and that finishing
// or redirecting the older navigation does not affect the newer one.
TEST_F(EnterpriseProxyTabHelperTest, IgnoresStaleNavigationEvents) {
  std::unique_ptr<web::FakeNavigationContext> first_context =
      CreateNavigationContext(GURL(kStartURL));
  web_state_.OnNavigationStarted(first_context.get());

  std::unique_ptr<web::FakeNavigationContext> second_context =
      CreateNavigationContext(GURL(kOtherURL));
  web_state_.OnNavigationStarted(second_context.get());

  first_context->SetUrl(GURL(kRedirectURL));
  web_state_.OnNavigationRedirected(first_context.get());
  web_state_.OnNavigationFinished(first_context.get());

  EXPECT_EQ(tab_helper_->pending_navigation_id(),
            second_context->GetNavigationId());
  EXPECT_EQ(tab_helper_->pending_navigation_url(), GURL(kOtherURL));
}

// Test that finishing a navigation drops the disguised error recorded for it,
// including a navigation that was replaced by a newer one.
TEST_F(EnterpriseProxyTabHelperTest, RemovesRecordedErrorOnFinish) {
  std::unique_ptr<web::FakeNavigationContext> first_context =
      CreateNavigationContext(GURL(kStartURL));
  web_state_.OnNavigationStarted(first_context.get());
  std::unique_ptr<web::FakeNavigationContext> second_context =
      CreateNavigationContext(GURL(kOtherURL));
  web_state_.OnNavigationStarted(second_context.get());
  RecordDisguisedError(first_context->GetNavigationId());
  RecordDisguisedError(second_context->GetNavigationId());

  web_state_.OnNavigationFinished(first_context.get());
  web_state_.OnNavigationFinished(second_context.get());

  EXPECT_FALSE(
      error_service_.TakeDisguisedError(first_context->GetNavigationId())
          .has_value());
  EXPECT_FALSE(
      error_service_.TakeDisguisedError(second_context->GetNavigationId())
          .has_value());
}

// Test that destroying the WebState drops the disguised error recorded for its
// pending navigation, which then never finishes.
TEST_F(EnterpriseProxyTabHelperTest, RemovesPendingNavigationErrorOnDestroy) {
  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  EnterpriseProxyTabHelper::CreateForWebState(web_state.get(), &error_service_);
  web::FakeNavigationContext context;
  context.SetUrl(GURL(kStartURL));
  context.SetIsSameDocument(false);
  web_state->OnNavigationStarted(&context);
  RecordDisguisedError(context.GetNavigationId());

  web_state.reset();

  EXPECT_FALSE(
      error_service_.TakeDisguisedError(context.GetNavigationId()).has_value());
}
