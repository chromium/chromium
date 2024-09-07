// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller.h"

#import <MaterialComponents/MaterialProgressView.h>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"
#import "ios/chrome/common/ui/util/ui_util.h"

@interface PrimaryToolbarViewController ()

// Redefined to be a PrimaryToolbarView.
@property(nonatomic, strong) PrimaryToolbarView* view;
@property(nonatomic, assign) BOOL isNTP;
// The last fullscreen progress registered.
@property(nonatomic, assign) CGFloat previousFullscreenProgress;
// Pan Gesture Recognizer for the view revealing pan gesture handler.
@property(nonatomic, weak) UIPanGestureRecognizer* panGestureRecognizer;

@end

@implementation PrimaryToolbarViewController

@dynamic view;

#pragma mark - AdaptiveToolbarViewController

- (void)updateForSideSwipeSnapshot:(BOOL)onNonIncognitoNTP {
  [super updateForSideSwipeSnapshot:onNonIncognitoNTP];
  if (!onNonIncognitoNTP) {
    return;
  }

  // An opaque image is expected during a snapshot. Make sure the view is not
  // hidden and display a blank view by using the NTP background and by hidding
  // the location bar.
  self.view.hidden = NO;
  self.view.backgroundColor =
      self.buttonFactory.toolbarConfiguration.NTPBackgroundColor;
  self.view.locationBarContainer.hidden = YES;
}

- (void)resetAfterSideSwipeSnapshot {
  [super resetAfterSideSwipeSnapshot];
  // Note: the view is made visible or not by an `updateToolbar` call when the
  // snapshot animation ends.
  self.view.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  if (self.hasOmnibox) {
    self.view.locationBarContainer.hidden = NO;
  }
}

#pragma mark - AdaptiveToolbarViewController (Subclassing)

- (void)setLocationBarViewController:
    (UIViewController*)locationBarViewController {
  [super setLocationBarViewController:locationBarViewController];

  self.view.separator.hidden = !self.hasOmnibox;
  [self updateBackgroundColor];
}

- (void)updateBackgroundColor {
  UIColor* backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  if (base::FeatureList::IsEnabled(kThemeColorInTopToolbar) &&
      !self.hasOmnibox) {
    if (self.pageThemeColor) {
      backgroundColor = self.pageThemeColor;
    } else if (self.underPageBackgroundColor) {
      backgroundColor = self.underPageBackgroundColor;
    }
  }
  self.view.backgroundColor = backgroundColor;
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

  // When the locationBarContainer is hidden, show the `fakeOmniboxTarget`.
  if (progress == 0 && !self.view.fakeOmniboxTarget) {
    [self.view addFakeOmniboxTarget];
    UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self.omniboxCommandsHandler
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
  [self.layoutGuideCenter referenceView:self.view
                              underName:kPrimaryToolbarGuide];

  // This method cannot be called from the init as the topSafeAnchor can only be
  // set to topLayoutGuide after the view creation on iOS 10.
  [self.view setUp];

  [self.layoutGuideCenter referenceView:self.view.locationBarContainer
                              underName:kTopOmniboxGuide];
  self.view.locationBarBottomConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:1];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // iOS 17 and later introduce a new way to handle trait changes. If the OS
  // version is iOS 17 or later, we skip the old way of updating views.
  if (@available(iOS 17, *)) {
    return;
  }
  [self updateViews:self.view previousTraitCollection:previousTraitCollection];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // On iOS 17 and later, we register for specific trait changes (vertical and
  // horizontal size classes) and provide a handler method
  // `updateViews:previousTraitCollection:` to be called when those traits
  // change.
  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:@[
      UITraitVerticalSizeClass.self, UITraitHorizontalSizeClass.self
    ]
                       withAction:@selector(updateViews:
                                      previousTraitCollection:)];
  }
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self.delegate close];
}

#pragma mark - Public

- (void)setTabGroupIndicatorView:(TabGroupIndicatorView*)view {
  CHECK(IsTabGroupIndicatorEnabled());
  self.view.tabGroupIndicatorView = view;
}

#pragma mark - Property accessors

- (void)setIsNTP:(BOOL)isNTP {
  if (isNTP == _isNTP)
    return;
  [super setIsNTP:isNTP];
  _isNTP = isNTP;
  if (IsSplitToolbarMode(self) || !self.shouldHideOmniboxOnNTP)
    return;

  // This is hiding/showing and positionning the omnibox. This is only needed
  // if the omnibox should be hidden when there is only one toolbar.
  if (!isNTP) {
    // Reset any location bar view updates when not an NTP.
    [self setScrollProgressForTabletOmnibox:1];
  } else {
    // Hides the omnibox.
    [self setScrollProgressForTabletOmnibox:0];
  }
}

#pragma mark - SharingPositioner

- (UIView*)sourceView {
  return self.view.shareButton;
}

- (CGRect)sourceRect {
  return self.view.shareButton.bounds;
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  [super updateForFullscreenProgress:progress];

  self.previousFullscreenProgress = progress;

  CGFloat alphaValue = fmax(progress * 2 - 1, 0);
  self.view.leadingStackView.alpha = alphaValue;
  self.view.trailingStackView.alpha = alphaValue;
  if (IsTabGroupIndicatorEnabled()) {
    self.view.tabGroupIndicatorView.alpha = alphaValue;
  }
  self.view.locationBarBottomConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:progress];
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

- (void)setToolbarFaded:(BOOL)faded {
  self.view.alpha = faded ? 0 : 1;
}

- (void)setLocationBarHeightToMatchFakeOmnibox {
  if (!IsSplitToolbarMode(self)) {
    return;
  }
  [self setLocationBarContainerHeight:content_suggestions::
                                          PinnedFakeOmniboxHeight()];
  self.view.matchNTPHeight = YES;
}

- (void)setLocationBarHeightExpanded {
  [self setLocationBarContainerHeight:LocationBarHeight(
                                          self.traitCollection
                                              .preferredContentSizeCategory)];
  self.view.matchNTPHeight = NO;
}

#pragma mark - Private

// Adjusts the layout and appearance of views in response to changes in
// available space and trait collections.
- (void)updateViews:(UIView*)updatedView
    previousTraitCollection:(UITraitCollection*)previousTraitCollection {
  self.view.locationBarBottomConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:
                self.previousFullscreenProgress];
  self.view.topCornersRounded = NO;
  if (IsTabGroupIndicatorEnabled()) {
    [self.view updateTabGroupIndicatorAvailability];
  }
  [self.delegate
      viewControllerTraitCollectionDidChange:previousTraitCollection];
}

- (CGFloat)clampedFontSizeMultiplier {
  return ToolbarClampedFontSizeMultiplier(
      self.traitCollection.preferredContentSizeCategory);
}

// Returns the vertical margin to the location bar based on fullscreen
// `progress`, aligned to the nearest pixel.
- (CGFloat)verticalMarginForLocationBarForFullscreenProgress:(CGFloat)progress {
  // The vertical bottom margin for the location bar is such that the location
  // bar looks visually centered. However, the constraints are not geometrically
  // centering the location bar. It is moved by 0pt in iPhone landscape and by
  // 3pt in all other configurations.
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

// Sets the height of the location bar container.
- (void)setLocationBarContainerHeight:(CGFloat)height {
  PrimaryToolbarView* view = self.view;
  view.locationBarContainerHeight.constant = height;
  view.locationBarContainer.layer.cornerRadius = height / 2;
}
@end
