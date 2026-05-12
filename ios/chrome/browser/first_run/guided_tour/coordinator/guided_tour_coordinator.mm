// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/guided_tour/coordinator/guided_tour_coordinator.h"

#import "base/notreached.h"
#import "ios/chrome/browser/bubble/ui_bundled/guided_tour/guided_tour_bubble_view_controller_presenter.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/legacy_toolbar_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Corner radius of the spotlight cutouts.
const CGFloat kNTPTabGridButtonSpotlightCornerRadius = 7.0f;
const CGFloat kNTPAppBarTabGridButtonSpotlightCornerRadius = 14.0f;
const CGFloat kNTPTabGridPageControlCornerRadius = 13.0f;
}  // namespace

@interface GuidedTourCoordinator () <
    GuidedTourBubbleViewControllerPresenterDelegate>
@end

@implementation GuidedTourCoordinator {
  GuidedTourStep _step;
  GuidedTourBubbleViewControllerPresenter* _presenter;
  ProceduralBlock _completionBlock;
}

- (instancetype)initWithStep:(GuidedTourStep)step
          baseViewController:(UIViewController*)baseViewController
                     browser:(Browser*)browser
             completionBlock:(ProceduralBlock)completionBlock {
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser])) {
    _step = step;
    _completionBlock = completionBlock;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
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
                completionCallback:_completionBlock];
  _presenter.delegate = self;
  _presenter.maximumContentSizeCategory =
      UIContentSizeCategoryExtraExtraExtraLarge;

  UIView* anchorView = [self anchorView];
  CGPoint anchorPoint = [self anchorPointForAnchorView:anchorView];

  [_presenter presentInViewController:self.baseViewController
                          anchorPoint:anchorPoint
                           anchorView:[self cutoutView]];
}

- (void)stop {
  [_presenter dismiss];
  _presenter = nil;
}

#pragma mark - Private

// Returns the view to which the bubble view will be anchored.
- (UIView*)anchorView {
  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  if (_step == GuidedTourStep::kNTP) {
    if (IsChromeNextIaEnabled()) {
      UIButton* tabSwitcherButton = static_cast<UIButton*>(
          [layoutGuideCenter referencedViewUnderName:kTabSwitcherGuide]);
      return tabSwitcherButton;
    }
    LegacyToolbarButton* tabSwitcherButton = static_cast<LegacyToolbarButton*>(
        [layoutGuideCenter referencedViewUnderName:kTabSwitcherGuide]);
    return tabSwitcherButton.spotlightView;
  } else if (_step == GuidedTourStep::kTabGridIncognito) {
    return [layoutGuideCenter
        referencedViewUnderName:kTabGridPageControlIncognitoGuide];
  } else if (_step == GuidedTourStep::kTabGridLongPress) {
    return
        [layoutGuideCenter referencedViewUnderName:kSelectedRegularCellGuide];
  } else if (_step == GuidedTourStep::kTabGridTabGroup) {
    return [layoutGuideCenter
        referencedViewUnderName:kTabGridPageControlTabGroupsGuide];
  }
  NOTREACHED() << "A layout guide view needs to be fetched for each step";
}

// Returns the anchor point in `anchorView` to which the bubble view will be
// anchored.
- (CGPoint)anchorPointForAnchorView:(UIView*)anchorView {
  CGFloat anchorPointX = CGRectGetMidX(anchorView.frame);
  CGFloat anchorPointY;
  if ([self shouldPointArrowDown]) {
    anchorPointY = CGRectGetMinY(anchorView.frame);
  } else {
    anchorPointY = CGRectGetMaxY(anchorView.frame);
  }

  // Sometimes, the tab grid only has 1 column (e.g. if the dynamic text size is
  // large). In this case, the tab can be quite large, so putting the bubble
  // below or above the tab can lead to not enough space being available.
  // Instead, put the bubble over the middle of the tab to allow for more space.
  if (_step == GuidedTourStep::kTabGridLongPress &&
      anchorView.bounds.size.width >
          self.baseViewController.view.bounds.size.width / 2) {
    anchorPointY = CGRectGetMidY(anchorView.frame);
  }
  CGPoint anchorPoint = CGPointMake(anchorPointX, anchorPointY);

  return [anchorView.superview convertPoint:anchorPoint toView:nil];
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
  CGFloat NTPTabGridButtonSpotlightCornerRadius =
      IsChromeNextIaEnabled() ? kNTPAppBarTabGridButtonSpotlightCornerRadius
                              : kNTPTabGridButtonSpotlightCornerRadius;
  return _step == GuidedTourStep::kNTP ? NTPTabGridButtonSpotlightCornerRadius
                                       : kNTPTabGridPageControlCornerRadius;
}

// YES if the bubble arrow should point down (e.g. the NTP step is pointing down
// to the bottom toolbar).
- (BOOL)shouldPointArrowDown {
  if (_step == GuidedTourStep::kNTP) {
    return IsSplitToolbarMode(self.baseViewController);
  }
  if (_step == GuidedTourStep::kTabGridLongPress) {
    UIView* anchorView = [self anchorView];
    CGRect anchorFrameInBaseViewController =
        [anchorView convertRect:[anchorView bounds]
                         toView:self.baseViewController.view];
    // Point arrow down if anchor view is on the bottom half of the screen.
    return CGRectGetMidY(anchorFrameInBaseViewController) >
           CGRectGetMidY(self.baseViewController.view.bounds);
  }
  return NO;
}

// Returns the bubble alignment for each step.
- (BubbleAlignment)bubbleAlignment {
  if (_step == GuidedTourStep::kNTP) {
    return BubbleAlignmentBottomOrTrailing;
  } else if (_step == GuidedTourStep::kTabGridIncognito) {
    return BubbleAlignmentTopOrLeading;
  } else if (_step == GuidedTourStep::kTabGridLongPress) {
    UIView* anchorView = [self anchorView];

    // Sometimes, the tab grid only has 1 column (e.g. if the dynamic text size
    // is large). In this case, center the arrow.
    if (anchorView.bounds.size.width >
        self.baseViewController.view.bounds.size.width / 2) {
      return BubbleAlignmentCenter;
    }

    CGRect anchorFrameInBaseViewController =
        [anchorView convertRect:[anchorView bounds]
                         toView:self.baseViewController.view];

    CGFloat screenCenterX = CGRectGetMidX(self.baseViewController.view.bounds);
    CGFloat anchorFrameCenterX = CGRectGetMidX(anchorFrameInBaseViewController);

    BOOL onLeadingHalfOfScreen =
        EdgeLeadsEdge(anchorFrameCenterX, screenCenterX);
    return onLeadingHalfOfScreen ? BubbleAlignmentTopOrLeading
                                 : BubbleAlignmentBottomOrTrailing;
  } else if (_step == GuidedTourStep::kTabGridTabGroup) {
    return BubbleAlignmentBottomOrTrailing;
  }
  NOTREACHED()
      << "Need to define the bubble alignment for each guided tour step";
}

// Returns a UIView that needs to be cut out of the blur background.
- (UIView*)cutoutView {
  UIView* cutoutView;
  if (_step == GuidedTourStep::kNTP ||
      _step == GuidedTourStep::kTabGridLongPress) {
    cutoutView = [self anchorView];
  } else {
    // The TabGrid Page Control steps should cut out the entire page control,
    // not just the anchor view.
    cutoutView = [LayoutGuideCenterForBrowser(self.browser)
        referencedViewUnderName:kTabGridPageControlGuide];
  }
  return cutoutView;
}

#pragma mark - GuidedTourBubbleViewControllerPresenterDelegate

- (CGPoint)anchorPointForGuidedTourBubbleViewControllerPresenter:
    (GuidedTourBubbleViewControllerPresenter*)presenter {
  return [self anchorPointForAnchorView:[self anchorView]];
}

@end
