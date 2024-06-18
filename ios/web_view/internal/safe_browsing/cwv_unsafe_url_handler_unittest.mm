// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/safe_browsing/cwv_unsafe_url_handler_internal.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/apple/url_conversions.h"
#import "services/network/public/mojom/fetch_api.mojom.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace ios_web_view {

class CWVUnsafeURLHandlerTest : public PlatformTest {
 public:
  CWVUnsafeURLHandlerTest(const CWVUnsafeURLHandlerTest&) = delete;
  CWVUnsafeURLHandlerTest& operator=(const CWVUnsafeURLHandlerTest&) = delete;

 protected:
  CWVUnsafeURLHandlerTest() {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    web_state_.SetNavigationManager(std::move(navigation_manager));

    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
  }

  CWVUnsafeURLHandler* CreateHandler(
      const GURL& request_url,
      safe_browsing::SBThreatType threat_type,
      base::OnceCallback<void(NSString*)> callback) {
    security_interstitials::UnsafeResource unsafe_resource;
    unsafe_resource.url = request_url;
    unsafe_resource.threat_type = threat_type;
    unsafe_resource.weak_web_state = web_state_.GetWeakPtr();
    return [[CWVUnsafeURLHandler alloc] initWithWebState:&web_state_
                                          unsafeResource:unsafe_resource
                                            htmlCallback:std::move(callback)];
  }

  web::FakeNavigationManager* GetNavigationManager() {
    return static_cast<web::FakeNavigationManager*>(
        web_state_.GetNavigationManager());
  }

  web::FakeWebState web_state_;
};

// Checks that public API agrees with the internal UnsafeResource for unsafe
// loads.
TEST_F(CWVUnsafeURLHandlerTest, InitializationForUnsafeResource) {
  auto request_url = GURL("https://www.chromium.org");
  CWVUnsafeURLHandler* handler = CreateHandler(
      request_url, safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING,
      base::DoNothing());
  EXPECT_EQ(request_url, net::GURLWithNSURL(handler.mainFrameURL));
  EXPECT_EQ(request_url, net::GURLWithNSURL(handler.requestURL));
  EXPECT_EQ(CWVUnsafeURLThreatTypeBilling, handler.threatType);
}

// Tests that html callback is only invoked once.
TEST_F(CWVUnsafeURLHandlerTest, DisplayHTMLCallbackIsOnlyCalledOnce) {
  __block NSString* callback_html = nil;
  __block int callback_count = 0;
  base::OnceCallback callback = base::BindOnce(^(NSString* html) {
    ++callback_count;
    callback_html = html;
  });
  CWVUnsafeURLHandler* handler = CreateHandler(
      GURL(), safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      std::move(callback));

  [handler displayInterstitialPageWithHTML:@"foo"];
  EXPECT_NSEQ(@"foo", callback_html);
  EXPECT_EQ(1, callback_count);

  // Second call should be a no op.
  [handler displayInterstitialPageWithHTML:@"bar"];
  EXPECT_NSEQ(@"foo", callback_html);
  EXPECT_EQ(1, callback_count);
}

// Tests that proceeding will update allow list and reload the web state.
TEST_F(CWVUnsafeURLHandlerTest, ProceedingUpdatesAllowListAndReloadsWebState) {
  auto request_url = GURL("https://www.chromium.org");
  auto threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING;
  CWVUnsafeURLHandler* handler =
      CreateHandler(request_url, threat_type, base::DoNothing());
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  allow_list->AddPendingUnsafeNavigationDecision(request_url, threat_type);

  [handler proceed];
  EXPECT_TRUE(GetNavigationManager()->ReloadWasCalled());
  EXPECT_TRUE(allow_list->AreUnsafeNavigationsAllowed(request_url));
  EXPECT_FALSE(allow_list->IsUnsafeNavigationDecisionPending(request_url));
}

// Tests that going back will close the web state.
TEST_F(CWVUnsafeURLHandlerTest, GoingBackClosesWebState) {
  auto request_url = GURL("https://www.chromium.org");
  auto threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING;
  CWVUnsafeURLHandler* handler =
      CreateHandler(request_url, threat_type, base::DoNothing());
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  allow_list->AddPendingUnsafeNavigationDecision(request_url, threat_type);

  [handler goBack];
  EXPECT_FALSE(allow_list->AreUnsafeNavigationsAllowed(request_url));
  EXPECT_FALSE(allow_list->IsUnsafeNavigationDecisionPending(request_url));
  EXPECT_TRUE(web_state_.IsClosed());
}

// Tests that going back will navigate backwards if possible.
TEST_F(CWVUnsafeURLHandlerTest, GoingBackNavigatesBack) {
  GetNavigationManager()->AddItem(GURL("https://www.example.com"),
                                  ui::PAGE_TRANSITION_TYPED);
  GetNavigationManager()->AddItem(GURL("https://www.example2.com"),
                                  ui::PAGE_TRANSITION_TYPED);

  auto request_url = GURL("https://www.chromium.org");
  auto threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING;
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  allow_list->AddPendingUnsafeNavigationDecision(request_url, threat_type);
  CWVUnsafeURLHandler* handler =
      CreateHandler(request_url, threat_type, base::DoNothing());
  EXPECT_TRUE(GetNavigationManager()->CanGoBack());
  int item_index = GetNavigationManager()->GetLastCommittedItemIndex();
  [handler goBack];
  EXPECT_FALSE(allow_list->IsUnsafeNavigationDecisionPending(request_url));
  EXPECT_EQ(item_index - 1,
            GetNavigationManager()->GetLastCommittedItemIndex());
}

// Tests that deallocation will remove a pending decision.
TEST_F(CWVUnsafeURLHandlerTest, DeallocationRemovesPendingDecision) {
  auto request_url = GURL("https://www.chromium.org");
  auto threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING;
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  // Manually creating an autorelease pool will ensure handler is released
  // before the end of the test.
  @autoreleasepool {
    __unused CWVUnsafeURLHandler* handler =
        CreateHandler(request_url, threat_type, base::DoNothing());
    allow_list->AddPendingUnsafeNavigationDecision(request_url, threat_type);
    EXPECT_TRUE(allow_list->IsUnsafeNavigationDecisionPending(request_url));
  }
  EXPECT_FALSE(allow_list->IsUnsafeNavigationDecisionPending(request_url));
}

}  // namespace ios_web_view
