// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/coordinator/tab_group_indicator_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/collaboration_flow_entry_point.h"
#import "components/collaboration/public/collaboration_flow_type.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
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

using collaboration::CollaborationServiceShareOrManageEntryPoint;
using ResultCallback =
    collaboration::CollaborationControllerDelegate::ResultCallback;
using collaboration::CollaborationControllerDelegate;
using collaboration::FlowType;
using collaboration::IOSCollaborationControllerDelegate;

@interface TabGroupIndicatorCoordinator () <
    CreateOrEditTabGroupCoordinatorDelegate,
    TabGroupIndicatorMediatorDelegate>

// Callback invoked upon confirming leaving or deleting a shared group.
@property(nonatomic, copy) void (^leaveOrDeleteCompletion)
    (CollaborationControllerDelegate::Outcome);

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
  CHECK(IsTabGroupInGridEnabled());
  Browser* browser = self.browser;
  BOOL incognito = browser->GetProfile()->IsOffTheRecord();
  _view = [[TabGroupIndicatorView alloc] init];
  _view.displayedOnNTP = _displayedOnNTP;
  _view.incognito = incognito;
  _view.toolbarHeightDelegate = self.toolbarHeightDelegate;
  ProfileIOS* profile = browser->GetProfile();

  tab_groups::TabGroupSyncService* tabGroupSyncService =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  data_sharing::DataSharingService* dataSharingService =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);

  _mediator = [[TabGroupIndicatorMediator alloc]
      initWithTabGroupSyncService:tabGroupSyncService
                  shareKitService:ShareKitServiceFactory::GetForProfile(profile)
             collaborationService:collaborationService
               dataSharingService:dataSharingService
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
  [self clearLeaveOrDeleteCompletion];
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

- (void)
    showTabGroupIndicatorConfirmationForAction:(TabGroupActionType)actionType
                                         group:(base::WeakPtr<const TabGroup>)
                                                   tabGroup {
  if (!tabGroup) {
    return;
  }
  [self stopTabGroupConfirmationCoordinator];
  _tabGroupConfirmationCoordinator = [[TabGroupConfirmationCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      actionType:actionType
                      sourceView:_view];
  __weak TabGroupIndicatorMediator* weakMediator = _mediator;
  __weak __typeof(self) weakSelf = self;
  _tabGroupConfirmationCoordinator.primaryAction = ^{
    switch (actionType) {
      case TabGroupActionType::kUngroupTabGroup:
        [weakMediator unGroupWithConfirmation:NO];
        break;
      case TabGroupActionType::kDeleteTabGroup:
        [weakMediator deleteGroupWithConfirmation:NO];
        break;
      case TabGroupActionType::kLeaveSharedTabGroup:
      case TabGroupActionType::kDeleteSharedTabGroup:
        [weakSelf runLeaveOrDeleteCompletion];
        break;
      case TabGroupActionType::kLeaveOrKeepSharedTabGroup:
      case TabGroupActionType::kDeleteOrKeepSharedTabGroup:
        NOTREACHED();
    }
  };
  _tabGroupConfirmationCoordinator.dismissAction = ^{
    [weakSelf clearLeaveOrDeleteCompletion];
  };
  _tabGroupConfirmationCoordinator.tabGroupName = tabGroup->GetTitle();

  [_tabGroupConfirmationCoordinator start];
}

- (void)startLeaveOrDeleteSharedGroup:(base::WeakPtr<const TabGroup>)tabGroup
                            forAction:(TabGroupActionType)actionType {
  [self stopTabGroupConfirmationCoordinator];

  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(ResultCallback)> completionCallback =
      base::BindOnce(^(ResultCallback resultCallback) {
        TabGroupIndicatorCoordinator* strongSelf = weakSelf;
        if (!strongSelf) {
          std::move(resultCallback)
              .Run(CollaborationControllerDelegate::Outcome::kCancel);
          return;
        }
        auto completionBlock = base::CallbackToBlock(std::move(resultCallback));
        strongSelf.leaveOrDeleteCompletion =
            ^(CollaborationControllerDelegate::Outcome outcome) {
              completionBlock(outcome);
            };

        [strongSelf showTabGroupIndicatorConfirmationForAction:actionType
                                                         group:tabGroup];
      });

  Browser* browser = self.browser;
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(
          browser->GetProfile());

  const TabGroup* group = tabGroup.get();
  if (!group || !collaborationService) {
    return;
  }

  std::unique_ptr<IOSCollaborationControllerDelegate> delegate =
      std::make_unique<IOSCollaborationControllerDelegate>(
          browser,
          CreateControllerDelegateParamsFromProfile(
              self.profile, self.baseViewController, FlowType::kLeaveOrDelete));
  delegate->SetLeaveOrDeleteConfirmationCallback(std::move(completionCallback));

  collaboration::CollaborationServiceLeaveOrDeleteEntryPoint entryPoint =
      collaboration::CollaborationServiceLeaveOrDeleteEntryPoint::kUnknown;
  collaborationService->StartLeaveOrDeleteFlow(
      std::move(delegate), group->tab_group_id(), entryPoint);
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
    [tabGridHandler showPage:TabGridPageTabGroups animated:NO];
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
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
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
             arrowDirection:BubbleArrowDirectionUp
                  alignment:BubbleAlignmentCenter
                 bubbleType:BubbleViewTypeDefault
            pageControlPage:BubblePageControlPageNone
          dismissalCallback:^(IPHDismissalReasonType reason) {
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

- (void)shareOrManageTabGroup:(const TabGroup*)tabGroup
                   entryPoint:
                       (CollaborationServiceShareOrManageEntryPoint)entryPoint {
  Browser* browser = self.browser;
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(
          browser->GetProfile());

  if (!tabGroup || !collaborationService) {
    return;
  }

  std::unique_ptr<CollaborationControllerDelegate> delegate =
      std::make_unique<collaboration::IOSCollaborationControllerDelegate>(
          browser,
          CreateControllerDelegateParamsFromProfile(
              self.profile, self.baseViewController, FlowType::kShareOrManage));
  collaborationService->StartShareOrManageFlow(
      std::move(delegate), tabGroup->tab_group_id(), entryPoint);
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
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
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

// Clears `leaveOrDeleteCompletion`. If not nil, calls it with `kCancel`.
- (void)clearLeaveOrDeleteCompletion {
  if (self.leaveOrDeleteCompletion) {
    self.leaveOrDeleteCompletion(
        CollaborationControllerDelegate::Outcome::kCancel);
  }
  self.leaveOrDeleteCompletion = nil;
}

// Runs `leaveOrDeleteCompletion`. If not nil, calls it with `kSuccess`.
- (void)runLeaveOrDeleteCompletion {
  if (self.leaveOrDeleteCompletion) {
    self.leaveOrDeleteCompletion(
        CollaborationControllerDelegate::Outcome::kSuccess);
  }
  self.leaveOrDeleteCompletion = nil;
}

@end
