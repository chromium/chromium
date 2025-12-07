// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_coordinator.h"

#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/collaboration_flow_entry_point.h"
#import "components/collaboration/public/collaboration_flow_type.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/prefs/pref_service.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_factory.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/saved_tab_groups/coordinator/face_pile_configuration.h"
#import "ios/chrome/browser/saved_tab_groups/coordinator/face_pile_coordinator.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_action_context.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/disabled_grid_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_mediator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_mediator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_confirmation_coordinator.h"

using collaboration::CollaborationServiceFactory;
using collaboration::FlowType;
using collaboration::IOSCollaborationControllerDelegate;
using collaboration::messaging::MessagingBackendServiceFactory;
using ResultCallback =
    collaboration::CollaborationControllerDelegate::ResultCallback;
using collaboration::CollaborationControllerDelegate;

namespace {

// The preferred size in points for the avatar icons.
constexpr CGFloat kFacePileAvatarSize = 24;

}  // namespace

@interface TabGroupsPanelCoordinator () <TabGroupsPanelMediatorDelegate>

// Callback invoked upon confirming leaving or deleting a shared group.
@property(nonatomic, copy) void (^leaveOrDeleteCompletion)
    (CollaborationControllerDelegate::Outcome);

@end

@implementation TabGroupsPanelCoordinator {
  // Mutator that handles toolbars changes.
  __weak id<GridToolbarsMutator> _toolbarsMutator;
  // Delegate that handles the screen when the Tab Groups panel is disabled.
  __weak id<DisabledGridViewControllerDelegate> _disabledViewControllerDelegate;
  // The coordinator to handle the confirmation dialog for the action taken for
  // a tab group.
  TabGroupConfirmationCoordinator* _tabGroupConfirmationCoordinator;
}

- (instancetype)
        initWithBaseViewController:(UIViewController*)baseViewController
                    regularBrowser:(Browser*)regularBrowser
                   toolbarsMutator:(id<GridToolbarsMutator>)toolbarsMutator
    disabledViewControllerDelegate:
        (id<DisabledGridViewControllerDelegate>)disabledViewControllerDelegate {
  CHECK(baseViewController);
  CHECK(regularBrowser);
  CHECK(!regularBrowser->GetProfile()->IsOffTheRecord());
  CHECK(toolbarsMutator);
  CHECK(disabledViewControllerDelegate);
  self = [super initWithBaseViewController:baseViewController
                                   browser:regularBrowser];
  if (self) {
    _toolbarsMutator = toolbarsMutator;
    _disabledViewControllerDelegate = disabledViewControllerDelegate;
  }
  return self;
}

- (void)start {
  [super start];

  ProfileIOS* profile = self.profile;
  _gridContainerViewController = [[GridContainerViewController alloc] init];

  BOOL regularModeDisabled = IsIncognitoModeForced(profile->GetPrefs());
  if (regularModeDisabled) {
    _disabledViewController =
        [[DisabledGridViewController alloc] initWithPage:TabGridPageTabGroups];
    _disabledViewController.delegate = _disabledViewControllerDelegate;
    _gridContainerViewController.containedViewController =
        _disabledViewController;
  } else {
    _gridViewController = [[TabGroupsPanelViewController alloc] init];
    _gridContainerViewController.containedViewController = _gridViewController;
  }

  tab_groups::TabGroupSyncService* tabGroupSyncService =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  ShareKitService* shareKitService =
      ShareKitServiceFactory::GetForProfile(profile);
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  collaboration::messaging::MessagingBackendService* messagingService =
      MessagingBackendServiceFactory::GetForProfile(profile);
  data_sharing::DataSharingService* dataSharingService =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);

  _mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:tabGroupSyncService
                  shareKitService:shareKitService
             collaborationService:collaborationService
                 messagingService:messagingService
               dataSharingService:dataSharingService
              regularWebStateList:self.browser->GetWebStateList()
                    faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                      profile)
                 disabledByPolicy:regularModeDisabled
                      browserList:BrowserListFactory::GetForProfile(profile)];

  _mediator.toolbarsMutator = _toolbarsMutator;
  _mediator.tabGridHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), TabGridCommands);
  _mediator.applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  _mediator.tabGroupsCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);
  _mediator.consumer = _gridViewController;
  _mediator.delegate = self;
  _gridViewController.mutator = _mediator;
  _gridViewController.itemDataSource = _mediator;
}

- (void)stop {
  [super stop];

  [self clearLeaveOrDeleteCompletion];
  if (_tabGroupConfirmationCoordinator) {
    [_tabGroupConfirmationCoordinator stop];
    _tabGroupConfirmationCoordinator = nil;
  }
  [_mediator disconnect];
  _mediator.toolbarsMutator = nil;
  _mediator = nil;
  _gridViewController = nil;
  _disabledViewController.delegate = nil;
  _disabledViewController = nil;
  _gridContainerViewController.containedViewController = nil;
  _gridContainerViewController = nil;
}

- (void)prepareForAppearance {
  [_gridViewController prepareForAppearance];
}

- (void)stopChildCoordinators {
  if (_tabGroupConfirmationCoordinator) {
    [_tabGroupConfirmationCoordinator stop];
    _tabGroupConfirmationCoordinator = nil;
  }
  [_gridViewController dismissModals];
}

#pragma mark - TabGroupsPanelMediatorDelegate

- (void)tabGroupsPanelMediator:(TabGroupsPanelMediator*)tabGroupsPanelMediator
           openGroupWithSyncID:(const base::Uuid&)syncID {
  tab_groups::TabGroupSyncService* tabGroupSyncService =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(self.profile);
  tabGroupSyncService->OpenTabGroup(
      syncID,
      std::make_unique<tab_groups::IOSTabGroupActionContext>(self.browser));
}

- (void)tabGroupsPanelMediator:(TabGroupsPanelMediator*)tabGroupsPanelMediator
    showDeleteGroupConfirmationWithSyncID:(const base::Uuid)syncID
                               sourceView:(UIView*)sourceView {
  _tabGroupConfirmationCoordinator = [[TabGroupConfirmationCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      actionType:TabGroupActionType::kDeleteTabGroup
                      sourceView:sourceView];
  __weak TabGroupsPanelCoordinator* weakSelf = self;
  _tabGroupConfirmationCoordinator.primaryAction = ^{
    [weakSelf deleteSyncedTabGroup:syncID];
  };

  [_tabGroupConfirmationCoordinator start];
}

- (void)tabGroupsPanelMediator:(TabGroupsPanelMediator*)tabGroupsPanelMediator
    startLeaveOrDeleteSharedGroupWithSyncID:(const base::Uuid)syncID
                                 groupTitle:(NSString*)groupTitle
                                  forAction:(TabGroupActionType)actionType
                                 sourceView:(UIView*)sourceView {
  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(ResultCallback)> completionCallback =
      base::BindOnce(^(ResultCallback resultCallback) {
        TabGroupsPanelCoordinator* strongSelf = weakSelf;
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

        switch (actionType) {
          case TabGroupActionType::kLeaveSharedTabGroup:
          case TabGroupActionType::kDeleteSharedTabGroup:
            [strongSelf
                showLeaveOrDeleteSharedGroupConfirmationWithActionType:
                    actionType
                                                            groupTitle:
                                                                groupTitle
                                                            sourceView:
                                                                sourceView];
            break;
          case TabGroupActionType::kDeleteOrKeepSharedTabGroup:
          case TabGroupActionType::kLeaveOrKeepSharedTabGroup:
          case TabGroupActionType::kUngroupTabGroup:
          case TabGroupActionType::kDeleteTabGroup:
          case TabGroupActionType::kCloseLastTabUnknownRole:
            NOTREACHED();
        }
      });

  Browser* browser = self.browser;
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(
          browser->GetProfile());
  if (!collaborationService) {
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
  collaborationService->StartLeaveOrDeleteFlow(std::move(delegate), syncID,
                                               entryPoint);
}

- (id<FacePileProviding>)facePileProviderForGroupID:
    (const std::string&)groupID {
  // Configure the face pile.
  FacePileConfiguration* config = [[FacePileConfiguration alloc] init];
  config.showsEmptyState = NO;
  config.avatarSize = kFacePileAvatarSize;
  config.groupID = data_sharing::GroupId(groupID);

  FacePileCoordinator* facePileCoordinator =
      [[FacePileCoordinator alloc] initWithFacePileConfiguration:config
                                                         browser:self.browser];
  [facePileCoordinator start];

  return facePileCoordinator;
}

#pragma mark - Private

// Displays a confirmation dialog anchoring to `sourceView` on iPad or at the
// bottom on iPhone to confirm that the shared group is going to be leaved or
// deleted.
- (void)
    showLeaveOrDeleteSharedGroupConfirmationWithActionType:
        (TabGroupActionType)actionType
                                                groupTitle:(NSString*)groupTitle
                                                sourceView:(UIView*)sourceView {
  _tabGroupConfirmationCoordinator = [[TabGroupConfirmationCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      actionType:actionType
                      sourceView:sourceView];
  _tabGroupConfirmationCoordinator.tabGroupName = groupTitle;
  __weak TabGroupsPanelCoordinator* weakSelf = self;
  _tabGroupConfirmationCoordinator.primaryAction = ^{
    [weakSelf runLeaveOrDeleteCompletion];
  };
  _tabGroupConfirmationCoordinator.dismissAction = ^{
    [weakSelf clearLeaveOrDeleteCompletion];
  };

  [_tabGroupConfirmationCoordinator start];
}

// Deletes a synced tab group and dismisses the confirmation coordinator.
- (void)deleteSyncedTabGroup:(const base::Uuid&)syncID {
  [_mediator deleteSyncedTabGroup:syncID];
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
