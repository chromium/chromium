// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/guided_tour/coordinator/guided_tour_coordinator.h"

#import <memory>

#import "base/ios/block_types.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/first_run/guided_tour/coordinator/guided_tour_coordinator+Testing.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface FakeTabGridToolbarCommands : NSObject <TabGridToolbarCommands>
@property(nonatomic, copy) ProceduralBlock dismissalCompletion;
@end

@implementation FakeTabGridToolbarCommands
- (void)showGuidedTourIncognitoStepWithDismissalCompletion:
    (ProceduralBlock)completion {
  self.dismissalCompletion = completion;
}
- (void)showGuidedTourTabGroupStepWithDismissalCompletion:
    (ProceduralBlock)completion {
  self.dismissalCompletion = completion;
}
- (void)showSavedTabGroupIPH {
}
- (void)hideTabGridToolbarGuidedTour {
}
@end

@interface FakeTabGridCommands : NSObject <TabGridCommands>
@property(nonatomic, copy) ProceduralBlock dismissalCompletion;
@end

@implementation FakeTabGridCommands
- (void)showGuidedTourLongPressStepWithDismissalCompletion:
    (ProceduralBlock)completion {
  self.dismissalCompletion = completion;
}
- (void)bringGroupIntoView:(const TabGroup*)group animated:(BOOL)animated {
}
- (void)showHistoryForText:(NSString*)text {
}
- (void)showWebSearchForText:(NSString*)text {
}
- (void)showPage:(TabGridPage)page animated:(BOOL)animated {
}
- (void)prepareToExitTabGrid {
}
- (void)exitTabGrid {
}
- (void)hideTabGridGuidedTour {
}
- (void)presentPinTabBubble {
}
- (void)presentCreateTabGroupBubble {
}
- (void)showSwipeToIncognitoIPH {
}
- (void)showPageActionMenuFromTabGrid {
}
- (void)activateGridContainerConstraints {
}
- (void)deactivateGridContainerConstraints {
}
@end

// Tests `GuidedTourCoordinator`.
class GuidedTourCoordinatorTest : public PlatformTest {
 public:
  GuidedTourCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[SceneState alloc] init];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    base_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[GuidedTourCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
  }

  ~GuidedTourCoordinatorTest() override {
    [coordinator_ stop];
    coordinator_ = nil;
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList enabled_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  GuidedTourCoordinator* coordinator_;
};

// Tests that the correct metric is logged when the user rejects the Guided Tour
// promo.
TEST_F(GuidedTourCoordinatorTest, GuidedTourPromoMetrics) {
  enabled_feature_list_.InitAndEnableFeatureWithParameters(kBestOfAppFRE,
                                                           {{"variant", "4"}});
  base::HistogramTester tester;
  __block BOOL completion_called = NO;
  __block BOOL tour_started = YES;
  coordinator_.completion = ^(BOOL guidedTourStarted) {
    completion_called = YES;
    tour_started = guidedTourStarted;
  };
  [coordinator_ dismissGuidedTourPromo];
  tester.ExpectUniqueSample("IOS.GuidedTour.Promo.DidAccept", false, 1);
  EXPECT_TRUE(completion_called);
  EXPECT_FALSE(tour_started);
}

// Tests that the correct metric is logged when each step in the Guided Tour
// finishes.
TEST_F(GuidedTourCoordinatorTest, GuidedTourStepMetrics) {
  enabled_feature_list_.InitAndEnableFeatureWithParameters(kBestOfAppFRE,
                                                           {{"variant", "4"}});

  FakeTabGridToolbarCommands* fake_tab_grid_toolbar_commands =
      [[FakeTabGridToolbarCommands alloc] init];
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:fake_tab_grid_toolbar_commands
                   forProtocol:@protocol(TabGridToolbarCommands)];

  FakeTabGridCommands* fake_tab_grid_commands =
      [[FakeTabGridCommands alloc] init];
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:fake_tab_grid_commands
                   forProtocol:@protocol(TabGridCommands)];

  id scene_commands_handler = OCMProtocolMock(@protocol(SceneCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:scene_commands_handler
                   forProtocol:@protocol(SceneCommands)];

  id guided_tour_handler = OCMProtocolMock(@protocol(GuidedTourCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:guided_tour_handler
                   forProtocol:@protocol(GuidedTourCommands)];

  __block BOOL completion_called = NO;
  __block BOOL tour_started = NO;
  coordinator_.completion = ^(BOOL guidedTourStarted) {
    completion_called = YES;
    tour_started = guidedTourStarted;
  };

  base::HistogramTester tester;

  // Start and complete step 1 (NTP).
  [coordinator_ startGuidedTour];
  [coordinator_ guidedTourNTPStepCompleted];
  tester.ExpectBucketCount("IOS.GuidedTour.DidFinishStep", 0, 1);

  // Tab grid being presented is the signal to start step 2.
  scene_state_.tabGridState.tabGridVisible = YES;

  ASSERT_NE(fake_tab_grid_toolbar_commands.dismissalCompletion, nil);
  fake_tab_grid_toolbar_commands.dismissalCompletion();
  tester.ExpectBucketCount("IOS.GuidedTour.DidFinishStep", 1, 1);

  // Step 2 completion triggers step 3.
  ASSERT_NE(fake_tab_grid_commands.dismissalCompletion, nil);
  fake_tab_grid_commands.dismissalCompletion();
  tester.ExpectBucketCount("IOS.GuidedTour.DidFinishStep", 2, 1);

  OCMExpect([scene_commands_handler hideGuidedTour]);

  // Step 3 completion triggers step 4.
  ASSERT_NE(fake_tab_grid_toolbar_commands.dismissalCompletion, nil);
  fake_tab_grid_toolbar_commands.dismissalCompletion();
  tester.ExpectBucketCount("IOS.GuidedTour.DidFinishStep", 3, 1);

  EXPECT_TRUE(completion_called);
  EXPECT_TRUE(tour_started);
  EXPECT_OCMOCK_VERIFY(scene_commands_handler);
}

// Tests that `stop` does not crash when the command dispatcher is not
// dispatching for `GuidedTourCommands` or `SceneCommands`.
TEST_F(GuidedTourCoordinatorTest, StopGuidedTourWithoutHandlers) {
  enabled_feature_list_.InitAndEnableFeatureWithParameters(kBestOfAppFRE,
                                                           {{"variant", "4"}});

  [coordinator_ startGuidedTour];

  // This should not crash even if handlers are missing.
  [coordinator_ stop];
}
