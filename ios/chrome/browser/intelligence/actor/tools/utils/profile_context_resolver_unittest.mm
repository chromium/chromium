// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/utils/profile_context_resolver.h"

#import "base/memory/weak_ptr.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class ProfileContextResolverTest : public PlatformTest {
 public:
  ProfileContextResolverTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
    resolver_ = std::make_unique<ProfileContextResolver>(profile_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<ProfileContextResolver> resolver_;
};

// Tests that resolving a valid tab ID in a regular browser returns the correct
// TabResolutionResult.
TEST_F(ProfileContextResolverTest, ResolveValidTab) {
  auto web_state = std::make_unique<web::FakeWebState>();
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  web::WebState* web_state_ptr = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  auto result = resolver_->ResolveTab(tab_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->tab_index, 0);
  EXPECT_EQ(result->web_state.get(), web_state_ptr);
  EXPECT_EQ(result->url_loader.get(),
            UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
}

// Tests that resolving an invalid tab ID returns the target tab not found
// error.
TEST_F(ProfileContextResolverTest, ResolveInvalidTab) {
  auto result = resolver_->ResolveTab(999);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kTabWentAway);
  EXPECT_FALSE(result.error().internal_code().has_value());
}

// Tests that resolving a tab in an incognito browser returns target tab not
// found when include_incognito is false (which is currently hardcoded in
// ResolveTab).
TEST_F(ProfileContextResolverTest, ResolveIncognitoTab) {
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  auto otr_browser = std::make_unique<TestBrowser>(otr_profile);
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  browser_list->AddBrowser(otr_browser.get());

  auto web_state = std::make_unique<web::FakeWebState>();
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  otr_browser->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  auto result = resolver_->ResolveTab(tab_id);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kTabWentAway);
  EXPECT_FALSE(result.error().internal_code().has_value());
}

}  // namespace actor
