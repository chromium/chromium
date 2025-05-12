// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/guided_tour/guided_tour_coordinator.h"

#import "base/notreached.h"
#import "ios/chrome/browser/bubble/ui_bundled/guided_tour/guided_tour_bubble_view_controller_presenter.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Corner radius of the Tab Grid button spotlight cutout for the NTP step.
const CGFloat kNTPTabGridButtonSpotlightCornerRadius = 7.0f;
}  // namespace

@implementation GuidedTourCoordinator {
  GuidedTourStep _step;
  __weak id<GuidedTourCoordinatorDelegate> _delegate;
  GuidedTourBubbleViewControllerPresenter* _presenter;
}

- (instancetype)initWithStep:(GuidedTourStep)step
          baseViewController:(UIViewController*)baseViewController
                     browser:(Browser*)browser
                    delegate:(id<GuidedTourCoordinatorDelegate>)delegate {
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser])) {
    _step = step;
    _delegate = delegate;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  __weak GuidedTourCoordinator* weakSelf = self;
  BubbleArrowDirection direction = IsSplitToolbarMode(self.baseViewController)
                                       ? BubbleArrowDirectionDown
                                       : BubbleArrowDirectionUp;
  _presenter = [[GuidedTourBubbleViewControllerPresenter alloc]
      initWithText:[self bodyString]
      title:[self titleString]
      arrowDirection:direction
      alignment:BubbleAlignmentBottomOrTrailing
      bubbleType:BubbleViewTypeRichWithNext
      backgroundCutoutCornerRadius:[self backgroundCutoutCornerRadius]
      dismissalCallback:^(IPHDismissalReasonType reason) {
        [weakSelf dismissFinished];
      }
      completionCallback:^{
        [weakSelf nextTapped];
      }];

  UIView* anchorView = [self anchorView];
  CGPoint anchorPoint = [self anchorPointForAnchorView:anchorView];
  CGPoint anchorViewOrigin =
      [anchorView.superview convertPoint:anchorView.frame.origin toView:nil];
  CGRect anchorViewFrame =
      CGRectMake(anchorViewOrigin.x, anchorViewOrigin.y,
                 anchorView.frame.size.width, anchorView.frame.size.height);

  [_presenter presentInViewController:self.baseViewController
                          anchorPoint:anchorPoint
                      anchorViewFrame:anchorViewFrame];
}

- (void)stop {
  [_presenter dismiss];
  _presenter = nil;
}

#pragma mark - Private

// Returns the view to which the bubble view will be anchored.
- (UIView*)anchorView {
  if (_step == GuidedTourStepNTP) {
    ToolbarButton* tabSwitcherButton =
        static_cast<ToolbarButton*>([LayoutGuideCenterForBrowser(self.browser)
            referencedViewUnderName:kTabSwitcherGuide]);
    return tabSwitcherButton.spotlightView;
  }
  NOTREACHED() << "A layout guide view needs to be fetched for each step";
}

// Returns the anchor point in `anchorView` to which the bubble view will be
// anchored.
- (CGPoint)anchorPointForAnchorView:(UIView*)anchorView {
  CGPoint anchorPoint;
  if (IsSplitToolbarMode(self.baseViewController)) {
    anchorPoint = CGPointMake(CGRectGetMidX(anchorView.frame),
                              CGRectGetMinY(anchorView.frame));
  } else {
    anchorPoint = CGPointMake(CGRectGetMidX(anchorView.frame),
                              CGRectGetMaxY(anchorView.frame));
  }
  return [anchorView.superview convertPoint:anchorPoint toView:nil];
}

// Handle the user tapping on the next button.
- (void)nextTapped {
  [_delegate nextTappedForStep:_step];
}

// Handle the dismissal completion of this step.
- (void)dismissFinished {
  [_delegate stepCompleted:_step];
}

// Returns the title string used for this step's Bubble View.
- (NSString*)titleString {
  if (_step == GuidedTourStepNTP) {
    return l10n_util::GetNSString(IDS_IOS_FIRST_RUN_GUIDED_TOUR_NTP_IPH_TITLE);
  }
  return @"";
}

// Returns the main text string for this step's Bubble View.
- (NSString*)bodyString {
  if (_step == GuidedTourStepNTP) {
    return l10n_util::GetNSString(IDS_IOS_FIRST_RUN_GUIDED_TOUR_NTP_IPH_TEXT);
  }
  return @"";
}

// The corner radius of the spotlight cutout for this Bubble View.
- (CGFloat)backgroundCutoutCornerRadius {
  return _step == GuidedTourStepNTP ? kNTPTabGridButtonSpotlightCornerRadius
                                    : 0;
}

@end
