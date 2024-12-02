// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/coordinator/tab_group_indicator_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/create_or_edit_tab_group_coordinator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/create_tab_group_coordinator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_confirmation_coordinator.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/coordinator/tab_group_indicator_mediator.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/coordinator/tab_group_indicator_mediator_delegate.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/ui/tab_group_indicator_view.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface TabGroupIndicatorCoordinator () <
    CreateOrEditTabGroupCoordinatorDelegate,
    TabGroupIndicatorMediatorDelegate>
@end

@implementation TabGroupIndicatorCoordinator {
  TabGroupIndicatorMediator* _mediator;
  // Coordinator that shows the tab group coordinator edit view.
  CreateTabGroupCoordinator* _createTabGroupCoordinator;
  // Coordinator that handles confirmation dialog for the action taken for a tab
  // group.
  TabGroupConfirmationCoordinator* _tabGroupConfirmationCoordinator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  CHECK(IsTabGroupIndicatorEnabled());
  Browser* browser = self.browser;
  BOOL incognito = browser->GetProfile()->IsOffTheRecord();
  _view = [[TabGroupIndicatorView alloc] init];
  _view.displayedOnNTP = _displayedOnNTP;
  _view.incognito = incognito;
  _view.toolbarHeightDelegate = self.toolbarHeightDelegate;
  _view.facePileParentViewController = self.parentViewController;
  ProfileIOS* profile = browser->GetProfile();

  tab_groups::TabGroupSyncService* tabGroupSyncService =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);

  _mediator = [[TabGroupIndicatorMediator alloc]
      initWithTabGroupSyncService:tabGroupSyncService
                  shareKitService:ShareKitServiceFactory::GetForProfile(profile)
             collaborationService:collaborationService
                         consumer:_view
                     webStateList:browser->GetWebStateList()
                        URLLoader:UrlLoadingBrowserAgent::FromBrowser(browser)
                   featureTracker:feature_engagement::TrackerFactory::
                                      GetForProfile(profile)
                        incognito:incognito];
  _mediator.delegate = self;
  _mediator.baseViewController = self.baseViewController;
  _mediator.applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  _view.mutator = _mediator;
  [browser->GetSceneState() addObserver:_mediator];
}

- (void)stop {
  [self stopCreateTabGroupCoordinator];
  [self stopTabGroupConfirmationCoordinator];
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - TabGroupIndicatorMediatorDelegate

- (void)showTabGroupIndicatorEditionForGroup:
    (base::WeakPtr<const TabGroup>)tabGroup {
  if (!tabGroup) {
    return;
  }
  [self stopCreateTabGroupCoordinator];
  _createTabGroupCoordinator = [[CreateTabGroupCoordinator alloc]
      initTabGroupEditionWithBaseViewController:self.baseViewController
                                        browser:self.browser
                                       tabGroup:tabGroup.get()];
  _createTabGroupCoordinator.delegate = self;
  [_createTabGroupCoordinator start];
}

- (void)showRecentActivityForGroup:(base::WeakPtr<const TabGroup>)tabGroup {
  id<TabGroupsCommands> tabGroupsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);
  [tabGroupsHandler showRecentActivityForGroup:tabGroup];
}

- (void)showTabGroupIndicatorConfirmationForAction:
    (TabGroupActionType)actionType {
  [self stopTabGroupConfirmationCoordinator];
  _tabGroupConfirmationCoordinator = [[TabGroupConfirmationCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      actionType:actionType
                      sourceView:_view];
  __weak TabGroupIndicatorMediator* weakMediator = _mediator;
  _tabGroupConfirmationCoordinator.action = ^{
    switch (actionType) {
      case TabGroupActionType::kUngroupTabGroup:
        [weakMediator unGroupWithConfirmation:NO];
        break;
      case TabGroupActionType::kDeleteTabGroup:
        [weakMediator deleteGroupWithConfirmation:NO];
        break;
    }
  };

  [_tabGroupConfirmationCoordinator start];
}

- (void)showTabGroupIndicatorSnackbarAfterClosingGroup {
  // Create the "Open Tab Groups" action.
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  __weak id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  __weak id<TabGridCommands> tabGridHandler =
      HandlerForProtocol(dispatcher, TabGridCommands);
  void (^openTabGroupPanelAction)() = ^{
    [applicationHandler displayTabGridInMode:TabGridOpeningMode::kRegular];
    [tabGridHandler showTabGroupsPanelAnimated:NO];
  };

  // Create and config the snackbar.
  NSString* messageLabel = base::SysUTF16ToNSString(
      l10n_util::GetPluralStringFUTF16(IDS_IOS_TAB_GROUP_SNACKBAR_LABEL, 1));
  MDCSnackbarMessage* message = CreateSnackbarMessage(messageLabel);
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = openTabGroupPanelAction;
  action.title = l10n_util::GetNSString(IDS_IOS_TAB_GROUP_SNACKBAR_ACTION);
  message.action = action;

  id<SnackbarCommands> snackbarCommandsHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarCommandsHandler showSnackbarMessage:message];
}

- (void)showIPHForSharedTabGroupForegrounded {
  if (!_view.available) {
    return;
  }

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());
  if (!tracker->WouldTriggerHelpUI(
          feature_engagement::kIPHiOSSharedTabGroupForeground)) {
    return;
  }

  NSString* IPHTitle = l10n_util::GetNSString(
      IDS_IOS_SHARED_GROUP_USER_EDUCATION_IPH_FOREGROUND);
  __weak __typeof(self) weakSelf = self;
  BubbleViewControllerPresenter* presenter =
      [[BubbleViewControllerPresenter alloc]
               initWithText:IPHTitle
                      title:nil
                      image:nil
             arrowDirection:BubbleArrowDirectionUp
                  alignment:BubbleAlignmentCenter
                 bubbleType:BubbleViewTypeDefault
          dismissalCallback:^(
              IPHDismissalReasonType reason,
              feature_engagement::Tracker::SnoozeAction action) {
            [weakSelf sharedTabGroupForegroundIPHDismissed];
          }];
  presenter.voiceOverAnnouncement = IPHTitle;

  CGPoint anchorPoint =
      CGPointMake(CGRectGetMidX(_view.bounds), CGRectGetMaxY(_view.bounds));
  anchorPoint = [_view convertPoint:anchorPoint toView:nil];

  if (![presenter canPresentInView:self.baseViewController.view
                       anchorPoint:anchorPoint] ||
      !tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHiOSSharedTabGroupForeground)) {
    return;
  }

  [presenter presentInViewController:self.baseViewController
                         anchorPoint:anchorPoint];
}

#pragma mark - CreateOrEditTabGroupCoordinatorDelegate

- (void)createOrEditTabGroupCoordinatorDidDismiss:
            (CreateTabGroupCoordinator*)coordinator
                                         animated:(BOOL)animated {
  CHECK(coordinator == _createTabGroupCoordinator);
  _createTabGroupCoordinator.animatedDismissal = animated;
  _createTabGroupCoordinator.delegate = nil;
  [_createTabGroupCoordinator stop];
  _createTabGroupCoordinator = nil;
}

#pragma mark - Private

// Called when the IPH shown when the user foreground the app while having a
// shared tab group in foreground is dismissed.
- (void)sharedTabGroupForegroundIPHDismissed {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());
  tracker->Dismissed(feature_engagement::kIPHiOSSharedTabGroupForeground);
}

// Stops `_createTabGroupCoordinator`.
- (void)stopCreateTabGroupCoordinator {
  _createTabGroupCoordinator.delegate = nil;
  [_createTabGroupCoordinator stop];
  _createTabGroupCoordinator = nil;
}

// Stops `_tabGroupConfirmationCoordinator`.
- (void)stopTabGroupConfirmationCoordinator {
  [_tabGroupConfirmationCoordinator stop];
  _tabGroupConfirmationCoordinator = nil;
}

@end
