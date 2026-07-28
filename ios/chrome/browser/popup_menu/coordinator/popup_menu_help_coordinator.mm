// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/popup_menu/coordinator/popup_menu_help_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/send_tab_to_self/features.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/bubble/model/utils.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_util.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/public/features.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/public/overflow_menu_action_provider.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/public/overflow_menu_constants.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/ui/ui_swift.h"
#import "ios/chrome/browser/popup_menu/public/popup_menu_ui_updating.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/omnibox_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

base::TimeDelta kPromoDisplayDelayForTests = base::Seconds(1);

// Total number of pages in the Level Up Password Checkup walkthrough sequence.
const NSInteger kLevelUpPasswordCheckupWalkthroughTotalPages = 4;

// Total number of pages in the Level Up Quick Delete walkthrough sequence.
const NSInteger kLevelUpQuickDeleteWalkthroughTotalPages = 2;

// Total number of pages in the Level Up Payment Methods walkthrough sequence.
const NSInteger kLevelUpPaymentMethodsWalkthroughTotalPages = 4;

// The active IPH session type inside the popup menu.
enum class PopupMenuIPHSessionType {
  // No active IPH session.
  kNone,
  // Active session when history menu item IPH is triggered.
  kHistoryMenuItem,
  // Active session when Tab Reminders IPH is triggered.
  kTabReminders,
  // Active session when Level Up Password Checkup walkthrough IPH is triggered.
  kLevelUpPasswordCheckupWalkthrough,
  // Active session when Level Up Quick Delete walkthrough IPH is triggered.
  kLevelUpQuickDeleteWalkthrough,
  // Active session when Level Up Payment Methods walkthrough IPH is triggered.
  kLevelUpPaymentMethodsWalkthrough,
};
}  // namespace

@interface PopupMenuHelpCoordinator () <SceneStateObserver>

// Bubble view controller presenter for popup menu tip.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* popupMenuBubblePresenter;

// Bubble view controller presenter for the Overflow Menu tips.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* overflowMenuBubblePresenter;

// The layout guide installed in the base view controller on which to anchor the
// potential IPH bubble.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

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

  // Whether the coordinator has been stopped.
  // TODO(crbug.com/424761561): Remove when crashes stop.
  BOOL _stopped;

  // The type of active IPH session in progress.
  PopupMenuIPHSessionType _activeIPHSessionType;
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

- (feature_engagement::Tracker*)featureEngagementTracker {
  CHECK(!_stopped, base::NotFatalUntil::M147)
      << "PopupMenuHelpCoordinator used after -stop";
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
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

  // TODO(crbug.com/380450091): SceneState does not notify observer that it
  // reached an activation level when calling -addObserver:. This means that
  // if the scene is already in SceneActivationLevelForegroundActive level
  // when -start is called, the method -prepareToShowPopupMenuIPHs would not
  // be called and the popup not displayed until the user switch to another
  // Scene and back.
  //
  // This means there is a race-condition between PopupMenuHelpCoordinator
  // being started and the SceneState reaching active foreground level. To
  // fix this race-condition, explicitly check if the SceneState has reached
  // the level or not by calling -sceneState:transitionedToActivationLevel:.
  [self sceneState:sceneState
      transitionedToActivationLevel:sceneState.activationLevel];
}

- (void)stop {
  _stopped = YES;

  SceneState* sceneState = self.browser->GetSceneState();
  [sceneState removeObserver:self];
}

- (NSNumber*)highlightDestination {
  if (_activeIPHSessionType == PopupMenuIPHSessionType::kHistoryMenuItem) {
    return [NSNumber numberWithInt:static_cast<NSInteger>(
                                       overflow_menu::Destination::History)];
  }
  if (_activeIPHSessionType ==
      PopupMenuIPHSessionType::kLevelUpPasswordCheckupWalkthrough) {
    return [NSNumber numberWithInt:static_cast<NSInteger>(
                                       overflow_menu::Destination::Passwords)];
  }
  if (_activeIPHSessionType ==
      PopupMenuIPHSessionType::kLevelUpPaymentMethodsWalkthrough) {
    return [NSNumber numberWithInt:static_cast<NSInteger>(
                                       overflow_menu::Destination::Settings)];
  }
  return nil;
}

- (void)showIPHAfterOpenOfOverflowMenu:(UIViewController*)menu {
  // If Tab Reminders IPH is active, highlight the Tab Reminder action in the
  // overflow menu, then reset the in-session flag
  // (`inSessionWithTabRemindersIPH`) so that the action isn’t highlighted on
  // future openings unless triggered again by the Tab Reminders bubble IPH.
  if (_activeIPHSessionType == PopupMenuIPHSessionType::kTabReminders) {
    OverflowMenuAction* setTabReminderAction = [self.actionProvider
        actionForActionType:overflow_menu::ActionType::SetTabReminder];
    setTabReminderAction.highlighted = YES;
    _activeIPHSessionType = PopupMenuIPHSessionType::kNone;
    return;
  }

  if (_activeIPHSessionType ==
      PopupMenuIPHSessionType::kLevelUpQuickDeleteWalkthrough) {
    OverflowMenuAction* clearBrowsingDataAction = [self.actionProvider
        actionForActionType:overflow_menu::ActionType::ClearBrowsingData];
    self.uiConfiguration.scrollToAction = clearBrowsingDataAction;
  }

  if ([self
          showIPHInViewController:menu
                   forSessionType:PopupMenuIPHSessionType::kHistoryMenuItem]) {
    return;
  }

  if ([self showIPHInViewController:menu
                     forSessionType:PopupMenuIPHSessionType::
                                        kLevelUpPasswordCheckupWalkthrough]) {
    return;
  }

  if ([self showIPHInViewController:menu
                     forSessionType:PopupMenuIPHSessionType::
                                        kLevelUpPaymentMethodsWalkthrough]) {
    return;
  }

  if ([self showIPHInViewController:menu
                     forSessionType:PopupMenuIPHSessionType::
                                        kLevelUpQuickDeleteWalkthrough]) {
    return;
  }

  // Only try to show customization IPH if history IPH was not shown.
  [self showCustomizationIPHInMenu:menu];
}

- (BOOL)hasBlueDotForOverflowMenu {
  return self.hasBlueDot;
}

#pragma mark - Private

/// Shows the IPH bubble in the overflow menu.
- (BOOL)showIPHInViewController:(UIViewController*)menu
                 forSessionType:(PopupMenuIPHSessionType)sessionType {
  if (_activeIPHSessionType != sessionType) {
    return NO;
  }

  CGRect destFrame = self.uiConfiguration.highlightedDestinationFrame;
  CGRect listFrame = self.uiConfiguration.destinationListScreenFrame;
  CGFloat parentViewWidth = CGRectGetWidth(menu.view.bounds);

  CGFloat anchorXInParent = CGRectGetWidth(destFrame) > 0
                                ? CGRectGetMidX(destFrame)
                                : 0.5 * parentViewWidth;

  CGPoint anchorInMenu =
      CGPointMake(anchorXInParent, CGRectGetMidY(menu.view.bounds));
  if (sessionType != PopupMenuIPHSessionType::kLevelUpQuickDeleteWalkthrough &&
      CGRectGetHeight(listFrame) > 0) {
    CGPoint listBottomInMenu =
        [menu.view convertPoint:CGPointMake(0, CGRectGetMaxY(listFrame))
                       fromView:nil];
    anchorInMenu.y = listBottomInMenu.y;
  }

  CGPoint anchorPoint = [menu.view.window convertPoint:anchorInMenu
                                              fromView:menu.view];

  self.overflowMenuBubblePresenter =
      [self createBubblePresenterForIPH:sessionType
                        anchorXInParent:anchorXInParent
                        parentViewWidth:parentViewWidth];

  if (![self.overflowMenuBubblePresenter canPresentInView:menu.view
                                              anchorPoint:anchorPoint]) {
    // Reset the highlight status of the destination as we will miss the other
    // path of resetting it when dismissing the IPH.
    self.uiConfiguration.highlightDestination = -1;
    // No effect besides leaving it in a clean state.
    self.uiConfiguration.highlightedDestinationFrame = CGRectZero;
    self.overflowMenuBubblePresenter = nil;
    return NO;
  }

  [self.overflowMenuBubblePresenter presentInViewController:menu
                                                anchorPoint:anchorPoint];
  _activeIPHSessionType = PopupMenuIPHSessionType::kNone;
  return YES;
}

// Creates the BubbleViewControllerPresenter for the given session type.
- (BubbleViewControllerPresenter*)
    createBubblePresenterForIPH:(PopupMenuIPHSessionType)iphType
                anchorXInParent:(CGFloat)anchorXInParent
                parentViewWidth:(CGFloat)parentViewWidth {
  switch (iphType) {
    case PopupMenuIPHSessionType::kHistoryMenuItem:
      return [self
          newHistoryIPHBubblePresenterWithAnchorXInParent:anchorXInParent
                                          parentViewWidth:parentViewWidth];
    case PopupMenuIPHSessionType::kLevelUpPasswordCheckupWalkthrough:
      return [self
          newLevelUpPasswordCheckupWalkthroughBubblePresenterWithAnchorXInParent:
              anchorXInParent
                                                                 parentViewWidth:
                                                                     parentViewWidth];
    case PopupMenuIPHSessionType::kLevelUpPaymentMethodsWalkthrough:
      return [self
          newLevelUpPaymentMethodsWalkthroughBubblePresenterWithAnchorXInParent:
              anchorXInParent
                                                                parentViewWidth:
                                                                    parentViewWidth];
    case PopupMenuIPHSessionType::kLevelUpQuickDeleteWalkthrough:
      return [self
          newLevelUpQuickDeleteWalkthroughBubblePresenterWithAnchorXInParent:
              anchorXInParent
                                                             parentViewWidth:
                                                                 parentViewWidth];
    default:
      NOTREACHED();
  }
}

// Possibly shows the IPH for the Overflow Menu Customization feature. Returns
// whether or not the IPH was shown.
- (BOOL)showCustomizationIPHInMenu:(UIViewController*)menu {
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
  CHECK(!_stopped, base::NotFatalUntil::M147)
      << "PopupMenuHelpCoordinator used after -stop";

  // As sync error takes precendence on blue dot for settings destination in the
  // overflow menu. In that case don't show blue dot as the full path from
  // toolbar to default browser settings cannot be highlighted.
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.profile);
  if (syncService && GetAccountErrorUIInfo(syncService) != nil) {
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
  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType reason) {
        [weakSelf popupMenuIPHDidDismissWithReasonType:reason];
      };

  NSString* text =
      l10n_util::GetNSString(IDS_IOS_VIEW_BROWSING_HISTORY_OVERFLOW_MENU_TIP);

  std::u16string menuButtonA11yLabel = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS));

  NSString* voiceOverAnnouncement = l10n_util::GetNSStringF(
      IDS_IOS_VIEW_BROWSING_HISTORY_FROM_MENU_ANNOUNCEMENT,
      menuButtonA11yLabel);

  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [self createPopupMenuBubblePresenterWithText:text
                             voiceOverAnnouncement:voiceOverAnnouncement
                                 dismissalCallback:dismissalCallback];

  return bubbleViewControllerPresenter;
}

- (void)popupMenuIPHDidDismissWithReasonType:(IPHDismissalReasonType)reason {
  if (reason == IPHDismissalReasonType::kTappedAnchorView ||
      reason == IPHDismissalReasonType::kTimedOut) {
    _activeIPHSessionType = PopupMenuIPHSessionType::kHistoryMenuItem;
  }

  feature_engagement::Tracker* tracker = self.featureEngagementTracker;

  if (tracker) {
    const base::Feature& feature =
        feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature;
    tracker->Dismissed(feature);
  }

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
  if (!IsUserNewSafariSwitcher(_deviceSwitcherResultDispatcher)) {
    return;
  }

  BubbleViewControllerPresenter* bubblePresenter =
      [self newPopupMenuBubblePresenter];

  [self displayPopupMenuIPHBubble:bubblePresenter
                       forFeature:feature_engagement::
                                      kIPHiOSHistoryOnOverflowMenuFeature];
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

  [self.UIUpdater setOverflowMenuBlueDot:self.hasBlueDot];
}

- (void)notifyIPHBubblePresenting {
  // Remove blue dot if IPH bubble will be presenting on tools menu button.
  self.hasBlueDot = NO;
  [self.UIUpdater setOverflowMenuBlueDot:self.hasBlueDot];
}

- (void)displayPopupMenuTabRemindersIPH {
  CHECK(send_tab_to_self::AreIOSTabRemindersEnabled());

  BubbleViewControllerPresenter* bubblePresenter =
      [self newReminderNotificationsOverflowMenuBubblePresenter];

  [self
      displayPopupMenuIPHBubble:bubblePresenter
                     forFeature:
                         feature_engagement::
                             kIPHiOSReminderNotificationsOverflowMenuBubbleFeature];
}

// Creates and returns a `BubbleViewControllerPresenter` for the reminder
// notifications IPH in the overflow menu.
- (BubbleViewControllerPresenter*)
    newReminderNotificationsOverflowMenuBubblePresenter {
  CHECK(send_tab_to_self::AreIOSTabRemindersEnabled());

  NSString* text = l10n_util::GetNSString(
      IDS_IOS_REMINDER_NOTIFICATIONS_TOOLS_MENU_BUBBLE_IPH);

  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback = ^(
      IPHDismissalReasonType reason) {
    [weakSelf
        reminderNotificationsOverflowMenuIPHDidDismissWithReasonType:reason];
  };

  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [self createPopupMenuBubblePresenterWithText:text
                             voiceOverAnnouncement:nil
                                 dismissalCallback:dismissalCallback];

  bubbleViewControllerPresenter.customBubbleVisibilityDuration =
      kDefaultLongDurationBubbleVisibility;

  return bubbleViewControllerPresenter;
}

// Handles the dismissal of the reminder notifications IPH in the overflow menu.
// `IPHDismissalReasonType`: The reason why the IPH was dismissed.
- (void)reminderNotificationsOverflowMenuIPHDidDismissWithReasonType:
    (IPHDismissalReasonType)reason {
  CHECK(send_tab_to_self::AreIOSTabRemindersEnabled());

  if (reason == IPHDismissalReasonType::kTappedAnchorView ||
      reason == IPHDismissalReasonType::kTappedIPH) {
    // If the user interacted with the IPH by tapping on it or its anchor view,
    // consider this as a successful interaction and set
    // `inSessionWithTabRemindersIPH` to YES to enable highlighting the
    // corresponding action in the overflow menu.
    _activeIPHSessionType = PopupMenuIPHSessionType::kTabReminders;
  }

  feature_engagement::Tracker* tracker = self.featureEngagementTracker;

  if (tracker) {
    const base::Feature& feature = feature_engagement::
        kIPHiOSReminderNotificationsOverflowMenuBubbleFeature;
    tracker->Dismissed(feature);
  }

  [self.UIUpdater updateUIForIPHDismissed];

  self.popupMenuBubblePresenter = nil;
}

// Triggers Step 1 of a Level Up walkthrough IPH sequence (a bubble on the
// toolbar pointing to the Tools menu button).
- (void)showLevelUpWalkthroughStep1WithSessionType:
            (PopupMenuIPHSessionType)sessionType
                                              text:(NSString*)text
                                        totalPages:(NSInteger)totalPages {
  if (!IsLevelUpEnabled()) {
    return;
  }

  BOOL hasActiveIPHSession =
      _activeIPHSessionType != PopupMenuIPHSessionType::kNone;

  if (self.popupMenuBubblePresenter || hasActiveIPHSession) {
    return;
  }

  _activeIPHSessionType = sessionType;

  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType reason) {
        [weakSelf levelUpWalkthroughStep1DismissedWithReason:reason
                                                 sessionType:sessionType];
        weakSelf.popupMenuBubblePresenter = nil;
      };

  BOOL isAtBottom = [self isToolsMenuAtBottom];
  BubbleArrowDirection arrowDirection =
      isAtBottom ? BubbleArrowDirectionDown : BubbleArrowDirectionUp;

  UIViewController* baseViewController = self.baseViewController;
  UIView* baseView = baseViewController.view;
  CGRect anchorFrame = self.layoutGuide.layoutFrame;

  BubbleAlignment alignment =
      CGRectGetMidX(anchorFrame) < CGRectGetWidth(baseView.bounds) / 2.0
          ? BubbleAlignmentTopOrLeading
          : BubbleAlignmentBottomOrTrailing;

  BubbleViewControllerPresenter* bubblePresenter =
      [[BubbleViewControllerPresenter alloc]
                   initWithText:text
                          title:nil
                 arrowDirection:arrowDirection
                      alignment:alignment
                     bubbleType:BubbleViewTypeRichWithNext
                pageControlPage:BubblePageControlPageFirst
          totalPageControlPages:totalPages
          customNextButtonTitle:nil
              dismissalCallback:dismissalCallback];
  bubblePresenter.dismissalTimerDisabled = YES;

  CGFloat anchorPointY =
      isAtBottom ? CGRectGetMinY(anchorFrame) : CGRectGetMaxY(anchorFrame);

  CGPoint anchorPointInOwningView =
      CGPointMake(CGRectGetMidX(anchorFrame), anchorPointY);
  CGPoint anchorPoint = [baseView convertPoint:anchorPointInOwningView
                                      fromView:self.layoutGuide.owningView];

  CGFloat alignmentOffset = bubble_util::BubbleDefaultAlignmentOffset();
  CGFloat maxAnchorX = CGRectGetWidth(baseView.bounds) - alignmentOffset - 1.0;
  if (anchorPoint.x > maxAnchorX) {
    anchorPoint.x = maxAnchorX;
  }

  if (![bubblePresenter canPresentInView:baseView anchorPoint:anchorPoint]) {
    _activeIPHSessionType = PopupMenuIPHSessionType::kNone;
    return;
  }

  self.popupMenuBubblePresenter = bubblePresenter;
  [self.popupMenuBubblePresenter presentInViewController:baseViewController
                                             anchorPoint:anchorPoint
                                         anchorViewFrame:anchorFrame];
  [self notifyIPHBubblePresenting];
}

// Action triggered when the Level Up walkthrough Step 1 IPH (pointing to the
// Tools menu button) is dismissed. Prepares the session state for Step 2.
- (void)
    levelUpWalkthroughStep1DismissedWithReason:(IPHDismissalReasonType)reason
                                   sessionType:
                                       (PopupMenuIPHSessionType)sessionType {
  if (reason == IPHDismissalReasonType::kTappedAnchorView ||
      reason == IPHDismissalReasonType::kTappedIPH ||
      reason == IPHDismissalReasonType::kTappedNext) {
    _activeIPHSessionType = sessionType;
    if (reason == IPHDismissalReasonType::kTappedNext) {
      id<PopupMenuCommands> popupMenuHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), PopupMenuCommands);
      [popupMenuHandler showToolsMenuPopup];
    }
  } else {
    _activeIPHSessionType = PopupMenuIPHSessionType::kNone;
  }
}

// Triggers Step 1 of the Level Up Password Checkup walkthrough IPH sequence.
- (void)showLevelUpPasswordCheckupWalkthroughIPH {
  [self
      showLevelUpWalkthroughStep1WithSessionType:
          PopupMenuIPHSessionType::kLevelUpPasswordCheckupWalkthrough
                                            text:
                                                l10n_util::GetNSString(
                                                    IDS_IOS_LEVEL_UP_WALKTHROUGH_OPEN_CHROME_MENU)
                                      totalPages:
                                          kLevelUpPasswordCheckupWalkthroughTotalPages];
}

// Action triggered when the Level Up Password Checkup Step 2 IPH (pointing to
// the Password Manager row in the overflow menu) is dismissed. Navigates the
// user to the Password Manager settings page to show Step 3.
- (void)levelUpPasswordCheckupIPHDismissedWithReason:
    (IPHDismissalReasonType)reason {
  _activeIPHSessionType = PopupMenuIPHSessionType::kNone;
  if (reason == IPHDismissalReasonType::kTappedNext ||
      reason == IPHDismissalReasonType::kTappedAnchorView ||
      reason == IPHDismissalReasonType::kTappedIPH) {
    id<SettingsCommands> settingsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), SettingsCommands);
    [settingsHandler
        showSavedPasswordsSettingsFromViewController:self.baseViewController
                     shouldShowLevelUpWalkthroughIPH:YES];
  }
}

// Triggers Step 1 of the Level Up Payment Methods walkthrough IPH sequence.
- (void)showLevelUpPaymentMethodsWalkthroughIPH {
  [self
      showLevelUpWalkthroughStep1WithSessionType:
          PopupMenuIPHSessionType::kLevelUpPaymentMethodsWalkthrough
                                            text:
                                                l10n_util::GetNSString(
                                                    IDS_IOS_LEVEL_UP_WALKTHROUGH_OPEN_CHROME_MENU)
                                      totalPages:
                                          kLevelUpPaymentMethodsWalkthroughTotalPages];
}

// Action triggered when the Level Up Payment Methods Step 2 IPH (pointing to
// the Settings / Payment Methods item in the overflow menu) is dismissed.
- (void)levelUpPaymentMethodsIPHDismissedWithReason:
    (IPHDismissalReasonType)reason {
  _activeIPHSessionType = PopupMenuIPHSessionType::kNone;
  switch (reason) {
    case IPHDismissalReasonType::kTappedNext:
    case IPHDismissalReasonType::kTappedAnchorView:
    case IPHDismissalReasonType::kTappedIPH: {
      id<PopupMenuCommands> popupMenuHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), PopupMenuCommands);
      [popupMenuHandler dismissPopupMenuAnimated:YES];

      id<SceneCommands> sceneHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), SceneCommands);
      [sceneHandler showSettingsFromViewController:self.baseViewController
                   shouldShowLevelUpWalkthroughIPH:YES];
      break;
    }
    default:
      break;
  }
}

// Triggers Step 1 of the Level Up Quick Delete walkthrough IPH sequence.
- (void)showLevelUpQuickDeleteWalkthroughIPH {
  [self
      showLevelUpWalkthroughStep1WithSessionType:
          PopupMenuIPHSessionType::kLevelUpQuickDeleteWalkthrough
                                            text:
                                                l10n_util::GetNSString(
                                                    IDS_IOS_LEVEL_UP_WALKTHROUGH_OPEN_CHROME_MENU)
                                      totalPages:
                                          kLevelUpQuickDeleteWalkthroughTotalPages];
}

// Action triggered when the Level Up Quick Delete Step 2 IPH (pointing to
// the Clear Browsing Data row in the overflow menu) is dismissed.
- (void)levelUpQuickDeleteIPHDismissedWithReason:
    (IPHDismissalReasonType)reason {
  _activeIPHSessionType = PopupMenuIPHSessionType::kNone;
  if (reason == IPHDismissalReasonType::kTappedNext ||
      reason == IPHDismissalReasonType::kTappedAnchorView ||
      reason == IPHDismissalReasonType::kTappedIPH) {
    id<QuickDeleteCommands> quickDeleteHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), QuickDeleteCommands);
    [quickDeleteHandler showQuickDeleteAndCanPerformRadialWipeAnimation:YES];
  }
}

#pragma mark - Overflow Menu Bubble methods

// Generic factory helper that creates and configures a bubble presenter shown
// in the overflow menu using the provided parameters.
- (BubbleViewControllerPresenter*)
    newOverflowMenuBubblePresenterWithAnchorXInParent:(CGFloat)anchorXInParent
                                      parentViewWidth:(CGFloat)parentViewWidth
                                                 text:(NSString*)text
                                           bubbleType:(BubbleViewType)bubbleType
                                      pageControlPage:
                                          (BubblePageControlPage)pageControlPage
                                    dismissalCallback:
                                        (CallbackWithIPHDismissalReasonType)
                                            dismissalCallback {
  BubbleAlignment alignment = anchorXInParent < 0.5 * parentViewWidth
                                  ? BubbleAlignmentTopOrLeading
                                  : BubbleAlignmentBottomOrTrailing;
  BubbleArrowDirection arrowDirection = BubbleArrowDirectionUp;
  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc] initWithText:text
                                                    title:nil
                                           arrowDirection:arrowDirection
                                                alignment:alignment
                                               bubbleType:bubbleType
                                          pageControlPage:pageControlPage
                                        dismissalCallback:dismissalCallback];
  return bubbleViewControllerPresenter;
}

// Creates and returns a `BubbleViewControllerPresenter` for the History IPH in
// the overflow menu.
- (BubbleViewControllerPresenter*)
    newHistoryIPHBubblePresenterWithAnchorXInParent:(CGFloat)anchorXInParent
                                    parentViewWidth:(CGFloat)parentViewWidth {
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_VIEW_BROWSING_HISTORY_OVERFLOW_MENU_TIP);

  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType reason) {
        [weakSelf overflowMenuIPHDidDismiss];
      };

  BubbleViewControllerPresenter* bubbleViewControllerPresenter = [self
      newOverflowMenuBubblePresenterWithAnchorXInParent:anchorXInParent
                                        parentViewWidth:parentViewWidth
                                                   text:text
                                             bubbleType:BubbleViewTypeDefault
                                        pageControlPage:
                                            BubblePageControlPageNone
                                      dismissalCallback:dismissalCallback];
  std::u16string historyButtonA11yLabel = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_HISTORY));
  bubbleViewControllerPresenter.voiceOverAnnouncement = l10n_util::GetNSStringF(
      IDS_IOS_VIEW_BROWSING_HISTORY_BY_SELECTING_HISTORY_TIP_ANNOUNCEMENT,
      historyButtonA11yLabel);
  return bubbleViewControllerPresenter;
}

// Creates and returns a `BubbleViewControllerPresenter` for Step 2 of the Level
// Up Password Checkup walkthrough sequence (a bubble pointing to the Password
// Manager item inside the overflow menu).
- (BubbleViewControllerPresenter*)
    newLevelUpPasswordCheckupWalkthroughBubblePresenterWithAnchorXInParent:
        (CGFloat)anchorXInParent
                                                           parentViewWidth:
                                                               (CGFloat)
                                                                   parentViewWidth {
  NSString* text = l10n_util::GetNSString(
      IDS_IOS_LEVEL_UP_WALKTHROUGH_OPEN_PASSWORD_MANAGER);

  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType reason) {
        [weakSelf levelUpPasswordCheckupIPHDismissedWithReason:reason];
        weakSelf.overflowMenuBubblePresenter = nil;
      };

  BubbleViewControllerPresenter* bubbleViewControllerPresenter = [self
      newOverflowMenuBubblePresenterWithAnchorXInParent:anchorXInParent
                                        parentViewWidth:parentViewWidth
                                                   text:text
                                             bubbleType:
                                                 BubbleViewTypeRichWithNext
                                        pageControlPage:
                                            BubblePageControlPageSecond
                                      dismissalCallback:dismissalCallback];
  bubbleViewControllerPresenter.dismissalTimerDisabled = YES;
  return bubbleViewControllerPresenter;
}

// Creates and returns a `BubbleViewControllerPresenter` for Step 2 of the Level
// Up Payment Methods walkthrough sequence (a bubble pointing to the Payment
// Methods item inside the overflow menu).
- (BubbleViewControllerPresenter*)
    newLevelUpPaymentMethodsWalkthroughBubblePresenterWithAnchorXInParent:
        (CGFloat)anchorXInParent
                                                          parentViewWidth:
                                                              (CGFloat)
                                                                  parentViewWidth {
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_WALKTHROUGH_OPEN_SETTINGS);

  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType reason) {
        [weakSelf levelUpPaymentMethodsIPHDismissedWithReason:reason];
        weakSelf.overflowMenuBubblePresenter = nil;
      };

  BubbleAlignment alignment = anchorXInParent < 0.5 * parentViewWidth
                                  ? BubbleAlignmentTopOrLeading
                                  : BubbleAlignmentBottomOrTrailing;

  NSString* customNextButtonTitle =
      l10n_util::GetNSString(IDS_IOS_IPH_BUBBLE_NEXT);

  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
                   initWithText:text
                          title:nil
                 arrowDirection:BubbleArrowDirectionUp
                      alignment:alignment
                     bubbleType:BubbleViewTypeRichWithNext
                pageControlPage:BubblePageControlPageSecond
          totalPageControlPages:kLevelUpPaymentMethodsWalkthroughTotalPages
          customNextButtonTitle:customNextButtonTitle
              dismissalCallback:dismissalCallback];
  bubbleViewControllerPresenter.dismissalTimerDisabled = YES;
  return bubbleViewControllerPresenter;
}

// Creates and returns a `BubbleViewControllerPresenter` for Step 2 of the Level
// Up Quick Delete walkthrough sequence (a bubble pointing to the Clear Browsing
// Data item inside the overflow menu).
- (BubbleViewControllerPresenter*)
    newLevelUpQuickDeleteWalkthroughBubblePresenterWithAnchorXInParent:
        (CGFloat)anchorXInParent
                                                       parentViewWidth:
                                                           (CGFloat)
                                                               parentViewWidth {
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_WALKTHROUGH_DELETE_BROWSING_DATA);

  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType reason) {
        [weakSelf levelUpQuickDeleteIPHDismissedWithReason:reason];
        weakSelf.overflowMenuBubblePresenter = nil;
      };

  BubbleAlignment alignment = anchorXInParent < 0.5 * parentViewWidth
                                  ? BubbleAlignmentTopOrLeading
                                  : BubbleAlignmentBottomOrTrailing;

  NSString* customNextButtonTitle =
      l10n_util::GetNSString(IDS_IOS_IPH_BUBBLE_NEXT);

  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
                   initWithText:text
                          title:nil
                 arrowDirection:BubbleArrowDirectionDown
                      alignment:alignment
                     bubbleType:BubbleViewTypeRichWithNext
                pageControlPage:BubblePageControlPageSecond
          totalPageControlPages:kLevelUpQuickDeleteWalkthroughTotalPages
          customNextButtonTitle:customNextButtonTitle
              dismissalCallback:dismissalCallback];
  bubbleViewControllerPresenter.dismissalTimerDisabled = YES;
  return bubbleViewControllerPresenter;
}

- (void)overflowMenuIPHDidDismiss {
  self.overflowMenuBubblePresenter = nil;
  self.uiConfiguration.highlightDestination = -1;
  OverflowMenuAction* clearBrowsingDataAction = [self.actionProvider
      actionForActionType:overflow_menu::ActionType::ClearBrowsingData];
  clearBrowsingDataAction.highlighted = NO;
  clearBrowsingDataAction.automaticallyUnhighlight = YES;
}

#pragma mark - Overflow Menu Customization Methods

- (BubbleViewControllerPresenter*)newOverflowMenuCustomizationBubblePresenter {
  NSString* text = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_CUSTOMIZATION_IPH);

  // Prepare the dismissal callback.
  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType reason) {
        [weakSelf overflowMenuCustomizationIPHDidDismissWithReasonType:reason];
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

- (void)overflowMenuCustomizationIPHDidDismissWithReasonType:
    (IPHDismissalReasonType)reason {
  if (reason == IPHDismissalReasonType::kTappedIPH) {
    [self scrollToEditActionsButton];
  }
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
    _activeIPHSessionType = PopupMenuIPHSessionType::kNone;
  } else if (level >= SceneActivationLevelForegroundActive) {
    [self prepareToShowPopupMenuIPHs];
  }
}

#pragma mark - Feature Engagement Tracker queries

// Queries the feature engagement tracker to see if the Overflow Menu
// Customization IPH can be displayed. If this returns YES, the IPH MUST be
// shown and dismissed.
- (BOOL)canShowOverflowMenuCustomizationIPH {
  if (IsOverflowMenuNTPRefactorEnabled()) {
    // Edit button is not available on the NTP.
    if (IsVisibleURLNewTabPage(
            self.browser->GetWebStateList()->GetActiveWebState())) {
      return NO;
    }
  }
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  const base::Feature& feature =
      feature_engagement::kIPHiOSOverflowMenuCustomizationFeature;
  return tracker && tracker->ShouldTriggerHelpUI(feature);
}

#pragma mark - Bubble Presenter Helpers

// Helper function to create a `BubbleViewControllerPresenter` specifically
// for the popup menu button (also known as the tools menu button).
// `text`: The text to display in the bubble.
// `voiceOverAnnouncement`: The announcement for VoiceOver to read. Can be
// `nil`.
// `dismissalCallback`: A callback invoked when the bubble is dismissed.
- (BubbleViewControllerPresenter*)
    createPopupMenuBubblePresenterWithText:(NSString*)text
                     voiceOverAnnouncement:(NSString*)voiceOverAnnouncement
                         dismissalCallback:(CallbackWithIPHDismissalReasonType)
                                               dismissalCallback {
  BubbleArrowDirection arrowDirection = [self isToolsMenuAtBottom]
                                            ? BubbleArrowDirectionDown
                                            : BubbleArrowDirectionUp;

  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
          initDefaultBubbleWithText:text
                     arrowDirection:arrowDirection
                          alignment:BubbleAlignmentBottomOrTrailing
                  dismissalCallback:dismissalCallback];

  bubbleViewControllerPresenter.voiceOverAnnouncement = voiceOverAnnouncement;

  return bubbleViewControllerPresenter;
}

// Returns whether the tools menu button is displayed at the bottom of the
// screen.
- (BOOL)isToolsMenuAtBottom {
  if (IsChromeNextIaEnabled()) {
    return IsCurrentLayoutBottomOmnibox(self.browser);
  }
  return IsSplitToolbarMode(self.baseViewController);
}

// Displays an IPH bubble anchored to the popup menu button (tools menu button).
// `bubblePresenter`: The presenter configured for the bubble.
// `feature`: The feature engagement feature associated with this IPH.
- (void)displayPopupMenuIPHBubble:
            (BubbleViewControllerPresenter*)bubblePresenter
                       forFeature:(const base::Feature&)feature {
  BOOL hasActiveIPHSession =
      _activeIPHSessionType != PopupMenuIPHSessionType::kNone;

  // Skip if a bubble presentation or active IPH session is already in progress.
  if (self.popupMenuBubblePresenter || hasActiveIPHSession) {
    return;
  }

  UIViewController* baseViewController = self.baseViewController;
  UIView* baseView = baseViewController.view;
  CGRect anchorFrame = self.layoutGuide.layoutFrame;

  BOOL isAtBottom = [self isToolsMenuAtBottom];
  CGFloat anchorPointY =
      isAtBottom ? CGRectGetMinY(anchorFrame) : CGRectGetMaxY(anchorFrame);

  CGPoint anchorPointInOwningView =
      CGPointMake(CGRectGetMidX(anchorFrame), anchorPointY);
  CGPoint anchorPoint = [baseView convertPoint:anchorPointInOwningView
                                      fromView:self.layoutGuide.owningView];

  // Cap `anchorPoint.x` to prevent the bubble from overflowing the screen.
  // The `-1.0` accounts for subpixel rounding in
  // `bubble_util::LeadingDistance`.
  CGFloat alignmentOffset = bubble_util::BubbleDefaultAlignmentOffset();
  CGFloat maxAnchorX = CGRectGetWidth(baseView.bounds) - alignmentOffset - 1.0;
  if (anchorPoint.x > maxAnchorX) {
    anchorPoint.x = maxAnchorX;
  }

  // Discard if it doesn't fit in the view as it is currently shown.
  if (![bubblePresenter canPresentInView:baseView anchorPoint:anchorPoint]) {
    return;
  }

  // Early return if the Feature Engagement Tracker won't display the IPH.
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  if (!tracker || !tracker->ShouldTriggerHelpUI(feature)) {
    return;
  }

  self.popupMenuBubblePresenter = bubblePresenter;
  [self.popupMenuBubblePresenter presentInViewController:baseViewController
                                             anchorPoint:anchorPoint
                                         anchorViewFrame:anchorFrame];
  [self.UIUpdater updateUIForOverflowMenuIPHDisplayed];
}

@end
