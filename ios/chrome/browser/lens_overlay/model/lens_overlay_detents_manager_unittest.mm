// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_detents_manager.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Fake delegate that exposes the latest reported value by the detents manager.
@interface FakeDetentsManagerDelegate
    : NSObject <LensOverlayDetentsManagerDelegate>

// The latest reported dimension state.
@property(nonatomic, readonly) SheetDimensionState latestReportedDimensionState;

@end

@implementation FakeDetentsManagerDelegate

- (BOOL)lensOverlayDetentsManagerShouldDismissBottomSheet:
    (LensOverlayDetentsManager*)detentsManager {
  return YES;
}

- (void)lensOverlayDetentsManagerDidChangeDimensionState:
    (LensOverlayDetentsManager*)detentsManager {
  _latestReportedDimensionState = detentsManager.sheetDimension;
}

@end

class LensOverlayDetentsManagerTest : public PlatformTest {
 protected:
  LensOverlayDetentsManagerTest() {
    presenting_view_controller_ = [[UIViewController alloc] init];
    presenting_view_controller_.view.backgroundColor = [UIColor redColor];
    presenting_view_controller_.modalPresentationStyle =
        UIModalPresentationPageSheet;
    presented_view_controller_ = [[UIViewController alloc] init];
    presented_view_controller_.view.backgroundColor = [UIColor blueColor];
    [scoped_key_window_.Get()
        setRootViewController:presenting_view_controller_];

    detents_manager_ = [[LensOverlayDetentsManager alloc]
        initWithBottomSheet:presented_view_controller_
                                .sheetPresentationController
                     window:scoped_key_window_.Get()];
  }

  LensOverlayDetentsManager* detents_manager_;
  __strong UIViewController* presented_view_controller_;
  __strong UIViewController* presenting_view_controller_;
  ScopedKeyWindow scoped_key_window_;

  // Presents and blocks until the view controller is presented.
  void WaitForPresentation() {
    __block bool presentation_finished = NO;
    [presenting_view_controller_
        presentViewController:presented_view_controller_
                     animated:NO
                   completion:^{
                     presentation_finished = YES;
                   }];

    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
      return presentation_finished;
    }));
  }
};

// Tests the default dimension state when presenting in unrestricted movement.
TEST_F(LensOverlayDetentsManagerTest,
       TestDefaultDimensionUnrestrictedMovement) {
  // Given an initially hidden sheet.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kHidden);

  // When presenting in an unrestricted movement state.
  [detents_manager_ adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
  WaitForPresentation();

  // Then the default post presentation dimension should be medium.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kMedium);
}

// Tests the default dimension state when presenting in consent mode.
TEST_F(LensOverlayDetentsManagerTest, TestDefaultDimensionConsent) {
  // Given an initially hidden sheet.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kHidden);

  // When presenting for the consent page.
  [detents_manager_ adjustDetentsForState:SheetDetentStateConsentDialog];
  WaitForPresentation();

  // Then the reported state dimension should be the consent one.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kConsent);
}

// Tests the default dimension state when presenting in peaking mode.
TEST_F(LensOverlayDetentsManagerTest, TestDefaultDimensionPeakingMode) {
  // Given an initially hidden sheet.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kHidden);

  // When presenting in the peaking state.
  [detents_manager_ adjustDetentsForState:SheetDetentStatePeakEnabled];
  WaitForPresentation();

  // Then the reported state dimension should have been adjusted accordingly.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kPeaking);
}

// Tests the ability of adjusting the detent after the initial presentation.
TEST_F(LensOverlayDetentsManagerTest,
       TestAdjustingDetentsPostInitialPresentation) {
  // Given an initially hidden sheet.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kHidden);

  // When presenting in the unrestricted movement state.
  [detents_manager_ adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
  WaitForPresentation();

  // Then the default presentation dimension immediately after presenting
  // should be medium.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kMedium);

  // When adjusting post presentation to the peaking state.
  [detents_manager_ adjustDetentsForState:SheetDetentStatePeakEnabled];

  // Then the reported state dimension should have been re-adjusted.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kPeaking);
}

// Tests that the correct presentation strategy is used when none is explicitly
// specified.
TEST_F(LensOverlayDetentsManagerTest, TestDefaultPresentationStrategy) {
  // Given an initially hidden sheet.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kHidden);

  // When presenting the page.
  WaitForPresentation();

  // Then the default presentation strategy should be selection.
  EXPECT_EQ(detents_manager_.presentationStrategy,
            SheetDetentPresentationStategySelection);
}

// Tests that adjusting the presentation strategy correctly adjusts the medium
// detent height.
TEST_F(LensOverlayDetentsManagerTest,
       TestAdjustingPresentationStrategyInUnrestrictedMode) {
  // Given an initially hidden sheet.
  EXPECT_EQ(detents_manager_.sheetDimension, SheetDimensionState::kHidden);

  // When presenting in the unrestricted movement state.
  [detents_manager_ adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
  WaitForPresentation();

  // Then the default presentation strategy should be selection
  EXPECT_EQ(detents_manager_.presentationStrategy,
            SheetDetentPresentationStategySelection);
  CGFloat estimatedMediumDetentHeightSelection =
      detents_manager_.estimatedMediumDetentHeight;
  EXPECT_GT(estimatedMediumDetentHeightSelection, 0.0);

  detents_manager_.presentationStrategy =
      SheetDetentPresentationStategyTranslate;
  CGFloat estimatedMediumDetentHeightTranslate =
      detents_manager_.estimatedMediumDetentHeight;

  // Then the height in translate should be lower than the height in selection.
  EXPECT_GT(estimatedMediumDetentHeightTranslate, 0.0);
  EXPECT_LT(estimatedMediumDetentHeightTranslate,
            estimatedMediumDetentHeightSelection);
}

// Tests the ability of adjusting the detent after the initial presentation.
TEST_F(LensOverlayDetentsManagerTest, TestAdjustingDetentsNotifiesDelegate) {
  // Given a detents delegate.
  FakeDetentsManagerDelegate* fakeDelegate =
      [[FakeDetentsManagerDelegate alloc] init];
  detents_manager_.delegate = fakeDelegate;
  EXPECT_EQ(fakeDelegate.latestReportedDimensionState,
            SheetDimensionState::kHidden);

  // When presenting in the unrestricted movement state.
  [detents_manager_ adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
  WaitForPresentation();

  // Then the changes in the dimension state should be propagated to the detents
  // delegate.
  EXPECT_EQ(fakeDelegate.latestReportedDimensionState,
            SheetDimensionState::kMedium);
  [detents_manager_ adjustDetentsForState:SheetDetentStatePeakEnabled];
  EXPECT_EQ(fakeDelegate.latestReportedDimensionState,
            SheetDimensionState::kPeaking);
}
