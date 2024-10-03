// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_help_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/utils.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_action_provider.h"
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

// Bubble view controller presenter for the Overflow Menu tips.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* overflowMenuBubblePresenter;

// The profile. May return null after the coordinator has been stopped
// (thus the returned value must be checked for null).
@property(nonatomic, readonly) ProfileIOS* profile;

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

// Whether overflow menu button has a blue dot.
@property(nonatomic, assign) BOOL hasBlueDot;

@end

@implementation PopupMenuHelpCoordinator {
  raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      _deviceSwitcherResultDispatcher;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    if (!browser->GetProfile()->IsOffTheRecord()) {
      _deviceSwitcherResultDispatcher =
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetDispatcherForProfile(browser->GetProfile());
    }
  }
  return self;
}

#pragma mark - Getters

- (ProfileIOS*)profile {
  return self.browser ? self.browser->GetProfile() : nullptr;
}

- (feature_engagement::Tracker*)featureEngagementTracker {
  ProfileIOS* profile = self.profile;
  if (!profile) {
    return nullptr;
  }
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  DCHECK(tracker);
  return tracker;
}

#pragma mark - Public methods

- (void)start {
  SceneState* sceneState = self.browser->GetSceneState();
  [sceneState addObserver:self];

  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.layoutGuide = [layoutGuideCenter makeLayoutGuideNamed:kToolsMenuGuide];
  [self.baseViewController.view addLayoutGuide:self.layoutGuide];
}

- (void)stop {
  SceneState* sceneState = self.browser->GetSceneState();
  [sceneState removeObserver:self];
}

- (NSNumber*)highlightDestination {
  if (self.inSessionWithHistoryMenuItemIPH) {
    return [NSNumber numberWithInt:static_cast<NSInteger>(
                                       overflow_menu::Destination::History)];
  }
  return nil;
}

- (void)showIPHAfterOpenOfOverflowMenu:(UIViewController*)menu {
  if ([self showHistoryOnOverflowMenuIPHInViewController:menu]) {
    return;
  }
  // Only try to show customization IPH if history IPH was not shown.
  [self showCustomizationIPHInMenu:menu];
}

- (BOOL)hasBlueDotForOverflowMenu {
  return self.hasBlueDot;
}

#pragma mark - Private

// Shows the History IPH when the overflow menu opens. Returns `YES` if the IPH
// was successfully shown or `NO` if it was not.
- (BOOL)showHistoryOnOverflowMenuIPHInViewController:(UIViewController*)menu {
  // Show the IPH in the overflow menu if user is still in a session where they
  // saw the IPH of the three-dot menu item.
  if (!self.inSessionWithHistoryMenuItemIPH) {
    return NO;
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
    return NO;
  }

  self.inSessionWithHistoryMenuItemIPH = NO;

  [self.overflowMenuBubblePresenter presentInViewController:menu
                                                anchorPoint:anchorPoint];
  return YES;
}

// Possibly shows the IPH for the Overflow Menu Customization feature. Returns
// whether or not the IPH was shown.
- (BOOL)showCustomizationIPHInMenu:(UIViewController*)menu {
  if (!IsNewOverflowMenuEnabled()) {
    return NO;
  }

  // In global coordinate system
  CGPoint anchorPointInView = CGPointMake(CGRectGetMaxX(menu.view.frame) / 2,
                                          CGRectGetMaxY(menu.view.frame) - 20);
  CGPoint anchorPoint = [menu.view.window convertPoint:anchorPointInView
                                              fromView:menu.view];

  self.overflowMenuBubblePresenter =
      [self newOverflowMenuCustomizationBubblePresenter];

  if (![self.overflowMenuBubblePresenter canPresentInView:menu.view
                                              anchorPoint:anchorPoint]) {
    return NO;
  }

  if (![self canShowOverflowMenuCustomizationIPH]) {
    self.overflowMenuBubblePresenter = nil;
    return NO;
  }

  [self.overflowMenuBubblePresenter presentInViewController:menu
                                                anchorPoint:anchorPoint];

  OverflowMenuAction* editActionsAction = [self.actionProvider
      actionForActionType:overflow_menu::ActionType::EditActions];
  editActionsAction.highlighted = YES;
  editActionsAction.displayNewLabelIcon = YES;

  return YES;
}

- (void)scrollToEditActionsButton {
  self.uiConfiguration.scrollToAction = [self.actionProvider
      actionForActionType:overflow_menu::ActionType::EditActions];
}

// Returns whether blue dot should be shown.
- (BOOL)shouldShowBlueDot {
  // As sync error takes precendence on blue dot for settings destination in the
  // overflow menu. In that case don't show blue dot as the full path from
  // toolbar to default browser settings cannot be highlighted.
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.browser->GetProfile());
  if (syncService && ShouldIndicateIdentityErrorInOverflowMenu(syncService)) {
    return NO;
  }

  if (self.featureEngagementTracker &&
      ShouldTriggerDefaultBrowserHighlightFeature(
          self.featureEngagementTracker)) {
    RecordDefaultBrowserBlueDotFirstDisplay();
    return YES;
  }
  return NO;
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
  if (IPHDismissalReasonType == IPHDismissalReasonType::kTappedAnchorView ||
      IPHDismissalReasonType == IPHDismissalReasonType::kTimedOut) {
    self.inSessionWithHistoryMenuItemIPH = YES;
  }
  [self trackerIPHDidDismissWithSnoozeAction:snoozeAction];
  [self.UIUpdater updateUIForIPHDismissed];
  self.popupMenuBubblePresenter = nil;
}

- (void)prepareToShowPopupMenuIPHs {
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
          [weakSelf showPopupMenuIPHs];
        }));
    return;
  }

  if (tests_hook::DelayAppLaunchPromos()) {
    __weak __typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kPromoDisplayDelayForTests.InNanoseconds()),
                   dispatch_get_main_queue(), ^{
                     [weakSelf showPopupMenuIPHs];
                   });
  } else {
    [self showPopupMenuIPHs];
  }
}

- (void)showPopupMenuIPHs {
  [self showPopupMenuBubbleIfNecessary];
  [self updateBlueDotVisibility];
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
                  anchorPoint:anchorPoint
              anchorViewFrame:anchorFrame];
  [self.UIUpdater updateUIForOverflowMenuIPHDisplayed];
}

- (void)updateBlueDotVisibility {
  self.hasBlueDot = YES;

  // Don't show blue dot if already showing another IPH.
  if (self.popupMenuBubblePresenter) {
    self.hasBlueDot = NO;
  }

  if (![self shouldShowBlueDot]) {
    self.hasBlueDot = NO;
  }

  if (IsBlueDotOnToolsMenuButtoneEnabled()) {
    [self.UIUpdater setOverflowMenuBlueDot:self.hasBlueDot];
  }
}

- (void)notifyIPHBubblePresenting {
  // Remove blue dot if IPH bubble will be presenting on tools menu button.
  self.hasBlueDot = NO;
  if (IsBlueDotOnToolsMenuButtoneEnabled()) {
    [self.UIUpdater setOverflowMenuBlueDot:self.hasBlueDot];
  }
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

#pragma mark - Overflow Menu Customization Methods

- (BubbleViewControllerPresenter*)newOverflowMenuCustomizationBubblePresenter {
  NSString* text = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_CUSTOMIZATION_IPH);

  // Prepare the dismissal callback.
  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback = ^(
      IPHDismissalReasonType IPHDismissalReasonType,
      feature_engagement::Tracker::SnoozeAction snoozeAction) {
    if (IPHDismissalReasonType == IPHDismissalReasonType::kTappedIPH) {
      [self scrollToEditActionsButton];
    }
    [weakSelf
        overflowMenuCustomizationIPHDidDismissWithSnoozeAction:snoozeAction];
  };

  BubbleAlignment alignment = BubbleAlignmentCenter;

  // Create the BubbleViewControllerPresenter.
  BubbleArrowDirection arrowDirection = BubbleArrowDirectionDown;
  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
          initDefaultBubbleWithText:text
                     arrowDirection:arrowDirection
                          alignment:alignment
                  dismissalCallback:dismissalCallback];

  bubbleViewControllerPresenter.customBubbleVisibilityDuration =
      kDefaultLongDurationBubbleVisibility;

  return bubbleViewControllerPresenter;
}

- (void)overflowMenuCustomizationIPHDidDismissWithSnoozeAction:
    (feature_engagement::Tracker::SnoozeAction)snoozeAction {
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  if (tracker) {
    const base::Feature& feature =
        feature_engagement::kIPHiOSOverflowMenuCustomizationFeature;
    tracker->Dismissed(feature);
  }
  self.overflowMenuBubblePresenter = nil;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelBackground) {
    self.inSessionWithHistoryMenuItemIPH = NO;
  } else if (level >= SceneActivationLevelForegroundActive) {
    [self prepareToShowPopupMenuIPHs];
  }
}

#pragma mark - Feature Engagement Tracker queries

// Queries the feature engagement tracker to see if IPH can currently be
// displayed. Once this is returning YES, the IPH MUST be shown and dismissed.
- (BOOL)canShowIPHForPopupMenu {
  if (!iph_for_new_chrome_user::IsUserNewSafariSwitcher(
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

// Queries the feature engagement tracker to see if the Overflow Menu
// Customization IPH can be displayed. If this returns YES, the IPH MUST be
// shown and dismissed.
- (BOOL)canShowOverflowMenuCustomizationIPH {
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  const base::Feature& feature =
      feature_engagement::kIPHiOSOverflowMenuCustomizationFeature;
  return tracker && tracker->ShouldTriggerHelpUI(feature);
}

@end
