// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_help_coordinator.h"

#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/ui/main/layout_guide_util.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_updating.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/util_swift.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Delay between the time the app launches, and the time the
// menu button tip is shown.
constexpr base::TimeDelta kMenuTipDelay = base::Seconds(1);
}  // namespace

@interface PopupMenuHelpCoordinator () <SceneStateObserver>

// Bubble view controller presenter for popup menu tip.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* popupMenuBubblePresenter;

@property(nonatomic, strong)
    BubbleViewControllerPresenter* overflowMenuBubblePresenter;

// The browser state. May return null after the coordinator has been stopped
// (thus the returned value must be checked for null).
@property(nonatomic, readonly) ChromeBrowserState* browserState;

// The layout guide installed in the base view controller on which to anchor the
// potential IPH bubble.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

// Whether the user is still in the same session as when the popup menu IPH was
// triggered
@property(nonatomic, assign) BOOL inSessionWithPopupMenuIPH;

// The tracker for feature engagement. May return null after the coordinator has
// been stopped (thus the returned value must be checked for null).
@property(nonatomic, readonly)
    feature_engagement::Tracker* featureEngagementTracker;

@end

@implementation PopupMenuHelpCoordinator

#pragma mark - Getters

- (ChromeBrowserState*)browserState {
  return self.browser ? self.browser->GetBrowserState() : nullptr;
}

- (feature_engagement::Tracker*)featureEngagementTracker {
  ChromeBrowserState* browserState = self.browserState;
  if (!browserState)
    return nullptr;
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);
  DCHECK(tracker);
  return tracker;
}

#pragma mark - Public methods

- (void)start {
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState addObserver:self];

  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.layoutGuide = [layoutGuideCenter makeLayoutGuideNamed:kToolsMenuGuide];
  [self.baseViewController.view addLayoutGuide:self.layoutGuide];
}

- (void)stop {
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState removeObserver:self];
}

- (void)showPopupMenuButtonIPH {
  [self showPopupMenuBubbleIfNecessary];
}

- (void)showOverflowMenuIPHInViewController:(UIViewController*)menu {
  // There are 2 reasons to show the IPH in the overflow menu:
  // 1. The alternate flow is enabled and the feature tracker says it can show.
  // 2. The user is still in a session where they saw the initial IPH.
  BOOL shouldShowIPH =
      (IsNewOverflowMenuAlternateIPHEnabled() && [self canShowIPH]) ||
      self.inSessionWithPopupMenuIPH;
  if (!shouldShowIPH) {
    return;
  }

  self.overflowMenuBubblePresenter = [self newOverflowMenuBubblePresenter];
  // The overflow menu IPH should be horizontally centered, but beneath the
  // destination list.
  CGPoint anchorPoint = CGPointMake(
      CGRectGetMidX(self.uiConfiguration.destinationListScreenFrame),
      CGRectGetMaxY(self.uiConfiguration.destinationListScreenFrame));

  if (![self.overflowMenuBubblePresenter canPresentInView:menu.view
                                              anchorPoint:anchorPoint]) {
    return;
  }

  self.inSessionWithPopupMenuIPH = NO;
  self.uiConfiguration.highlightDestinationsRow = YES;
  [self.overflowMenuBubblePresenter presentInViewController:menu
                                                       view:menu.view
                                                anchorPoint:anchorPoint];
}

#pragma mark - Popup Menu Button Bubble/IPH methods

- (BubbleViewControllerPresenter*)newPopupMenuBubblePresenter {
  NSString* text = l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_TIP);

  // Prepare the dismissal callback.
  __weak __typeof(self) weakSelf = self;
  ProceduralBlockWithSnoozeAction dismissalCallback =
      ^(feature_engagement::Tracker::SnoozeAction snoozeAction) {
        [weakSelf popupMenuIPHDidDismissWithSnoozeAction:snoozeAction];
      };

  // Create the BubbleViewControllerPresenter.
  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.baseViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
          initDefaultBubbleWithText:text
                     arrowDirection:arrowDirection
                          alignment:BubbleAlignmentTrailing
               isLongDurationBubble:NO
                  dismissalCallback:dismissalCallback];
  bubbleViewControllerPresenter.voiceOverAnnouncement = text;
  return bubbleViewControllerPresenter;
}

- (void)popupMenuIPHDidDismissWithSnoozeAction:
    (feature_engagement::Tracker::SnoozeAction)snoozeAction {
  [self trackerIPHDidDismissWithSnoozeAction:snoozeAction];
  [self.UIUpdater updateUIForIPHDismissed];
  self.popupMenuBubblePresenter = nil;
}

- (void)showPopupMenuBubbleIfNecessary {
  // The alternate IPH flow only shows the IPH when entering the menu.
  if (IsNewOverflowMenuAlternateIPHEnabled()) {
    return;
  }

  // Skip if a presentation is already in progress
  if (self.popupMenuBubblePresenter) {
    return;
  }

  BubbleViewControllerPresenter* bubblePresenter =
      [self newPopupMenuBubblePresenter];

  // Get the anchor point for the bubble. In Split Toolbar Mode, the anchor
  // button is at the bottom of the screen, so the bubble should be above it.
  // When there's only one toolbar, the anchor button is at the top of the
  // screen, so the bubble should be below it.
  CGRect anchorFrame = self.layoutGuide.layoutFrame;
  CGFloat anchorPointY = IsSplitToolbarMode(self.baseViewController)
                             ? CGRectGetMinY(anchorFrame)
                             : CGRectGetMaxY(anchorFrame);
  CGPoint anchorPoint = CGPointMake(CGRectGetMidX(anchorFrame), anchorPointY);

  // Discard if it doesn't fit in the view as it is currently shown.
  if (![bubblePresenter canPresentInView:self.baseViewController.view
                             anchorPoint:anchorPoint]) {
    return;
  }

  // Early return if the engagement tracker won't display the IPH.
  if (![self canShowIPH]) {
    return;
  }

  // Present the bubble after the delay.
  self.popupMenuBubblePresenter = bubblePresenter;
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf presentPopupMenuBubbleAtAnchorPoint:anchorPoint];
        [weakSelf.UIUpdater updateUIForIPHDisplayed:PopupMenuTypeToolsMenu];
      }),
      kMenuTipDelay);
}

// Actually presents the bubble.
- (void)presentPopupMenuBubbleAtAnchorPoint:(CGPoint)anchorPoint {
  self.inSessionWithPopupMenuIPH = YES;
  [self.popupMenuBubblePresenter
      presentInViewController:self.baseViewController
                         view:self.baseViewController.view
                  anchorPoint:anchorPoint];
}

#pragma mark - Overflow Menu Bubble methods

- (BubbleViewControllerPresenter*)newOverflowMenuBubblePresenter {
  NSString* text = l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_CAROUSEL_TIP);

  // Prepare the dismissal callback.
  __weak __typeof(self) weakSelf = self;
  ProceduralBlockWithSnoozeAction dismissalCallback =
      ^(feature_engagement::Tracker::SnoozeAction snoozeAction) {
        [weakSelf overflowMenuIPHDidDismissWithSnoozeAction:snoozeAction];
      };

  // Create the BubbleViewControllerPresenter.
  BubbleArrowDirection arrowDirection = BubbleArrowDirectionUp;
  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
          initDefaultBubbleWithText:text
                     arrowDirection:arrowDirection
                          alignment:BubbleAlignmentCenter
               isLongDurationBubble:NO
                  dismissalCallback:dismissalCallback];
  bubbleViewControllerPresenter.voiceOverAnnouncement =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_CAROUSEL_TIP_VOICEOVER);
  return bubbleViewControllerPresenter;
}

- (void)overflowMenuIPHDidDismissWithSnoozeAction:
    (feature_engagement::Tracker::SnoozeAction)snoozeAction {
  if (IsNewOverflowMenuAlternateIPHEnabled()) {
    [self trackerIPHDidDismissWithSnoozeAction:snoozeAction];
  }
  self.overflowMenuBubblePresenter = nil;
  self.uiConfiguration.highlightDestinationsRow = NO;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelBackground) {
    self.inSessionWithPopupMenuIPH = NO;
  } else if (level >= SceneActivationLevelForegroundActive) {
    [self showPopupMenuBubbleIfNecessary];
  }
}

#pragma mark - Feature Engagement Tracker queries

// Queries the feature engagement tracker to see if IPH can currently be
// displayed. Once this is called, the IPH MUST be shown and dismissed.
- (BOOL)canShowIPH {
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  const base::Feature& feature = feature_engagement::kIPHOverflowMenuTipFeature;
  return tracker && tracker->ShouldTriggerHelpUI(feature);
}

// Alerts the feature engagement tracker that a shown IPH was dismissed.
- (void)trackerIPHDidDismissWithSnoozeAction:
    (feature_engagement::Tracker::SnoozeAction)snoozeAction {
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  if (tracker) {
    const base::Feature& feature =
        feature_engagement::kIPHOverflowMenuTipFeature;
    tracker->DismissedWithSnooze(feature, snoozeAction);
  }
}

@end
