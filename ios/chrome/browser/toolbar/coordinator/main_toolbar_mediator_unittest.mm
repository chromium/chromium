// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/main_toolbar_mediator.h"

#import "base/test/task_environment.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FakeMainToolbarMediatorDelegate
    : NSObject <MainToolbarMediatorDelegate>
@property(nonatomic, assign) BOOL onDidChangeOmniboxPositionCalled;
@end

@implementation FakeMainToolbarMediatorDelegate

- (void)mainToolbarMediatorDidChangeOmniboxPosition:
    (MainToolbarMediator*)mediator {
  self.onDidChangeOmniboxPositionCalled = YES;
}

@end

class MainToolbarMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(omnibox::kIsOmniboxInBottomPosition,
                                            false);

    delegate_ = [[FakeMainToolbarMediatorDelegate alloc] init];
    mediator_ = [[MainToolbarMediator alloc] initWithPrefService:prefs_.get()];
    mediator_.delegate = delegate_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  FakeMainToolbarMediatorDelegate* delegate_;
  MainToolbarMediator* mediator_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests that the mediator correctly reports the omnibox position and notifies
// the delegate when it changes.
TEST_F(MainToolbarMediatorTest, TestPrefChangeNotifiesDelegate) {
  EXPECT_FALSE([mediator_ isOmniboxInBottomPosition]);
  EXPECT_FALSE(delegate_.onDidChangeOmniboxPositionCalled);

  prefs_->SetBoolean(omnibox::kIsOmniboxInBottomPosition, true);

  EXPECT_TRUE(delegate_.onDidChangeOmniboxPositionCalled);
  EXPECT_TRUE([mediator_ isOmniboxInBottomPosition] ||
              !IsBottomOmniboxAvailable());
}
