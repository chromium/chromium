// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/scene_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/feature_engagement/test/scoped_iph_feature_list.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/scene/ui/scene_consumer.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@protocol TestSceneConsumer <SceneConsumer, FullscreenUIElement>
@end

@interface FakeSceneConsumer : NSObject <TestSceneConsumer>
@property(nonatomic, assign) BOOL showNewIAPromoCalled;
@property(nonatomic, assign) BOOL eligibleForGemini;
@end

@implementation FakeSceneConsumer
- (void)showNewIAPromoWithGeminiEligibility:(BOOL)eligible {
  self.showNewIAPromoCalled = YES;
  self.eligibleForGemini = eligible;
}
- (void)updateForFullscreenProgress:(CGFloat)progress {
}
- (void)updateForFullscreenMinViewportInsets:(UIEdgeInsets)minViewportInsets
                           maxViewportInsets:(UIEdgeInsets)maxViewportInsets {
}
- (void)updateForFullscreenEnabled:(BOOL)enabled {
}
- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
}
@end

@interface SceneMediator (Test)
- (void)triggerNewIAPromo;
@end

namespace {

ACTION_P(RunInitializedCallback, success) {
  std::move(const_cast<base::OnceCallback<void(bool)>&>(arg0)).Run(success);
}

class SceneMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    TestFullscreenController::CreateForBrowser(browser_.get());
    FullscreenController* fullscreen_controller =
        TestFullscreenController::FromBrowser(browser_.get());

    mediator_ = [[SceneMediator alloc]
        initWithRegularFullscreenController:fullscreen_controller
              incognitoFullscreenController:nullptr];

    tracker_ = feature_engagement::CreateTestTracker();
    mediator_.tracker = tracker_.get();

    consumer_ = OCMProtocolMock(@protocol(TestSceneConsumer));
    mediator_.consumer = consumer_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  SceneMediator* mediator_;
  std::unique_ptr<feature_engagement::Tracker> tracker_;
  id<TestSceneConsumer> consumer_;
};

// Tests that the new IA IPH promo is triggered when the conditions are met.
TEST_F(SceneMediatorTest, TestTriggerNewIAPromo) {
  // Enable the feature.
  feature_engagement::test::ScopedIphFeatureList feature_list;
  feature_list.InitAndEnableFeatures(
      {feature_engagement::kIPHiOSNewIAPromoFeature});
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kChromeNextIa, kComposeboxIpad}, {});

  mediator_.appBarPositionAtLaunch = AppBarPosition::kBottom;

  // Create a mock tracker.
  auto mock_tracker = std::make_unique<feature_engagement::test::MockTracker>();

  // Set up expectations on the mock tracker.
  EXPECT_CALL(*mock_tracker, AddOnInitializedCallback(testing::_))
      .WillRepeatedly(RunInitializedCallback(true));
  EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(testing::Ref(
                                 feature_engagement::kIPHiOSNewIAPromoFeature)))
      .WillOnce(testing::Return(true))
      .WillRepeatedly(testing::Return(false));

  // Inject the mock tracker into the mediator.
  mediator_.tracker = mock_tracker.get();

  // Create a fake consumer.
  FakeSceneConsumer* fake_consumer = [[FakeSceneConsumer alloc] init];
  mediator_.consumer = fake_consumer;

  // Trigger the promo.
  [mediator_ triggerNewIAPromo];

  // Wait for the consumer to be called.
  bool success =
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(0.5), ^bool {
        return fake_consumer.showNewIAPromoCalled;
      });

  EXPECT_TRUE(success);
}

// Tests that the new IA IPH promo is NOT triggered when the position is kNone.
TEST_F(SceneMediatorTest, TestTriggerNewIAPromo_PositionNone) {
  // Enable the feature.
  feature_engagement::test::ScopedIphFeatureList feature_list;
  feature_list.InitAndEnableFeatures(
      {feature_engagement::kIPHiOSNewIAPromoFeature});
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kChromeNextIa, kComposeboxIpad}, {});

  // Create a mock tracker.
  auto mock_tracker = std::make_unique<feature_engagement::test::MockTracker>();

  // Set up expectations on the mock tracker.
  EXPECT_CALL(*mock_tracker, AddOnInitializedCallback(testing::_))
      .WillRepeatedly(RunInitializedCallback(true));

  // ShouldTriggerHelpUI should NOT be called because position check fails
  // first.
  EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(testing::_)).Times(0);

  // Inject the mock tracker into the mediator.
  mediator_.tracker = mock_tracker.get();

  // Set position to None.
  mediator_.appBarPositionAtLaunch = AppBarPosition::kNone;

  // Create a fake consumer.
  FakeSceneConsumer* fake_consumer = [[FakeSceneConsumer alloc] init];
  mediator_.consumer = fake_consumer;

  // Trigger the promo.
  [mediator_ triggerNewIAPromo];

  // Wait for the consumer to be called (which shouldn't happen).
  bool success =
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(0.5), ^bool {
        return fake_consumer.showNewIAPromoCalled;
      });
  EXPECT_FALSE(success);
}

}  // namespace
