// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/tab_strip/coordinator/tab_strip_coordinator.h"

#import "base/check_op.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "base/uuid.h"
#import "components/collaboration/public/collaboration_flow_entry_point.h"
#import "components/collaboration/public/collaboration_flow_type.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/saved_tab_groups/coordinator/face_pile_configuration.h"
#import "ios/chrome/browser/saved_tab_groups/coordinator/face_pile_coordinator.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/shared_tab_group_last_tab_closed_alert_command.h"
#import "ios/chrome/browser/shared/public/commands/shared_tab_group_last_tab_closed_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_last_tab_dragged_alert_command.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/coordinator/tab_strip_mediator.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/coordinator/tab_strip_mediator_delegate.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/context_menu/tab_strip_context_menu_helper.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/swift.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/create_or_edit_tab_group_coordinator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/create_tab_group_coordinator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_confirmation_coordinator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using collaboration::CollaborationServiceShareOrManageEntryPoint;
using collaboration::FlowType;
using collaboration::IOSCollaborationControllerDelegate;
using ResultCallback =
    collaboration::CollaborationControllerDelegate::ResultCallback;
using collaboration::CollaborationControllerDelegate;

namespace {

// The preferred size in points for the avatar icons.
constexpr CGFloat kFacePileAvatarSize = 16;

}  // namespace

@interface TabStripCoordinator () <CreateOrEditTabGroupCoordinatorDelegate,
                                   TabStripCommands,
                                   TabStripMediatorDelegate>

// Mediator for updating the TabStrip when the WebStateList changes.
@property(nonatomic, strong) TabStripMediator* mediator;
// Helper providing context menu for tab strip items.
@property(nonatomic, strong) TabStripContextMenuHelper* contextMenuHelper;
// The view controller for the tab strip.
@property(nonatomic, strong) TabStripViewController* tabStripViewController;
// Callback invoked upon confirming leaving or deleting a shared group.
@property(nonatomic, copy) void (^leaveOrDeleteCompletion)
    (CollaborationControllerDelegate::Outcome);

@end

@implementation TabStripCoordinator {
  SharingCoordinator* _sharingCoordinator;
  CreateTabGroupCoordinator* _createTabGroupCoordinator;
  AlertCoordinator* _alertCoordinator;
  // The coordinator to handle the confirmation dialog for the action taken for
  // a tab group.
  TabGroupConfirmationCoordinator* _tabGroupConfirmationCoordinator;
}

@synthesize baseViewController = _baseViewController;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  if (self.tabStripViewController) {
    return;
  }

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TabStripCommands)];

  ProfileIOS* profile = self.profile;
  CHECK(profile);
  self.tabStripViewController = [[TabStripViewController alloc] init];
  self.tabStripViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.tabStripViewController.overrideUserInterfaceStyle =
      profile->IsOffTheRecord() ? UIUserInterfaceStyleDark
                                : UIUserInterfaceStyleUnspecified;
  self.tabStripViewController.isIncognito = profile->IsOffTheRecord();

  BrowserList* browserList = BrowserListFactory::GetForProfile(profile);
  tab_groups::TabGroupSyncService* tabGroupSyncService =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  collaboration::messaging::MessagingBackendService* messagingService =
      collaboration::messaging::MessagingBackendServiceFactory::GetForProfile(
          profile);
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  ShareKitService* shareKitService =
      ShareKitServiceFactory::GetForProfile(profile);

  FaviconLoader* faviconLoader = nil;
  // Fetch favicons if in regular mode and sync or shared tab groups is enabled.
  if (!profile->IsOffTheRecord() &&
      IsSharedTabGroupsJoinEnabled(collaborationService)) {
    faviconLoader = IOSChromeFaviconLoaderFactory::GetForProfile(profile);
  }

  self.mediator =
      [[TabStripMediator alloc] initWithConsumer:self.tabStripViewController
                             tabGroupSyncService:tabGroupSyncService
                                     browserList:browserList
                                messagingService:messagingService
                                 shareKitService:shareKitService
                            collaborationService:collaborationService
                                   faviconLoader:faviconLoader];
  self.mediator.browser = self.browser;
  self.mediator.profile = profile;
  self.mediator.tabStripHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabStripCommands);
  self.mediator.URLLoader = UrlLoadingBrowserAgent::FromBrowser(self.browser);

  self.contextMenuHelper = [[TabStripContextMenuHelper alloc]
      initWithBrowserList:browserList
             webStateList:self.browser->GetWebStateList()];
  self.contextMenuHelper.incognito = profile->IsOffTheRecord();
  self.contextMenuHelper.profile = profile;
  self.contextMenuHelper.mutator = self.mediator;
  self.contextMenuHelper.handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabStripCommands);
  self.mediator.delegate = self;

  self.tabStripViewController.mutator = self.mediator;
  self.tabStripViewController.dragDropHandler = self.mediator;
  self.tabStripViewController.snapshotAndfaviconDataSource = self.mediator;
  self.tabStripViewController.tabGroupCellDataSource = self.mediator;
  self.tabStripViewController.contextMenuProvider = self.contextMenuHelper;
}

- (void)stop {
  [self clearLeaveOrDeleteCompletion];
  if (_tabGroupConfirmationCoordinator) {
    [_tabGroupConfirmationCoordinator stop];
    _tabGroupConfirmationCoordinator = nil;
  }
  [_sharingCoordinator stop];
  _sharingCoordinator = nil;
  [self.contextMenuHelper disconnect];
  self.contextMenuHelper = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(TabStripCommands)];
  self.tabStripViewController = nil;
}

#pragma mark - TabStripCommands

- (void)showTabStripGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers {
  [self hideTabStripGroupCreation];
  _createTabGroupCoordinator = [[CreateTabGroupCoordinator alloc]
      initTabGroupCreationWithBaseViewController:self.baseViewController
                                         browser:self.browser
                                    selectedTabs:identifiers];
  _createTabGroupCoordinator.delegate = self;
  [_createTabGroupCoordinator start];
}

- (void)showTabStripGroupEditionForGroup:
    (base::WeakPtr<const TabGroup>)tabGroup {
  if (!tabGroup) {
    return;
  }
  [self hideTabStripGroupCreation];
  _createTabGroupCoordinator = [[CreateTabGroupCoordinator alloc]
      initTabGroupEditionWithBaseViewController:self.baseViewController
                                        browser:self.browser
                                       tabGroup:tabGroup.get()];
  _createTabGroupCoordinator.delegate = self;
  [_createTabGroupCoordinator start];
}

- (void)hideTabStripGroupCreation {
  _createTabGroupCoordinator.delegate = nil;
  [_createTabGroupCoordinator stop];
  _createTabGroupCoordinator = nil;
}

- (void)shareItem:(TabSwitcherItem*)item originView:(UIView*)originView {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:item.URL
                                   title:item.title
                                scenario:SharingScenario::TabStripItem];
  _sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                          params:params
                      originView:originView];
  [_sharingCoordinator start];
}

- (void)showAlertForLastTabDragged:
    (TabStripLastTabDraggedAlertCommand*)command {
  if (self.profile->IsOffTheRecord()) {
    return;
  }

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  base::WeakPtr<Browser> weakBrowser = command.originBrowser->AsWeakPtr();
  __weak __typeof(self) weakSelf = self;
  tab_groups::TabGroupVisualData visualDataCopy = command.visualData;
  const base::Uuid savedIDCopy = command.savedGroupID;
  const tab_groups::TabGroupId localIDCopy = command.localGroupID;
  web::WebStateID tabIDCopy = command.tabID;
  int originIndexCopy = command.originIndex;

  NSString* title = l10n_util::GetNSString(
      IDS_IOS_TAB_GROUP_CONFIRMATION_LAST_TAB_MOVE_TITLE);
  NSString* message;
  if (identity) {
    message = l10n_util::GetNSStringF(
        IDS_IOS_TAB_GROUP_CONFIRMATION_DELETE_MESSAGE_WITH_EMAIL,
        base::SysNSStringToUTF16(identity.userEmail));
  } else {
    message = l10n_util::GetNSString(
        IDS_IOS_TAB_GROUP_CONFIRMATION_DELETE_MESSAGE_WITHOUT_EMAIL);
  }
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:title
                         message:message];
  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(
                                          IDS_IOS_CONTENT_CONTEXT_DELETEGROUP)
                               action:^(void) {
                                 [weakSelf deleteSavedGroupWithID:savedIDCopy];
                                 [weakSelf dimissAlertCoordinator];
                               }
                                style:UIAlertActionStyleDestructive];
  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               action:^(void) {
                                 if (!weakBrowser) {
                                   return;
                                 }
                                 [weakSelf cancelMoveForTab:tabIDCopy
                                              originBrowser:weakBrowser.get()
                                                originIndex:originIndexCopy
                                                 visualData:visualDataCopy
                                               localGroupID:localIDCopy
                                                    savedID:savedIDCopy];
                                 [weakSelf dimissAlertCoordinator];
                               }
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator start];
}

- (void)showAlertForLastTabRemovedFromGroup:(const TabGroup*)group
                                      tabID:(web::WebStateID)itemID
                                    closing:(BOOL)closing {
  UIView* sourceView = self.tabStripViewController.closedTabGroupView;
  SharedTabGroupLastTabAlertCommand* command =
      [[SharedTabGroupLastTabAlertCommand alloc]
               initWithTabID:itemID
                     browser:self.browser
                       group:group
          baseViewController:self.baseViewController
                  sourceView:sourceView
                     closing:closing];
  self.tabStripViewController.closedTabGroupView = nil;

  id<SharedTabGroupLastTabAlertCommands> lastTabAlertHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         SharedTabGroupLastTabAlertCommands);
  [lastTabAlertHandler showLastTabInSharedGroupAlert:command];
}

- (void)showTabGroupConfirmationForAction:(TabGroupActionType)actionType
                                groupItem:(TabGroupItem*)tabGroupItem
                               sourceView:(UIView*)sourceView {
  _tabGroupConfirmationCoordinator = [[TabGroupConfirmationCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      actionType:actionType
                      sourceView:sourceView];

  __weak TabStripCoordinator* weakSelf = self;
  _tabGroupConfirmationCoordinator.primaryAction = ^{
    [weakSelf takeActionForActionType:actionType tabGroupItem:tabGroupItem];
  };
  _tabGroupConfirmationCoordinator.dismissAction = ^{
    [weakSelf clearLeaveOrDeleteCompletion];
  };
  _tabGroupConfirmationCoordinator.tabGroupName = tabGroupItem.title;

  [_tabGroupConfirmationCoordinator start];
  self.tabStripViewController.tabGroupConfirmationHandler =
      _tabGroupConfirmationCoordinator;
}

- (void)showTabStripTabGroupSnackbarAfterClosingGroups:
    (int)numberOfClosedGroups {
  if (self.profile->IsOffTheRecord()) {
    return;
  }

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
  NSString* messageLabel =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_TAB_GROUP_SNACKBAR_LABEL, numberOfClosedGroups));
  SnackbarMessage* message =
      [[SnackbarMessage alloc] initWithTitle:messageLabel];
  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.handler = openTabGroupPanelAction;
  action.title = l10n_util::GetNSString(IDS_IOS_TAB_GROUP_SNACKBAR_ACTION);
  message.action = action;

  id<SnackbarCommands> snackbarCommandsHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarCommandsHandler showSnackbarMessage:message];
}

- (void)manageTabGroup:(base::WeakPtr<const TabGroup>)group {
  [self showShareOrManageForGroup:group
                       entryPoint:CollaborationServiceShareOrManageEntryPoint::
                                      kiOSTabStripManage];
}

- (void)shareTabGroup:(base::WeakPtr<const TabGroup>)group {
  [self showShareOrManageForGroup:group
                       entryPoint:CollaborationServiceShareOrManageEntryPoint::
                                      kiOSTabStripShare];
}

- (void)showRecentActivityForTabGroup:(base::WeakPtr<const TabGroup>)tabGroup {
  id<TabGroupsCommands> tabGroupsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);
  [tabGroupsHandler showRecentActivityForGroup:tabGroup];
}

- (void)startLeaveOrDeleteSharedGroupItem:(TabGroupItem*)tabGroupItem
                                forAction:(TabGroupActionType)actionType
                               sourceView:(UIView*)sourceView {
  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(ResultCallback)> completionCallback =
      base::BindOnce(^(ResultCallback resultCallback) {
        TabStripCoordinator* strongSelf = weakSelf;
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

        [strongSelf showTabGroupConfirmationForAction:actionType
                                            groupItem:tabGroupItem
                                           sourceView:sourceView];
      });

  Browser* browser = self.browser;
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(
          browser->GetProfile());

  const TabGroup* tabGroup = tabGroupItem.tabGroup;
  if (!tabGroup || !collaborationService) {
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

#pragma mark - TabStripMediatorDelegate

- (id<FacePileProviding>)facePileProviderForGroupID:(const std::string&)groupID
                                         groupColor:(UIColor*)groupColor {
  // Configure the face pile.
  FacePileConfiguration* config = [[FacePileConfiguration alloc] init];
  config.showsEmptyState = NO;
  config.backgroundColor = groupColor;
  config.avatarSize = kFacePileAvatarSize;
  config.groupID = data_sharing::GroupId(groupID);

  FacePileCoordinator* facePileCoordinator =
      [[FacePileCoordinator alloc] initWithFacePileConfiguration:config
                                                         browser:self.browser];
  [facePileCoordinator start];

  return facePileCoordinator;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.tabStripViewController;
}

#pragma mark - Public

- (void)hideTabStrip:(BOOL)hidden {
  self.tabStripViewController.view.hidden = hidden;
}

#pragma mark - Private

// Cancels the move of `tabID` by moving it back to its `originBrowser` and
// `originIndex` and recreates a new group based on its original group's
// `visualData`.
- (void)cancelMoveForTab:(web::WebStateID)tabID
           originBrowser:(Browser*)originBrowser
             originIndex:(int)originIndex
              visualData:(const tab_groups::TabGroupVisualData&)visualData
            localGroupID:(const tab_groups::TabGroupId&)localGroupID
                 savedID:(const base::Uuid&)savedID {
  [_mediator cancelMoveForTab:tabID
                originBrowser:originBrowser
                  originIndex:originIndex
                   visualData:visualData
                 localGroupID:localGroupID
                      savedID:savedID];
}

// Deletes the saved group with `savedID`.
- (void)deleteSavedGroupWithID:(const base::Uuid&)savedID {
  [_mediator deleteSavedGroupWithID:savedID];
}

// Dismisses the alert coordinator.
- (void)dimissAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

// Executes a corresponded action to `actionType` and dismiss
// the confirmation coordinator.
- (void)takeActionForActionType:(TabGroupActionType)actionType
                   tabGroupItem:(TabGroupItem*)tabGroupItem {
  if (!tabGroupItem) {
    return;
  }

  switch (actionType) {
    case TabGroupActionType::kUngroupTabGroup:
      [_mediator ungroupGroup:tabGroupItem];
      break;
    case TabGroupActionType::kDeleteTabGroup:
      [_mediator deleteGroup:tabGroupItem];
      break;
    case TabGroupActionType::kLeaveSharedTabGroup:
      [self runLeaveOrDeleteCompletion];
      break;
    case TabGroupActionType::kDeleteSharedTabGroup:
      [self runLeaveOrDeleteCompletion];
      break;
    case TabGroupActionType::kDeleteOrKeepSharedTabGroup:
    case TabGroupActionType::kLeaveOrKeepSharedTabGroup:
    case TabGroupActionType::kCloseLastTabUnknownRole:
      NOTREACHED();
  }

  [_tabGroupConfirmationCoordinator stop];
  _tabGroupConfirmationCoordinator = nil;
}

// Helper method to open a new tab when the last tab of a shared group is
// closed. By doing that, the user is keeping the group instead of deleting it.
- (void)replaceLastTabByNewTabInGroup:(TabGroupItem*)tabGroupItem {
  if (tabGroupItem) {
    [_mediator addNewTabInGroup:tabGroupItem];
    [_mediator closeSavedTabFromGroup:tabGroupItem];
  }
  [_tabGroupConfirmationCoordinator stop];
  _tabGroupConfirmationCoordinator = nil;
}

// Shows the "share" or "manage" screen for the `group`. The choice is
// automatically made based on whether the group is already shared or not.
- (void)showShareOrManageForGroup:(base::WeakPtr<const TabGroup>)group
                       entryPoint:(CollaborationServiceShareOrManageEntryPoint)
                                      entryPoint {
  Browser* browser = self.browser;
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(
          browser->GetProfile());
  const TabGroup* tabGroup = group.get();

  if (!tabGroup || !collaborationService) {
    return;
  }

  std::unique_ptr<CollaborationControllerDelegate> delegate =
      std::make_unique<IOSCollaborationControllerDelegate>(
          browser,
          CreateControllerDelegateParamsFromProfile(
              self.profile, self.baseViewController, FlowType::kShareOrManage));
  collaborationService->StartShareOrManageFlow(
      std::move(delegate), tabGroup->tab_group_id(), entryPoint);
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
