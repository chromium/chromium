// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/ProgressView/src/MaterialProgressView.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarViewController ()
// Redefined to be a PrimaryToolbarView.
@property(nonatomic, strong) PrimaryToolbarView* view;
@property(nonatomic, assign) BOOL isNTP;
// The last fullscreen progress registered.
@property(nonatomic, assign) CGFloat previousFullscreenProgress;
@end

@implementation PrimaryToolbarViewController

@synthesize delegate = _delegate;
@synthesize isNTP = _isNTP;
@synthesize previousFullscreenProgress = _previousFullscreenProgress;
@dynamic view;

#pragma mark - Public

- (void)showPrerenderingAnimation {
  __weak PrimaryToolbarViewController* weakSelf = self;
  [self.view.progressBar setProgress:0];
  [self.view.progressBar setHidden:NO
                          animated:[self areAnimationsEnabled]
                        completion:^(BOOL finished) {
                          [weakSelf stopProgressBar];
                        }];
}

#pragma mark - AdaptiveToolbarViewController

- (void)updateForSideSwipeSnapshotOnNTP:(BOOL)onNTP {
  [super updateForSideSwipeSnapshotOnNTP:onNTP];
  if (!onNTP)
    return;

  self.view.backgroundColor =
      self.buttonFactory.toolbarConfiguration.NTPBackgroundColor;
  self.view.locationBarContainer.hidden = YES;
}

- (void)resetAfterSideSwipeSnapshot {
  [super resetAfterSideSwipeSnapshot];
  self.view.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  self.view.locationBarContainer.hidden = NO;
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  [super setScrollProgressForTabletOmnibox:progress];

  // Sometimes an NTP may make a delegate call when it's no longer visible.
  if (!self.isNTP)
    progress = 1;

  if (progress == 1) {
    self.view.locationBarContainer.transform = CGAffineTransformIdentity;
  } else {
    self.view.locationBarContainer.transform = CGAffineTransformMakeTranslation(
        0, [self verticalMarginForLocationBarForFullscreenProgress:1] *
               (progress - 1));
  }
  self.view.locationBarContainer.alpha = progress;
  self.view.separator.alpha = progress;

  // When the locationBarContainer is hidden, show the |fakeOmniboxTarget|.
  if (progress == 0 && !self.view.fakeOmniboxTarget) {
    [self.view addFakeOmniboxTarget];
    UITapGestureRecognizer* tapRecognizer =
        [[UITapGestureRecognizer alloc] initWithTarget:self.dispatcher
                                                action:@selector(focusOmnibox)];
    [self.view.fakeOmniboxTarget addGestureRecognizer:tapRecognizer];
  } else if (progress > 0 && self.view.fakeOmniboxTarget) {
    [self.view removeFakeOmniboxTarget];
  }
}

#pragma mark - UIViewController

- (void)loadView {
  DCHECK(self.buttonFactory);

  // The first time, the toolbar is fully displayed.
  self.previousFullscreenProgress = 1;

  self.view =
      [[PrimaryToolbarView alloc] initWithButtonFactory:self.buttonFactory];

  // This method cannot be called from the init as the topSafeAnchor can only be
  // set to topLayoutGuide after the view creation on iOS 10.
  [self.view setUp];

  self.view.locationBarHeight.constant =
      [self locationBarHeightForFullscreenProgress:1];
  self.view.locationBarContainer.layer.cornerRadius =
      self.view.locationBarHeight.constant / 2;
  self.view.locationBarBottomConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:1];

  [self.view.collapsedToolbarButton addTarget:self
                                       action:@selector(exitFullscreen)
                             forControlEvents:UIControlEventTouchUpInside];

  if (IsCompactHeight(self)) {
    self.view.locationBarExtraBottomPadding.constant =
        kAdaptiveLocationBarExtraVerticalMargin;
  } else {
    self.view.locationBarExtraBottomPadding.constant = 0;
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  if (@available(iOS 13, *)) {
    [self updateLayoutForPreviousTraitCollection:nil];
  }
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  UIView* omniboxView = self.view.locationBarContainer;
  [NamedGuide guideWithName:kOmniboxGuide view:omniboxView].constrainedView =
      omniboxView;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateLayoutForPreviousTraitCollection:previousTraitCollection];
}

#pragma mark - Property accessors

- (void)setLocationBarViewController:
    (UIViewController*)locationBarViewController {
  [self addChildViewController:locationBarViewController];
  [locationBarViewController didMoveToParentViewController:self];
  self.view.locationBarView = locationBarViewController.view;
}

- (void)setIsNTP:(BOOL)isNTP {
  if (isNTP == _isNTP)
    return;
  [super setIsNTP:isNTP];
  _isNTP = isNTP;
  if (!isNTP && !IsSplitToolbarMode(self)) {
    // Reset any location bar view updates when not an NTP.
    [self setScrollProgressForTabletOmnibox:1];
  }
}

#pragma mark - ActivityServicePositioner

- (UIView*)shareButtonView {
  return self.view.shareButton;
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  CGFloat alphaValue = fmax(progress * 2 - 1, 0);
  self.view.leadingStackView.alpha = alphaValue;
  self.view.trailingStackView.alpha = alphaValue;
  self.view.locationBarHeight.constant =
      [self locationBarHeightForFullscreenProgress:progress];
  self.view.locationBarContainer.layer.cornerRadius =
      self.view.locationBarHeight.constant / 2;
  self.view.locationBarBottomConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:progress];
  self.view.locationBarContainer.backgroundColor =
      [self.buttonFactory.toolbarConfiguration
          locationBarBackgroundColorWithVisibility:alphaValue];
  self.previousFullscreenProgress = progress;

  self.view.collapsedToolbarButton.hidden = progress > 0.05;
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled)
    [self updateForFullscreenProgress:1.0];
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  CGFloat finalProgress = animator.finalProgress;
  // Using the animator doesn't work as the animation doesn't trigger a relayout
  // of the constraints (see crbug.com/978462, crbug.com/950994).
  [UIView animateWithDuration:animator.duration
                   animations:^{
                     [self updateForFullscreenProgress:finalProgress];
                     [self.view layoutIfNeeded];
                   }];
}

#pragma mark - ToolbarAnimatee

- (void)expandLocationBar {
  [self deactivateViewLocationBarConstraints];
  [NSLayoutConstraint activateConstraints:self.view.expandedConstraints];
  [self.view layoutIfNeeded];
}

- (void)contractLocationBar {
  [self deactivateViewLocationBarConstraints];
  if (IsSplitToolbarMode(self)) {
    [NSLayoutConstraint
        activateConstraints:self.view.contractedNoMarginConstraints];
  } else {
    [NSLayoutConstraint activateConstraints:self.view.contractedConstraints];
  }
  [self.view layoutIfNeeded];
}

- (void)showCancelButton {
  self.view.cancelButton.hidden = NO;
}

- (void)hideCancelButton {
  self.view.cancelButton.hidden = YES;
}

- (void)showControlButtons {
  for (ToolbarButton* button in self.view.allButtons) {
    button.alpha = 1;
  }
}

- (void)hideControlButtons {
  for (ToolbarButton* button in self.view.allButtons) {
    button.alpha = 0;
  }
}

#pragma mark - Private

- (void)updateLayoutForPreviousTraitCollection:
    (UITraitCollection*)previousTraitCollection {
  [self.delegate
      viewControllerTraitCollectionDidChange:previousTraitCollection];
  if (IsCompactHeight(self)) {
    self.view.locationBarExtraBottomPadding.constant =
        kAdaptiveLocationBarExtraVerticalMargin;
  } else {
    self.view.locationBarExtraBottomPadding.constant = 0;
  }
  self.view.locationBarBottomConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:
                self.previousFullscreenProgress];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    self.view.locationBarHeight.constant = [self
        locationBarHeightForFullscreenProgress:self.previousFullscreenProgress];
    self.view.locationBarContainer.layer.cornerRadius =
        self.view.locationBarHeight.constant / 2;
  }
}

- (CGFloat)clampedFontSizeMultiplier {
  return ToolbarClampedFontSizeMultiplier(
      self.traitCollection.preferredContentSizeCategory);
}

// Returns the desired height of the location bar, based on the fullscreen
// |progress|.
- (CGFloat)locationBarHeightForFullscreenProgress:(CGFloat)progress {
  CGFloat expandedHeight =
      LocationBarHeight(self.traitCollection.preferredContentSizeCategory);
  CGFloat collapsedHeight =
      ToolbarCollapsedHeight(self.traitCollection.preferredContentSizeCategory);
  CGFloat expandedCollapsedDelta = expandedHeight - collapsedHeight;

  return AlignValueToPixel(collapsedHeight + expandedCollapsedDelta * progress);
}

// Returns the vertical margin to the location bar based on fullscreen
// |progress|, aligned to the nearest pixel.
- (CGFloat)verticalMarginForLocationBarForFullscreenProgress:(CGFloat)progress {
  // The vertical bottom margin for the location bar is such that the location
  // bar looks visually centered. However, the constraints are not geometrically
  // centering the location bar. It is moved by 0pt (+ 1pt from extra padding)
  // in iPhone landscape and by 3pt in all other configurations.
  CGFloat fullscreenVerticalMargin =
      IsCompactHeight(self) ? 0 : kAdaptiveLocationBarVerticalMarginFullscreen;
  return -AlignValueToPixel((kAdaptiveLocationBarVerticalMargin * progress +
                             fullscreenVerticalMargin * (1 - progress)) *
                                [self clampedFontSizeMultiplier] +
                            ([self clampedFontSizeMultiplier] - 1) *
                                kLocationBarVerticalMarginDynamicType);
}

// Deactivates the constraints on the location bar positioning.
- (void)deactivateViewLocationBarConstraints {
  [NSLayoutConstraint deactivateConstraints:self.view.contractedConstraints];
  [NSLayoutConstraint
      deactivateConstraints:self.view.contractedNoMarginConstraints];
  [NSLayoutConstraint deactivateConstraints:self.view.expandedConstraints];
}

// Exits fullscreen.
- (void)exitFullscreen {
  [self.delegate exitFullscreen];
}

@end
