// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_help_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/utils.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/bubble/bubble_constants.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_updating.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
base::TimeDelta kPromoDisplayDelayForTests = base::Seconds(1);
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

// Whether the user is still in the same session as when the history menu item
// IPH was triggered
@property(nonatomic, assign) BOOL inSessionWithHistoryMenuItemIPH;

// The tracker for feature engagement. May return null after the coordinator has
// been stopped (thus the returned value must be checked for null).
@property(nonatomic, readonly)
    feature_engagement::Tracker* featureEngagementTracker;

@end

@implementation PopupMenuHelpCoordinator {
  segmentation_platform::DeviceSwitcherResultDispatcher*
      _deviceSwitcherResultDispatcher;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    if (!browser->GetBrowserState()->IsOffTheRecord()) {
      _deviceSwitcherResultDispatcher =
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetDispatcherForBrowserState(browser->GetBrowserState());
    }
  }
  return self;
}

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

- (NSNumber*)highlightDestination {
  if (self.inSessionWithHistoryMenuItemIPH) {
    return [NSNumber numberWithInt:static_cast<NSInteger>(
                                       overflow_menu::Destination::History)];
  }
  return nil;
}

- (void)showHistoryOnOverflowMenuIPHInViewController:(UIViewController*)menu {
  // Show the IPH in the overflow menu if user is still in a session where they
  // saw the IPH of the three-dot menu item.
  if (!self.inSessionWithHistoryMenuItemIPH) {
    return;
  }

  CGFloat anchorXInParent =
      CGRectGetMidX(self.uiConfiguration.highlightedDestinationFrame);
  CGFloat anchorX =
      [menu.view.window convertPoint:CGPointMake(anchorXInParent, 0)
                            fromView:menu.view]
          .x;
  // in global coordinate system
  CGPoint anchorPoint = CGPointMake(
      anchorX, CGRectGetMaxY(self.uiConfiguration.destinationListScreenFrame));

  self.overflowMenuBubblePresenter = [self
      newOverflowMenuBubblePresenterWithAnchorXInParent:anchorXInParent
                                        parentViewWidth:
                                            self.uiConfiguration
                                                .destinationListScreenFrame.size
                                                .width];

  if (![self.overflowMenuBubblePresenter canPresentInView:menu.view
                                              anchorPoint:anchorPoint]) {
    // Reset the highlight status of the destination as we will miss the other
    // path of resetting it when dismissing the IPH.
    self.uiConfiguration.highlightDestination = -1;
    // No effect besides leaving it in a clean state.
    self.uiConfiguration.highlightedDestinationFrame = CGRectZero;
    return;
  }

  self.inSessionWithHistoryMenuItemIPH = NO;

  [self.overflowMenuBubblePresenter presentInViewController:menu
                                                       view:menu.view
                                                anchorPoint:anchorPoint];
}

#pragma mark - Popup Menu Button Bubble/IPH methods

- (BubbleViewControllerPresenter*)newPopupMenuBubblePresenter {
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_VIEW_BROWSING_HISTORY_OVERFLOW_MENU_TIP);

  // Prepare the dismissal callback.
  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType IPHDismissalReasonType,
        feature_engagement::Tracker::SnoozeAction snoozeAction) {
        [weakSelf popupMenuIPHDidDismissWithReasonType:IPHDismissalReasonType
                                          SnoozeAction:snoozeAction];
      };

  // Create the BubbleViewControllerPresenter.
  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.baseViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
          initDefaultBubbleWithText:text
                     arrowDirection:arrowDirection
                          alignment:BubbleAlignmentBottomOrTrailing
               isLongDurationBubble:NO
                  dismissalCallback:dismissalCallback];
  std::u16string menuButtonA11yLabel = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS));
  bubbleViewControllerPresenter.voiceOverAnnouncement = l10n_util::GetNSStringF(
      IDS_IOS_VIEW_BROWSING_HISTORY_FROM_MENU_ANNOUNCEMENT,
      menuButtonA11yLabel);
  return bubbleViewControllerPresenter;
}

- (void)popupMenuIPHDidDismissWithReasonType:
            (IPHDismissalReasonType)IPHDismissalReasonType
                                SnoozeAction:
                                    (feature_engagement::Tracker::SnoozeAction)
                                        snoozeAction {
  if (IPHDismissalReasonType == IPHDismissalReasonType::kTappedAnchorView) {
    self.inSessionWithHistoryMenuItemIPH = YES;
  }
  [self trackerIPHDidDismissWithSnoozeAction:snoozeAction];
  [self.UIUpdater updateUIForIPHDismissed];
  self.popupMenuBubblePresenter = nil;
}

- (void)prepareToShowPopupMenuBubble {
  // There must be a feature engagment tracker to show a bubble.
  if (!self.featureEngagementTracker) {
    return;
  }

  // If the Feature Engagement Tracker isn't ready, queue up and re-show when
  // it has finished initializing.
  if (!self.featureEngagementTracker->IsInitialized()) {
    __weak __typeof(self) weakSelf = self;
    self.featureEngagementTracker->AddOnInitializedCallback(
        base::BindRepeating(^(bool success) {
          if (!success) {
            return;
          }
          [weakSelf prepareToShowPopupMenuBubble];
        }));
    return;
  }

  if (tests_hook::DelayAppLaunchPromos()) {
    __weak __typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kPromoDisplayDelayForTests.InNanoseconds()),
                   dispatch_get_main_queue(), ^{
                     [weakSelf showPopupMenuBubbleIfNecessary];
                   });
  } else {
    [self showPopupMenuBubbleIfNecessary];
  }
}

- (void)showPopupMenuBubbleIfNecessary {
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
  if (![self canShowIPHForPopupMenu]) {
    return;
  }

  // Present the bubble after the delay.
  self.popupMenuBubblePresenter = bubblePresenter;
  [self.popupMenuBubblePresenter
      presentInViewController:self.baseViewController
                         view:self.baseViewController.view
                  anchorPoint:anchorPoint
              anchorViewFrame:anchorFrame];
  [self.UIUpdater updateUIForOverflowMenuIPHDisplayed];
}

#pragma mark - Overflow Menu Bubble methods

- (BubbleViewControllerPresenter*)
    newOverflowMenuBubblePresenterWithAnchorXInParent:(CGFloat)anchorXInParent
                                      parentViewWidth:(CGFloat)parentViewWidth {
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_VIEW_BROWSING_HISTORY_OVERFLOW_MENU_TIP);

  // Prepare the dismissal callback.
  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType IPHDismissalReasonType,
        feature_engagement::Tracker::SnoozeAction snoozeAction) {
        [weakSelf overflowMenuIPHDidDismissWithSnoozeAction:snoozeAction];
      };

  BubbleAlignment alignment = anchorXInParent < 0.5 * parentViewWidth
                                  ? BubbleAlignmentTopOrLeading
                                  : BubbleAlignmentBottomOrTrailing;

  // Create the BubbleViewControllerPresenter.
  BubbleArrowDirection arrowDirection = BubbleArrowDirectionUp;
  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
          initDefaultBubbleWithText:text
                     arrowDirection:arrowDirection
                          alignment:alignment
               isLongDurationBubble:NO
                  dismissalCallback:dismissalCallback];
  std::u16string historyButtonA11yLabel = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_HISTORY));
  bubbleViewControllerPresenter.voiceOverAnnouncement = l10n_util::GetNSStringF(
      IDS_IOS_VIEW_BROWSING_HISTORY_BY_SELECTING_HISTORY_TIP_ANNOUNCEMENT,
      historyButtonA11yLabel);
  return bubbleViewControllerPresenter;
}

- (void)overflowMenuIPHDidDismissWithSnoozeAction:
    (feature_engagement::Tracker::SnoozeAction)snoozeAction {
  self.overflowMenuBubblePresenter = nil;
  self.uiConfiguration.highlightDestination = -1;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelBackground) {
    self.inSessionWithHistoryMenuItemIPH = NO;
  } else if (level >= SceneActivationLevelForegroundActive) {
    [self prepareToShowPopupMenuBubble];
  }
}

#pragma mark - Feature Engagement Tracker queries

// Queries the feature engagement tracker to see if IPH can currently be
// displayed. Once this is returning YES, the IPH MUST be shown and dismissed.
- (BOOL)canShowIPHForPopupMenu {
  if (!iph_for_new_chrome_user::IsUserEligible(
          _deviceSwitcherResultDispatcher)) {
    return NO;
  }
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  const base::Feature& feature =
      feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature;
  return tracker && tracker->ShouldTriggerHelpUI(feature);
}

// Alerts the feature engagement tracker that a shown IPH was dismissed.
- (void)trackerIPHDidDismissWithSnoozeAction:
    (feature_engagement::Tracker::SnoozeAction)snoozeAction {
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  if (tracker) {
    const base::Feature& feature =
        feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature;
    tracker->DismissedWithSnooze(feature, snoozeAction);
  }
}

@end
