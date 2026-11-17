// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test view controller allowing `presentingViewController` to be stubbed
// without requiring a live window presentation.
@interface TestManualFillViewController : UIViewController
@property(nonatomic, strong) UIViewController* stubbedPresentingViewController;
@end

@implementation TestManualFillViewController

- (UIViewController*)presentingViewController {
  if (self.stubbedPresentingViewController) {
    return self.stubbedPresentingViewController;
  }
  return self.parentViewController.presentingViewController;
}

@end

using ManualFillConstantsTest = PlatformTest;

// Tests that `shouldUsePopover:` does not query `modalPresentationStyle` on an
// unpresented view controller and falls back to its size class.
TEST_F(ManualFillConstantsTest,
       ShouldUsePopoverSkipsModalPresentationStyleWhenUnpresented) {
  UIViewController* view_controller = [[UIViewController alloc] init];
  id partial_mock = OCMPartialMock(view_controller);
  OCMReject([partial_mock modalPresentationStyle]);

  view_controller.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassCompact;
  view_controller.traitOverrides.verticalSizeClass =
      UIUserInterfaceSizeClassRegular;
  EXPECT_FALSE([ManualFillUtil shouldUsePopover:partial_mock]);

  view_controller.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;
  view_controller.traitOverrides.verticalSizeClass =
      UIUserInterfaceSizeClassRegular;
  EXPECT_TRUE([ManualFillUtil shouldUsePopover:partial_mock]);

  EXPECT_OCMOCK_VERIFY(partial_mock);
  [partial_mock stopMocking];
}

// Tests that `shouldUsePopover:` returns `NO` in compact width when
// `modalPresentationStyle` is `UIModalPresentationPopover` but the view
// controller is not presented.
TEST_F(ManualFillConstantsTest,
       ShouldUsePopoverIgnoresUnpresentedPopoverModalStyle) {
  TestManualFillViewController* view_controller =
      [[TestManualFillViewController alloc] init];
  view_controller.modalPresentationStyle = UIModalPresentationPopover;
  view_controller.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassCompact;
  view_controller.traitOverrides.verticalSizeClass =
      UIUserInterfaceSizeClassRegular;

  EXPECT_FALSE([ManualFillUtil shouldUsePopover:view_controller]);
}

// Tests that `shouldUsePopover:` returns `YES` for a presented popover view
// controller and its child view controller even when UIKit overrides the
// popover's `horizontalSizeClass` to compact.
TEST_F(ManualFillConstantsTest,
       ShouldUsePopoverReturnsYesForPresentedPopoverAndChild) {
  UIViewController* presenter = [[UIViewController alloc] init];
  TestManualFillViewController* parent =
      [[TestManualFillViewController alloc] init];
  parent.modalPresentationStyle = UIModalPresentationPopover;
  parent.stubbedPresentingViewController = presenter;
  parent.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
  parent.traitOverrides.verticalSizeClass = UIUserInterfaceSizeClassRegular;

  TestManualFillViewController* child =
      [[TestManualFillViewController alloc] init];
  child.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
  child.traitOverrides.verticalSizeClass = UIUserInterfaceSizeClassRegular;
  [parent addChildViewController:child];
  [child didMoveToParentViewController:parent];

  EXPECT_TRUE([ManualFillUtil shouldUsePopover:parent]);
  EXPECT_TRUE([ManualFillUtil shouldUsePopover:child]);
}

// Tests that `shouldUsePopover:` falls back to size class when a presented view
// controller does not use `UIModalPresentationPopover`.
TEST_F(ManualFillConstantsTest,
       ShouldUsePopoverFallsBackToSizeClassForNonPopoverPresentation) {
  UIViewController* presenter = [[UIViewController alloc] init];
  TestManualFillViewController* view_controller =
      [[TestManualFillViewController alloc] init];
  view_controller.modalPresentationStyle = UIModalPresentationPageSheet;
  view_controller.stubbedPresentingViewController = presenter;

  view_controller.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassCompact;
  view_controller.traitOverrides.verticalSizeClass =
      UIUserInterfaceSizeClassRegular;
  EXPECT_FALSE([ManualFillUtil shouldUsePopover:view_controller]);

  view_controller.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;
  view_controller.traitOverrides.verticalSizeClass =
      UIUserInterfaceSizeClassRegular;
  EXPECT_TRUE([ManualFillUtil shouldUsePopover:view_controller]);
}
