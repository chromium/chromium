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
// Corner radius of the spotlight cutouts.
const CGFloat kNTPTabGridButtonSpotlightCornerRadius = 7.0f;
const CGFloat kNTPTabGridPageControlCornerRadius = 13.0f;
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
  BubbleArrowDirection direction = [self shouldPointArrowDown]
                                       ? BubbleArrowDirectionDown
                                       : BubbleArrowDirectionUp;
  _presenter = [[GuidedTourBubbleViewControllerPresenter alloc]
      initWithText:[self bodyString]
      title:[self titleString]
      guidedTourStep:_step
      arrowDirection:direction
      alignment:[self bubbleAlignment]
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

  [_presenter presentInViewController:self.baseViewController
                          anchorPoint:anchorPoint
                      anchorViewFrame:[self cutoutView]];
}

- (void)stop {
  [_presenter dismiss];
  _presenter = nil;
}

#pragma mark - Private

// Returns the view to which the bubble view will be anchored.
- (UIView*)anchorView {
  if (_step == GuidedTourStep::kNTP) {
    ToolbarButton* tabSwitcherButton =
        static_cast<ToolbarButton*>([LayoutGuideCenterForBrowser(self.browser)
            referencedViewUnderName:kTabSwitcherGuide]);
    return tabSwitcherButton.spotlightView;
  } else if (_step == GuidedTourStep::kTabGridIncognito) {
    return [LayoutGuideCenterForBrowser(nil)
        referencedViewUnderName:kTabGridPageControlIncognitoGuide];
  } else if (_step == GuidedTourStep::kTabGridLongPress) {
    return [LayoutGuideCenterForBrowser(self.browser)
        referencedViewUnderName:kSelectedRegularCellGuide];
  } else if (_step == GuidedTourStep::kTabGridTabGroup) {
    return [LayoutGuideCenterForBrowser(nil)
        referencedViewUnderName:kTabGridPageControlTabGroupsGuide];
  }
  NOTREACHED() << "A layout guide view needs to be fetched for each step";
}

// Returns the anchor point in `anchorView` to which the bubble view will be
// anchored.
- (CGPoint)anchorPointForAnchorView:(UIView*)anchorView {
  CGPoint anchorPoint;
  if ([self shouldPointArrowDown]) {
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
  if (_step == GuidedTourStep::kNTP) {
    return l10n_util::GetNSString(IDS_IOS_FIRST_RUN_GUIDED_TOUR_NTP_IPH_TITLE);
  } else if (_step == GuidedTourStep::kTabGridIncognito) {
    return l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_GUIDED_TOUR_TAB_GRID_INCOGNITO_IPH_TITLE);
  } else if (_step == GuidedTourStep::kTabGridLongPress) {
    return l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_GUIDED_TOUR_TAB_GRID_LONG_PRESS_IPH_TITLE);
  } else if (_step == GuidedTourStep::kTabGridTabGroup) {
    return l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_GUIDED_TOUR_TAB_GRID_TAB_GROUP_IPH_TITLE);
  }
  return @"";
}

// Returns the main text string for this step's Bubble View.
- (NSString*)bodyString {
  if (_step == GuidedTourStep::kNTP) {
    return l10n_util::GetNSString(IDS_IOS_FIRST_RUN_GUIDED_TOUR_NTP_IPH_TEXT);
  } else if (_step == GuidedTourStep::kTabGridIncognito) {
    return l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_GUIDED_TOUR_TAB_GRID_INCOGNITO_IPH_TEXT);
  } else if (_step == GuidedTourStep::kTabGridLongPress) {
    return l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_GUIDED_TOUR_TAB_GRID_LONG_PRESS_IPH_TEXT);
  } else if (_step == GuidedTourStep::kTabGridTabGroup) {
    return l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_GUIDED_TOUR_TAB_GRID_TAB_GROUP_IPH_TEXT);
  }
  return @"";
}

// The corner radius of the spotlight cutout for this Bubble View.
- (CGFloat)backgroundCutoutCornerRadius {
  return _step == GuidedTourStep::kNTP ? kNTPTabGridButtonSpotlightCornerRadius
                                       : kNTPTabGridPageControlCornerRadius;
}

// YES if the bubble arrow should point down (e.g. the NTP step is pointing down
// to the bottom toolbar).
- (BOOL)shouldPointArrowDown {
  return IsSplitToolbarMode(self.baseViewController) &&
         _step == GuidedTourStep::kNTP;
}

// Returns the bubble alignment for each step.
- (BubbleAlignment)bubbleAlignment {
  if (_step == GuidedTourStep::kNTP) {
    return BubbleAlignmentBottomOrTrailing;
  } else if (_step == GuidedTourStep::kTabGridIncognito ||
             _step == GuidedTourStep::kTabGridLongPress) {
    return BubbleAlignmentTopOrLeading;
  } else if (_step == GuidedTourStep::kTabGridTabGroup) {
    return BubbleAlignmentBottomOrTrailing;
  }
  NOTREACHED()
      << "Need to define the bubble alignment for each guided tour step";
}

// Returns the frame that needs to be cut out of the blur background.
- (CGRect)cutoutView {
  UIView* cutoutView;
  if (_step == GuidedTourStep::kNTP ||
      _step == GuidedTourStep::kTabGridLongPress) {
    cutoutView = [self anchorView];
  } else {
    // The TabGrid Page Control steps should cut out the entire page control,
    // not just the anchor view.
    cutoutView = [LayoutGuideCenterForBrowser(nil)
        referencedViewUnderName:kTabGridPageControlGuide];
  }
  CGPoint cutoutViewOrigin =
      [cutoutView.superview convertPoint:cutoutView.frame.origin toView:nil];
  return CGRectMake(cutoutViewOrigin.x, cutoutViewOrigin.y,
                    cutoutView.frame.size.width, cutoutView.frame.size.height);
}

@end
