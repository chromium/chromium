// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"

#import <MaterialComponents/MaterialProgressView.h>

#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/ui/util/animation_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_menus_provider.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/material_timing.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kRotationInRadians = 5.0 / 180 * M_PI;
// Scale factor for the animation, must be < 1.
const CGFloat kScaleFactorDiff = 0.50;
const CGFloat kTabGridAnimationsTotalDuration = 0.5;
// The identifier for the context menu action trigger.
NSString* const kContextMenuActionIdentifier = @"kContextMenuActionIdentifier";
// The duration of the slide in animation.
const base::TimeDelta kToobarSlideInAnimationDuration = base::Milliseconds(500);
}  // namespace

@interface AdaptiveToolbarViewController ()

// Redefined to be an AdaptiveToolbarView.
@property(nonatomic, strong) UIView<AdaptiveToolbarView>* view;
// Whether a page is loading.
@property(nonatomic, assign, getter=isLoading) BOOL loading;
@property(nonatomic, assign) BOOL isNTP;

@end

@implementation AdaptiveToolbarViewController

@dynamic view;
@synthesize buttonFactory = _buttonFactory;
@synthesize loading = _loading;
@synthesize isNTP = _isNTP;

#pragma mark - Public

- (void)updateForSideSwipeSnapshotOnNTP:(BOOL)onNTP {
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

#pragma mark - UIViewController

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self updateAllButtonsVisibility];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self addStandardActionsForAllButtons];

  // Add the layout guide names to the buttons.
  self.view.toolsMenuButton.guideName = kToolsMenuGuide;
  self.view.tabGridButton.guideName = kTabSwitcherGuide;
  self.view.openNewTabButton.guideName = kNewTabButtonGuide;
  self.view.forwardButton.guideName = kForwardButtonGuide;
  self.view.backButton.guideName = kBackButtonGuide;

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

  [self updateLayoutBasedOnTraitCollection];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateLayoutBasedOnTraitCollection];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // TODO(crbug.com/882723): Remove this call once iPad trait collection
  // override issue is fixed.
  [self updateAllButtonsVisibility];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  [self updateAllButtonsVisibility];
}

#pragma mark - Public

- (ToolbarButton*)toolsMenuButton {
  return self.view.toolsMenuButton;
}

- (void)setLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  _layoutGuideCenter = layoutGuideCenter;

  if (self.isViewLoaded) {
    [self addLayoutGuideCenterToButtons];
  }
}

#pragma mark - ToolbarConsumer

- (void)setCanGoForward:(BOOL)canGoForward {
  self.view.forwardButton.enabled = canGoForward;
}

- (void)setCanGoBack:(BOOL)canGoBack {
  self.view.backButton.enabled = canGoBack;
}

- (void)setLoadingState:(BOOL)loading {
  if (self.loading == loading)
    return;

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
  if (self.view.tabGridButton.tabCount == tabCount)
    return;

  CGFloat scaleSign = tabCount > self.view.tabGridButton.tabCount ? 1 : -1;
  self.view.tabGridButton.tabCount = tabCount;

  if (IsRegularXRegularSizeClass(self))
    // No animation on Regular x Regular.
    return;

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

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  // No-op, should be handled by the primary toolbar.
}

#pragma mark - Protected

- (void)stopProgressBar {
  __weak AdaptiveToolbarViewController* weakSelf = self;
  [self.view.progressBar setProgress:1
                            animated:YES
                          completion:^(BOOL finished) {
                            [weakSelf updateProgressBarVisibility];
                          }];
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

#pragma mark - Private

// Makes sure that the visibility of the progress bar is matching the one which
// is expected.
- (void)updateProgressBarVisibility {
  __weak __typeof(self) weakSelf = self;
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
    NOTREACHED();
  }
}

- (void)updateLayoutBasedOnTraitCollection {
  [self updateAllButtonsVisibility];
  if (IsRegularXRegularSizeClass(self)) {
    [self.view.progressBar setHidden:YES animated:NO completion:nil];
  } else if (self.loading) {
    [self.view.progressBar setHidden:NO animated:NO completion:nil];
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
}

@end
