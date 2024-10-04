// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"

#import <MaterialComponents/MaterialProgressView.h>

#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/animation_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_menus_provider.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button_style.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/device_form_factor.h"

namespace {
const CGFloat kRotationInRadians = 5.0 / 180 * M_PI;
// Scale factor for the animation, must be < 1.
const CGFloat kScaleFactorDiff = 0.50;
const CGFloat kTabGridAnimationsTotalDuration = 0.5;
// The identifier for the context menu action trigger.
NSString* const kContextMenuActionIdentifier = @"kContextMenuActionIdentifier";
// The duration of the slide in animation.
const base::TimeDelta kToobarSlideInAnimationDuration = base::Milliseconds(500);
// Progress of fullscreen when the toolbars are fully visible.
const CGFloat kFullscreenProgressFullyExpanded = 1.0;

}  // namespace

@interface AdaptiveToolbarViewController ()

// Redefined to be an AdaptiveToolbarView.
@property(nonatomic, strong) UIView<AdaptiveToolbarView>* view;
// Whether a page is loading.
@property(nonatomic, assign, getter=isLoading) BOOL loading;
@property(nonatomic, assign) BOOL isNTP;
// The last progress of fullscreen registered. The progress range is between 0
// and 1.
@property(nonatomic, assign) CGFloat previousFullscreenProgress;
// The page's theme color.
@property(nonatomic, strong) UIColor* pageThemeColor;
// The under page background color.
@property(nonatomic, strong) UIColor* underPageBackgroundColor;

@end

@implementation AdaptiveToolbarViewController

@dynamic view;
@synthesize buttonFactory = _buttonFactory;
@synthesize loading = _loading;
@synthesize isNTP = _isNTP;

#pragma mark - Public

- (ToolbarButton*)toolsMenuButton {
  return self.view.toolsMenuButton;
}

- (void)updateForSideSwipeSnapshot:(BOOL)onNonIncognitoNTP {
  self.view.progressBar.hidden = YES;
  self.view.progressBar.alpha = 0;
}

- (void)resetAfterSideSwipeSnapshot {
  self.view.progressBar.alpha = 1;
}

- (void)triggerToolbarSlideInAnimationFromBelow:(BOOL)fromBelow {
  // Toolbar slide-in animations are disabled on iPads.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  const UIView* view = self.view;
  CGFloat toolbarHeight = view.frame.size.height;
  view.transform = CGAffineTransformMakeTranslation(
      0, fromBelow ? toolbarHeight : -toolbarHeight);
  auto animations = ^{
    [UIView addKeyframeWithRelativeStartTime:0
                            relativeDuration:1
                                  animations:^{
                                    view.transform = CGAffineTransformIdentity;
                                  }];
  };

  [UIView
      animateKeyframesWithDuration:kToobarSlideInAnimationDuration.InSecondsF()
                             delay:0
                           options:UIViewAnimationCurveEaseOut
                        animations:animations
                        completion:nil];
}

- (void)showPrerenderingAnimation {
  __weak __typeof__(self) weakSelf = self;
  [self.view.progressBar setProgress:0];
  if (self.hasOmnibox) {
    [self.view.progressBar setHidden:NO
                            animated:YES
                          completion:^(BOOL finished) {
                            [weakSelf stopProgressBar];
                          }];
  }
}

- (BOOL)hasOmnibox {
  return self.locationBarViewController != nil;
}

#pragma mark - UIViewController

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self updateAllButtonsVisibility];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // The first time, the toolbar is fully displayed.
  self.previousFullscreenProgress = kFullscreenProgressFullyExpanded;

  [self addStandardActionsForAllButtons];

  // Add the layout guide names to the buttons.
  self.view.toolsMenuButton.guideName = kToolsMenuGuide;
  self.view.tabGridButton.guideName = kTabSwitcherGuide;
  self.view.openNewTabButton.guideName = kNewTabButtonGuide;
  self.view.forwardButton.guideName = kForwardButtonGuide;
  self.view.backButton.guideName = kBackButtonGuide;
  self.view.shareButton.guideName = kShareButtonGuide;

  [self addLayoutGuideCenterToButtons];

  // Add navigation popup menu triggers.
  [self configureMenuProviderForButton:self.view.backButton
                            buttonType:AdaptiveToolbarButtonTypeBack];
  [self configureMenuProviderForButton:self.view.forwardButton
                            buttonType:AdaptiveToolbarButtonTypeForward];
  [self configureMenuProviderForButton:self.view.openNewTabButton
                            buttonType:AdaptiveToolbarButtonTypeNewTab];
  [self configureMenuProviderForButton:self.view.tabGridButton
                            buttonType:AdaptiveToolbarButtonTypeTabGrid];

  // LocationBarContainer initial fullscreen progress.
  [self updateLocationBarHeightForFullscreenProgress:
            kFullscreenProgressFullyExpanded];

  // CollapsedToolbarButton exit fullscreen.
  [self.view.collapsedToolbarButton
             addTarget:self
                action:@selector(collapsedToolbarButtonTapped)
      forControlEvents:UIControlEventTouchUpInside];
  UIHoverGestureRecognizer* hoverGestureRecognizer =
      [[UIHoverGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(exitFullscreen)];
  [self.view.collapsedToolbarButton
      addGestureRecognizer:hoverGestureRecognizer];

  [self traitCollectionDidChange:nil];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  // Progress bar and buttons visibility.
  [self updateAllButtonsVisibility];
  if (IsRegularXRegularSizeClass(self)) {
    [self.view.progressBar setHidden:YES animated:NO completion:nil];
  } else if (self.loading && self.hasOmnibox) {
    [self.view.progressBar setHidden:NO animated:NO completion:nil];
  }

  // Restore locationBarContainer height with previous fullscreen progress.
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self updateLocationBarHeightForFullscreenProgress:
              self.previousFullscreenProgress];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // TODO(crbug.com/41413004): Remove this call once iPad trait collection
  // override issue is fixed.
  [self updateAllButtonsVisibility];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  [self updateAllButtonsVisibility];
}

#pragma mark - Public Properties

- (void)setLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  _layoutGuideCenter = layoutGuideCenter;

  if (self.isViewLoaded) {
    [self addLayoutGuideCenterToButtons];
  }
}

- (void)setLocationBarViewController:
    (UIViewController*)locationBarViewController {
  _locationBarViewController = locationBarViewController;
  if (locationBarViewController) {
    [self addChildViewController:locationBarViewController];
    [locationBarViewController didMoveToParentViewController:self];
    [self.view setLocationBarView:locationBarViewController.view];
    self.view.locationBarContainer.hidden = NO;
    // Update the constraint of the location bar view to make sure the text is
    // centered.
    [locationBarViewController.view updateConstraintsIfNeeded];
  } else {
    [self.view setLocationBarView:nil];
    self.view.locationBarContainer.hidden = YES;
  }
  [self updateProgressBarVisibility];
}

#pragma mark - ToolbarConsumer

- (void)setCanGoForward:(BOOL)canGoForward {
  self.view.forwardButton.enabled = canGoForward;
}

- (void)setCanGoBack:(BOOL)canGoBack {
  self.view.backButton.enabled = canGoBack;
}

- (void)setLoadingState:(BOOL)loading {
  if (self.loading == loading) {
    return;
  }

  self.loading = loading;
  self.view.reloadButton.hiddenInCurrentState = loading;
  self.view.stopButton.hiddenInCurrentState = !loading;
  [self.view layoutIfNeeded];

  if (!loading) {
    [self stopProgressBar];
  } else if (self.view.progressBar.hidden &&
             !IsRegularXRegularSizeClass(self) && !self.isNTP) {
    [self.view.progressBar setProgress:0];
    [self updateProgressBarVisibility];
    // Layout if needed the progress bar to avoid having the progress bar
    // going backward when opening a page from the NTP.
    [self.view.progressBar layoutIfNeeded];
  }
}

- (void)setLoadingProgressFraction:(double)progress {
  [self.view.progressBar setProgress:progress animated:YES completion:nil];
}

- (void)setTabCount:(int)tabCount addedInBackground:(BOOL)inBackground {
  if (self.view.tabGridButton.tabCount == tabCount) {
    return;
  }

  CGFloat scaleSign = tabCount > self.view.tabGridButton.tabCount ? 1 : -1;
  self.view.tabGridButton.tabCount = tabCount;

  if (IsRegularXRegularSizeClass(self)) {
    // No animation on Regular x Regular.
    return;
  }

  CGFloat scaleFactor = 1 + scaleSign * kScaleFactorDiff;

  CGAffineTransform baseTransform =
      inBackground ? CGAffineTransformMakeRotation(kRotationInRadians)
                   : CGAffineTransformIdentity;

  auto animations = ^{
    [UIView addKeyframeWithRelativeStartTime:0
                            relativeDuration:0.5
                                  animations:^{
                                    self.view.tabGridButton.transform =
                                        CGAffineTransformScale(baseTransform,
                                                               scaleFactor,
                                                               scaleFactor);
                                  }];
    [UIView addKeyframeWithRelativeStartTime:0.5
                            relativeDuration:0.5
                                  animations:^{
                                    self.view.tabGridButton.transform =
                                        CGAffineTransformIdentity;
                                  }];
  };

  [UIView animateKeyframesWithDuration:kTabGridAnimationsTotalDuration
                                 delay:0
                               options:UIViewAnimationCurveEaseInOut
                            animations:animations
                            completion:nil];
}

- (void)setVoiceSearchEnabled:(BOOL)enabled {
  // No-op, should be handled by the location bar.
}

- (void)setShareMenuEnabled:(BOOL)enabled {
  self.view.shareButton.enabled = enabled;
}

- (void)setIsNTP:(BOOL)isNTP {
  _isNTP = isNTP;
}

- (void)setPageThemeColor:(UIColor*)pageThemeColor {
  if ([_pageThemeColor isEqual:pageThemeColor]) {
    return;
  }
  _pageThemeColor = pageThemeColor;
  [self updateBackgroundColor];
}

- (void)setUnderPageBackgroundColor:(UIColor*)underPageBackgroundColor {
  if ([_underPageBackgroundColor isEqual:underPageBackgroundColor]) {
    return;
  }
  _underPageBackgroundColor = underPageBackgroundColor;
  [self updateBackgroundColor];
}

- (void)setTabGridButtonStyle:(ToolbarTabGridButtonStyle)tabGridButtonStyle {
  [self.view setTabGridButtonStyle:tabGridButtonStyle];
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  // No-op, should be handled by the primary toolbar.
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  self.previousFullscreenProgress = progress;

  const CGFloat alphaValue = fmax(progress * 2 - 1, 0);

  [self updateLocationBarHeightForFullscreenProgress:progress];
  self.view.locationBarContainer.backgroundColor =
      [self.buttonFactory.toolbarConfiguration
          locationBarBackgroundColorWithVisibility:alphaValue];
  self.view.collapsedToolbarButton.hidden = progress > 0.05;
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled) {
    [self updateForFullscreenProgress:kFullscreenProgressFullyExpanded];
  }
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

#pragma mark - Protected

- (void)stopProgressBar {
  __weak AdaptiveToolbarViewController* weakSelf = self;
  [self.view.progressBar setProgress:kFullscreenProgressFullyExpanded
                            animated:YES
                          completion:^(BOOL finished) {
                            [weakSelf updateProgressBarVisibility];
                          }];
}

- (void)collapsedToolbarButtonTapped {
  base::RecordAction(base::UserMetricsAction("MobileFullscreenExitedManually"));
  [self exitFullscreen];
}

- (void)updateBackgroundColor {
  // Implemented in subclass.
}

#pragma mark - PopupMenuUIUpdating

- (void)updateUIForOverflowMenuIPHDisplayed {
  self.view.toolsMenuButton.iphHighlighted = YES;
}

- (void)updateUIForIPHDismissed {
  self.view.backButton.iphHighlighted = NO;
  self.view.forwardButton.iphHighlighted = NO;
  self.view.openNewTabButton.iphHighlighted = NO;
  self.view.tabGridButton.iphHighlighted = NO;
  self.view.toolsMenuButton.iphHighlighted = NO;
}

- (void)setOverflowMenuBlueDot:(BOOL)hasBlueDot {
  // Blue dot should also use the highlighted icon.
  self.view.toolsMenuButton.iphHighlighted = hasBlueDot;

  self.view.toolsMenuButton.hasBlueDot = hasBlueDot;
}

#pragma mark - Private

// Updates `locationBarContainer` height and adjusts its corner radius for the
// fullscreen `progress`
- (void)updateLocationBarHeightForFullscreenProgress:(CGFloat)progress {
  const CGFloat expandedHeight =
      LocationBarHeight(self.traitCollection.preferredContentSizeCategory);
  const CGFloat collapsedHeight =
      ToolbarCollapsedHeight(self.traitCollection.preferredContentSizeCategory);
  const CGFloat expandedCollapsedDelta = expandedHeight - collapsedHeight;

  const CGFloat height =
      AlignValueToPixel(collapsedHeight + expandedCollapsedDelta * progress);

  self.view.locationBarContainerHeight.constant = height;
  self.view.locationBarContainer.layer.cornerRadius = height / 2;
}

// Makes sure that the visibility of the progress bar is matching the one which
// is expected.
- (void)updateProgressBarVisibility {
  __weak __typeof(self) weakSelf = self;

  BOOL hasOmnibox = self.locationBarViewController != nil;
  if (!hasOmnibox) {
    self.view.progressBar.hidden = YES;
    return;
  }

  if (self.loading && self.view.progressBar.hidden) {
    [self.view.progressBar setHidden:NO
                            animated:YES
                          completion:^(BOOL finished) {
                            [weakSelf updateProgressBarVisibility];
                          }];
  } else if (!self.loading && !self.view.progressBar.hidden) {
    [self.view.progressBar setHidden:YES
                            animated:YES
                          completion:^(BOOL finished) {
                            [weakSelf updateProgressBarVisibility];
                          }];
  }
}

// Updates all buttons visibility to match any recent WebState or SizeClass
// change.
- (void)updateAllButtonsVisibility {
  for (ToolbarButton* button in self.view.allButtons) {
    [button updateHiddenInCurrentSizeClass];
  }
}

// Registers the actions which will be triggered when tapping a button.
- (void)addStandardActionsForAllButtons {
  for (ToolbarButton* button in self.view.allButtons) {
    if (button != self.view.toolsMenuButton &&
        button != self.view.openNewTabButton) {
      [button addTarget:self.omniboxCommandsHandler
                    action:@selector(cancelOmniboxEdit)
          forControlEvents:UIControlEventTouchUpInside];
    }
    [button addTarget:self
                  action:@selector(recordUserMetrics:)
        forControlEvents:UIControlEventTouchUpInside];
  }
}

// Records the use of a button.
- (IBAction)recordUserMetrics:(id)sender {
  if (!sender)
    return;

  if (sender == self.view.backButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarBack"));
  } else if (sender == self.view.forwardButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarForward"));
  } else if (sender == self.view.reloadButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarReload"));
  } else if (sender == self.view.stopButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarStop"));
  } else if (sender == self.view.toolsMenuButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShowMenu"));
  } else if (sender == self.view.tabGridButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShowStackView"));
  } else if (sender == self.view.shareButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShareMenu"));
  } else if (sender == self.view.openNewTabButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarNewTabShortcut"));
    base::RecordAction(base::UserMetricsAction("MobileTabNewTab"));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

// Configures `button` with the menu provider, making sure that the items are
// updated when the menu is presented. The `buttonType` is passed to the menu
// provider.
- (void)configureMenuProviderForButton:(UIButton*)button
                            buttonType:(AdaptiveToolbarButtonType)buttonType {
  // Adds an empty menu so the event triggers the first time.
  UIMenu* emptyMenu = [UIMenu menuWithChildren:@[]];
  button.menu = emptyMenu;

  [button removeActionForIdentifier:kContextMenuActionIdentifier
                   forControlEvents:UIControlEventMenuActionTriggered];

  __weak UIButton* weakButton = button;
  __weak __typeof(self) weakSelf = self;
  UIAction* action = [UIAction
      actionWithTitle:@""
                image:nil
           identifier:kContextMenuActionIdentifier
              handler:^(UIAction* uiAction) {
                base::RecordAction(
                    base::UserMetricsAction("MobileMenuToolbarMenuTriggered"));
                TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleHeavy);
                weakButton.menu =
                    [weakSelf.menuProvider menuForButtonOfType:buttonType];
              }];
  [button addAction:action forControlEvents:UIControlEventMenuActionTriggered];
}

- (void)addLayoutGuideCenterToButtons {
  self.view.toolsMenuButton.layoutGuideCenter = self.layoutGuideCenter;
  self.view.tabGridButton.layoutGuideCenter = self.layoutGuideCenter;
  self.view.openNewTabButton.layoutGuideCenter = self.layoutGuideCenter;
  self.view.forwardButton.layoutGuideCenter = self.layoutGuideCenter;
  self.view.backButton.layoutGuideCenter = self.layoutGuideCenter;
  self.view.shareButton.layoutGuideCenter = self.layoutGuideCenter;
}

// Exits fullscreen.
- (void)exitFullscreen {
  [self.adaptiveDelegate exitFullscreen];
}

@end
