// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"

#import "base/logging.h"
#include "base/metrics/user_metrics.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_long_press_delegate.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#include "ios/chrome/browser/ui/util/animation_util.h"
#import "ios/chrome/browser/ui/util/force_touch_long_press_gesture_recognizer.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/third_party/material_components_ios/src/components/ProgressView/src/MaterialProgressView.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kRotationInRadians = 5.0 / 180 * M_PI;
// Scale factor for the animation, must be < 1.
const CGFloat kScaleFactorDiff = 0.50;
const CGFloat kTabGridAnimationsTotalDuration = 0.5;
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
@synthesize dispatcher = _dispatcher;
@synthesize longPressDelegate = _longPressDelegate;
@synthesize loading = _loading;
@synthesize isNTP = _isNTP;

#pragma mark - Public

- (void)updateForSideSwipeSnapshotOnNTP:(BOOL)onNTP {
  self.view.progressBar.hidden = YES;
  self.view.progressBar.alpha = 0;
  self.view.blur.hidden = YES;
  self.view.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
}

- (void)resetAfterSideSwipeSnapshot {
  self.view.progressBar.alpha = 1;
  self.view.blur.hidden = NO;
  self.view.backgroundColor = [UIColor clearColor];
}

#pragma mark - UIViewController

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self updateAllButtonsVisibility];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self addStandardActionsForAllButtons];

  if (@available(iOS 11.0, *)) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(voiceOverChanged:)
               name:UIAccessibilityVoiceOverStatusDidChangeNotification
             object:nil];
  }
#if !defined(__IPHONE_11_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_11_0
  else {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(voiceOverChanged:)
               name:UIAccessibilityVoiceOverStatusChanged
             object:nil];
  }
#endif
  [self makeViewAccessibilityTraitsContainer];

  // Adds the layout guide to the buttons.
  self.view.toolsMenuButton.guideName = kToolsMenuGuide;
  self.view.tabGridButton.guideName = kTabSwitcherGuide;
  self.view.omniboxButton.guideName = kSearchButtonGuide;
  self.view.forwardButton.guideName = kForwardButtonGuide;
  self.view.backButton.guideName = kBackButtonGuide;

  // Add navigation popup menu triggers.
  [self addLongPressGestureToView:self.view.backButton];
  [self addLongPressGestureToView:self.view.forwardButton];
  [self addLongPressGestureToView:self.view.omniboxButton];
  [self addLongPressGestureToView:self.view.tabGridButton];
  [self addLongPressGestureToView:self.view.toolsMenuButton];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateAllButtonsVisibility];
  if (IsRegularXRegularSizeClass(self)) {
    [self.view.progressBar setHidden:YES animated:NO completion:nil];
  } else if (self.loading) {
    [self.view.progressBar setHidden:NO animated:NO completion:nil];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // TODO(crbug.com/882723): Remove this call once iPad trait collection
  // override issue is fixed.
  [self updateAllButtonsVisibility];
}

#pragma mark - Public

- (ToolbarToolsMenuButton*)toolsMenuButton {
  return self.view.toolsMenuButton;
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
    [self.view.progressBar setHidden:NO animated:YES completion:nil];
    // Layout if needed the progress bar to avoid having the progress bar going
    // backward when opening a page from the NTP.
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

- (void)setPageBookmarked:(BOOL)bookmarked {
  self.view.bookmarkButton.spotlighted = bookmarked;
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

- (void)setSearchIcon:(UIImage*)searchIcon {
  [self.view.omniboxButton setImage:searchIcon forState:UIControlStateNormal];
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  // No-op, should be handled by the primary toolbar.
}

#pragma mark - Protected

- (void)stopProgressBar {
  __weak MDCProgressView* weakProgressBar = self.view.progressBar;
  __weak AdaptiveToolbarViewController* weakSelf = self;
  [self.view.progressBar
      setProgress:1
         animated:YES
       completion:^(BOOL finished) {
         if (!weakSelf.loading) {
           [weakProgressBar setHidden:YES animated:YES completion:nil];
         }
       }];
}

#pragma mark - PopupMenuUIUpdating

- (void)updateUIForMenuDisplayed:(PopupMenuType)popupType {
  ToolbarButton* selectedButton = nil;
  switch (popupType) {
    case PopupMenuTypeNavigationForward:
      selectedButton = self.view.forwardButton;
      break;
    case PopupMenuTypeNavigationBackward:
      selectedButton = self.view.backButton;
      break;
    case PopupMenuTypeSearch:
      selectedButton = self.view.omniboxButton;
      break;
    case PopupMenuTypeTabGrid:
      selectedButton = self.view.tabGridButton;
      break;
    case PopupMenuTypeToolsMenu:
      selectedButton = self.view.toolsMenuButton;
      break;
    case PopupMenuTypeTabStripTabGrid:
      // ignore
      break;
  }

  selectedButton.spotlighted = YES;

  for (ToolbarButton* button in self.view.allButtons) {
    button.dimmed = YES;
  }
}

- (void)updateUIForMenuDismissed {
  self.view.backButton.spotlighted = NO;
  self.view.forwardButton.spotlighted = NO;
  self.view.omniboxButton.spotlighted = NO;
  self.view.tabGridButton.spotlighted = NO;
  self.view.toolsMenuButton.spotlighted = NO;

  for (ToolbarButton* button in self.view.allButtons) {
    button.dimmed = NO;
  }
}

#pragma mark - Accessibility

// Callback called when the voice over value is changed.
- (void)voiceOverChanged:(NSNotification*)notification {
  if (!UIAccessibilityIsVoiceOverRunning())
    return;

  __weak AdaptiveToolbarViewController* weakSelf = self;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   // The accessibility traits of the UIToolbar is only
                   // available after a certain amount of time after voice over
                   // activation.
                   [weakSelf makeViewAccessibilityTraitsContainer];
                 });
}

// Updates the accessibility traits of the view to have it interpreted as a
// container by voice over.
- (void)makeViewAccessibilityTraitsContainer {
  if (self.view.accessibilityTraits == UIAccessibilityTraitNone) {
    // TODO(crbug.com/857475): Remove this workaround once it is possible to set
    // elements as voice over container. For now, set the accessibility traits
    // of the toolbar to the accessibility traits of a UIToolbar allows it to
    // act as a voice over container.
    UIToolbar* toolbar = [[UIToolbar alloc] init];
    self.view.accessibilityTraits = toolbar.accessibilityTraits;
  }
}

#pragma mark - Private

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
        button != self.view.omniboxButton) {
      [button addTarget:self.dispatcher
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
  } else if (sender == self.view.bookmarkButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarToggleBookmark"));
  } else if (sender == self.view.toolsMenuButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShowMenu"));
  } else if (sender == self.view.tabGridButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShowStackView"));
  } else if (sender == self.view.shareButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShareMenu"));
  } else if (sender == self.view.omniboxButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarOmniboxShortcut"));
  } else {
    NOTREACHED();
  }
}

// Adds a LongPressGesture to the |view|, with target on -|handleLongPress:|.
- (void)addLongPressGestureToView:(UIView*)view {
  ForceTouchLongPressGestureRecognizer* gestureRecognizer =
      [[ForceTouchLongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleGestureRecognizer:)];
  gestureRecognizer.forceThreshold = 0.8;
  [view addGestureRecognizer:gestureRecognizer];
}

// Handles the gseture recognizer on the views.
- (void)handleGestureRecognizer:(UILongPressGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    if (gesture.view == self.view.backButton) {
      [self.dispatcher showNavigationHistoryBackPopupMenu];
    } else if (gesture.view == self.view.forwardButton) {
      [self.dispatcher showNavigationHistoryForwardPopupMenu];
    } else if (gesture.view == self.view.omniboxButton) {
      [self.dispatcher showSearchButtonPopup];
    } else if (gesture.view == self.view.tabGridButton) {
      [self.dispatcher showTabGridButtonPopup];
    } else if (gesture.view == self.view.toolsMenuButton) {
      base::RecordAction(base::UserMetricsAction("MobileToolbarShowMenu"));
      [self.dispatcher showToolsMenuPopup];
    }
    TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleHeavy);
  } else if (gesture.state == UIGestureRecognizerStateEnded) {
    [self.longPressDelegate
        longPressEndedAtPoint:[gesture locationOfTouch:0 inView:nil]];
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    [self.longPressDelegate
        longPressFocusPointChangedTo:[gesture locationOfTouch:0 inView:nil]];
  }
}

@end
