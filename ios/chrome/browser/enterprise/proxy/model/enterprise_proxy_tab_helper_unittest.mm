// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

constexpr char kStartURL[] = "https://start.example.com/";
constexpr char kRedirectURL[] = "https://redirect.example.com/";
constexpr char kOtherURL[] = "https://other.example.com/";

}  // namespace

class EnterpriseProxyTabHelperTest : public PlatformTest {
 protected:
  EnterpriseProxyTabHelperTest() {
    EnterpriseProxyTabHelper::CreateForWebState(&web_state_);
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
