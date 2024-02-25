// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_lookalike_url_handler_internal.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/apple/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace ios_web_view {

class CWVLookalikeURLHandlerTest : public PlatformTest {
 public:
  CWVLookalikeURLHandlerTest(const CWVLookalikeURLHandlerTest&) = delete;
  CWVLookalikeURLHandlerTest& operator=(const CWVLookalikeURLHandlerTest&) =
      delete;

 protected:
  CWVLookalikeURLHandlerTest() {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    web_state_.SetNavigationManager(std::move(navigation_manager));

    LookalikeUrlTabAllowList::CreateForWebState(&web_state_);
  }

  CWVLookalikeURLHandler* GetHandler(
      const GURL& safe_url,
      const GURL& request_url,
      base::OnceCallback<void(NSString*)> callback) {
    auto url_info = std::make_unique<LookalikeUrlContainer::LookalikeUrlInfo>(
        safe_url, request_url,
        lookalikes::LookalikeUrlMatchType::kSkeletonMatchSiteEngagement);
    return
        [[CWVLookalikeURLHandler alloc] initWithWebState:&web_state_
                                        lookalikeURLInfo:std::move(url_info)
                                            htmlCallback:std::move(callback)];
  }

  web::FakeNavigationManager* GetNavigationManager() {
    return static_cast<web::FakeNavigationManager*>(
        web_state_.GetNavigationManager());
  }

  web::FakeWebState web_state_;
};

TEST_F(CWVLookalikeURLHandlerTest, ValidSafeURL) {
  GURL safe_url = GURL("https://www.chromium.org");
  GURL request_url = GURL("https://www.chr0mium.org");
  CWVLookalikeURLHandler* handler =
      GetHandler(safe_url, request_url, base::DoNothing());
  EXPECT_EQ(request_url, net::GURLWithNSURL(handler.requestURL));
  EXPECT_EQ(safe_url, net::GURLWithNSURL(handler.safeURL));
}

TEST_F(CWVLookalikeURLHandlerTest, InvalidSafeURL) {
  GURL safe_url = GURL();
  GURL request_url = GURL("https://www.chr0mium.org");
  CWVLookalikeURLHandler* handler =
      GetHandler(safe_url, request_url, base::DoNothing());
  EXPECT_EQ(request_url, net::GURLWithNSURL(handler.requestURL));
  EXPECT_FALSE(handler.safeURL);
}

TEST_F(CWVLookalikeURLHandlerTest, DisplayHTMLCallback) {
  __block NSString* callback_html = nil;
  __block int callback_count = 0;
  base::OnceCallback callback = base::BindOnce(^(NSString* html) {
    ++callback_count;
    callback_html = html;
  });
  CWVLookalikeURLHandler* handler =
      GetHandler(GURL(), GURL(), std::move(callback));

  [handler displayInterstitialPageWithHTML:@"foo"];
  EXPECT_NSEQ(@"foo", callback_html);
  EXPECT_EQ(1, callback_count);

  // Second call should be a no op.
  [handler displayInterstitialPageWithHTML:@"bar"];
  EXPECT_NSEQ(@"foo", callback_html);
  EXPECT_EQ(1, callback_count);
}

TEST_F(CWVLookalikeURLHandlerTest, ProceedToRequestURL) {
  GURL safe_url = GURL("https://www.chromium.org");
  GURL request_url = GURL("https://www.chr0mium.org");
  CWVLookalikeURLHandler* handler =
      GetHandler(safe_url, request_url, base::DoNothing());
  BOOL result = [handler
      commitDecision:CWVLookalikeURLHandlerDecisionProceedToRequestURL];
  EXPECT_TRUE(result);
  EXPECT_TRUE(GetNavigationManager()->ReloadWasCalled());
  LookalikeUrlTabAllowList* allow_list =
      LookalikeUrlTabAllowList::FromWebState(&web_state_);
  EXPECT_TRUE(allow_list->IsDomainAllowed(request_url.host()));
}

TEST_F(CWVLookalikeURLHandlerTest, ProceedToInvalidSafeURL) {
  GURL safe_url = GURL();
  GURL request_url = GURL("https://www.chr0mium.org");
  CWVLookalikeURLHandler* handler =
      GetHandler(safe_url, request_url, base::DoNothing());
  BOOL result =
      [handler commitDecision:CWVLookalikeURLHandlerDecisionProceedToSafeURL];
  EXPECT_FALSE(result);
}

TEST_F(CWVLookalikeURLHandlerTest, ProceedToValidSafeURL) {
  GURL safe_url = GURL("https://www.chromium.org");
  GURL request_url = GURL("https://www.chr0mium.org");
  CWVLookalikeURLHandler* handler =
      GetHandler(safe_url, request_url, base::DoNothing());
  BOOL result =
      [handler commitDecision:CWVLookalikeURLHandlerDecisionProceedToSafeURL];
  EXPECT_TRUE(result);
}

TEST_F(CWVLookalikeURLHandlerTest, GoBack) {
  GetNavigationManager()->AddItem(GURL("https://www.example.com"),
                                  ui::PAGE_TRANSITION_TYPED);
  GetNavigationManager()->AddItem(GURL("https://www.example2.com"),
                                  ui::PAGE_TRANSITION_TYPED);

  GURL safe_url = GURL("https://www.chromium.org");
  GURL request_url = GURL("https://www.chr0mium.org");
  CWVLookalikeURLHandler* handler =
      GetHandler(safe_url, request_url, base::DoNothing());
  EXPECT_TRUE(GetNavigationManager()->CanGoBack());
  int item_index = GetNavigationManager()->GetLastCommittedItemIndex();
  BOOL result =
      [handler commitDecision:CWVLookalikeURLHandlerDecisionGoBackOrClose];
  EXPECT_TRUE(result);
  EXPECT_EQ(item_index - 1,
            GetNavigationManager()->GetLastCommittedItemIndex());
}

TEST_F(CWVLookalikeURLHandlerTest, Close) {
  GURL safe_url = GURL("https://www.chromium.org");
  GURL request_url = GURL("https://www.chr0mium.org");
  CWVLookalikeURLHandler* handler =
      GetHandler(safe_url, request_url, base::DoNothing());
  BOOL result =
      [handler commitDecision:CWVLookalikeURLHandlerDecisionGoBackOrClose];
  EXPECT_TRUE(result);
  EXPECT_TRUE(web_state_.IsClosed());
}

}  // ios_web_view
