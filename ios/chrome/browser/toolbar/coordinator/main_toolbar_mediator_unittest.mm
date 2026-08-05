// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/main_toolbar_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/browser_layout_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface MainToolbarMediator (Testing)
- (BOOL)isBottomOmniboxPrefEnabled;
@end

class MainToolbarMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    feature_list_.InitAndEnableFeature(kChromeNextIa);
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(omnibox::kIsOmniboxInBottomPosition,
                                            false);

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    browser_layout_state_ = browser_->GetBrowserLayoutState();

    mediator_ =
        [[MainToolbarMediator alloc] initWithPrefService:prefs_.get()
                                      browserLayoutState:browser_layout_state_];
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<TestBrowser> browser_;
  __weak BrowserLayoutState* browser_layout_state_;
  MainToolbarMediator* mediator_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the mediator correctly reports the omnibox position and updates
// the browser layout state when it changes.
TEST_F(MainToolbarMediatorTest, TestPrefChangeUpdatesLayoutState) {
  EXPECT_FALSE([mediator_ isBottomOmniboxPrefEnabled]);
  EXPECT_EQ(browser_layout_state_.toolbarPosition, ToolbarPosition::kTop);

  prefs_->SetBoolean(omnibox::kIsOmniboxInBottomPosition, true);

  EXPECT_EQ(browser_layout_state_.toolbarPosition,
            IsBottomOmniboxAvailable() ? ToolbarPosition::kBottom
                                       : ToolbarPosition::kTop);
  EXPECT_TRUE([mediator_ isBottomOmniboxPrefEnabled] ||
              !IsBottomOmniboxAvailable());
}
