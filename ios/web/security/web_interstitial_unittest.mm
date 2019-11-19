// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/security/web_interstitial.h"

#include <memory>

#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/test/fakes/test_web_state_observer.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/test/fakes/mock_interstitial_delegate.h"
#import "ios/web/web_state/web_state_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

const char kTestHostName[] = "https://chromium.test/";

}  // namespace

// Test fixture for WebInterstitial class.
class WebInterstitialTest : public WebTest {
 protected:
  void SetUp() override {
    WebTest::SetUp();
    WebState::CreateParams params(GetBrowserState());
    web_state_ = std::make_unique<WebStateImpl>(params);
    web_state_->GetNavigationManagerImpl().InitializeSession();

    // Transient item can only be added for pending non-app-specific loads.
    web_state_->GetNavigationManagerImpl().AddPendingItem(
        GURL(kTestHostName), Referrer(),
        ui::PageTransition::PAGE_TRANSITION_TYPED,
        NavigationInitiationType::BROWSER_INITIATED,
        NavigationManager::UserAgentOverrideOption::INHERIT);
  }

  std::unique_ptr<WebStateImpl> web_state_;
};

// Tests that the interstitial is shown and dismissed on Proceed call.
TEST_F(WebInterstitialTest, Proceed) {
  ASSERT_FALSE(web_state_->IsShowingWebInterstitial());

  GURL url(kTestHostName);
  std::unique_ptr<MockInterstitialDelegate> delegate =
      std::make_unique<MockInterstitialDelegate>();
  EXPECT_CALL(*delegate.get(), OnProceed());

  // Raw pointer to |interstitial| because it deletes itself when dismissed.
  WebInterstitial* interstitial = WebInterstitial::CreateInterstitial(
      web_state_.get(), true, url, std::move(delegate));
  interstitial->Show();
  ASSERT_TRUE(web_state_->IsShowingWebInterstitial());

  interstitial->Proceed();
  EXPECT_FALSE(web_state_->IsShowingWebInterstitial());
}

// Tests that the interstitial is shown and dismissed on DontProceed call.
TEST_F(WebInterstitialTest, DontProceed) {
  ASSERT_FALSE(web_state_->IsShowingWebInterstitial());

  std::unique_ptr<MockInterstitialDelegate> delegate =
      std::make_unique<MockInterstitialDelegate>();
  EXPECT_CALL(*delegate.get(), OnDontProceed());

  // Raw pointer to |interstitial| because it deletes itself when dismissed.
  WebInterstitial* interstitial = WebInterstitial::CreateInterstitial(
      web_state_.get(), true, GURL(kTestHostName), std::move(delegate));
  interstitial->Show();
  ASSERT_TRUE(web_state_->IsShowingWebInterstitial());

  interstitial->DontProceed();
  EXPECT_FALSE(web_state_->IsShowingWebInterstitial());
}

// Tests that presenting an interstitial changes the visible security state.
TEST_F(WebInterstitialTest, VisibleSecurityStateChanged) {
  TestWebStateObserver observer(web_state_.get());

  std::unique_ptr<MockInterstitialDelegate> delegate =
      std::make_unique<MockInterstitialDelegate>();
  // Raw pointer to |interstitial| because it deletes itself when dismissed.
  WebInterstitial* interstitial = WebInterstitial::CreateInterstitial(
      web_state_.get(), true, GURL(kTestHostName), std::move(delegate));

  interstitial->Show();
  ASSERT_TRUE(observer.did_change_visible_security_state_info());
  EXPECT_TRUE(web_state_->IsShowingWebInterstitial());

  interstitial->DontProceed();
}

// Tests that the interstitial is dismissed when the web state is destroyed.
TEST_F(WebInterstitialTest, WebStateDestroyed) {
  std::unique_ptr<MockInterstitialDelegate> delegate =
      std::make_unique<MockInterstitialDelegate>();
  // Interstitial should be dismissed if web state is destroyed.
  EXPECT_CALL(*delegate.get(), OnDontProceed());

  // Raw pointer to |interstitial| because it deletes itself when dismissed.
  WebInterstitial* interstitial = WebInterstitial::CreateInterstitial(
      web_state_.get(), true, GURL(kTestHostName), std::move(delegate));

  interstitial->Show();
  EXPECT_TRUE(web_state_->IsShowingWebInterstitial());

  // Simulate a destroyed web state.
  web_state_.reset();
}

}  // namespace web
