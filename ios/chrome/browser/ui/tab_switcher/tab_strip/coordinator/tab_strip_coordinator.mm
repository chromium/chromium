// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "base/uuid.h"
#import "components/strings/grit/components_strings.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_last_tab_dragged_alert_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_or_edit_tab_group_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_action_type.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_confirmation_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/context_menu/tab_strip_context_menu_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface TabStripCoordinator () <CreateOrEditTabGroupCoordinatorDelegate,
                                   TabStripCommands>

// Mediator for updating the TabStrip when the WebStateList changes.
@property(nonatomic, strong) TabStripMediator* mediator;
// Helper providing context menu for tab strip items.
@property(nonatomic, strong) TabStripContextMenuHelper* contextMenuHelper;

@property TabStripViewController* tabStripViewController;

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
  if (self.tabStripViewController)
    return;

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TabStripCommands)];

  ProfileIOS* profile = self.browser->GetProfile();
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
  self.mediator =
      [[TabStripMediator alloc] initWithConsumer:self.tabStripViewController
                             tabGroupSyncService:tabGroupSyncService
                                     browserList:browserList];
  self.mediator.webStateList = self.browser->GetWebStateList();
  self.mediator.profile = profile;
  self.mediator.browser = self.browser;
  self.mediator.tabStripHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabStripCommands);

  self.contextMenuHelper = [[TabStripContextMenuHelper alloc]
      initWithBrowserList:browserList
             webStateList:self.browser->GetWebStateList()];
  self.contextMenuHelper.incognito = profile->IsOffTheRecord();
  self.contextMenuHelper.mutator = self.mediator;
  self.contextMenuHelper.handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabStripCommands);

  self.tabStripViewController.mutator = self.mediator;
  self.tabStripViewController.dragDropHandler = self.mediator;
  self.tabStripViewController.contextMenuProvider = self.contextMenuHelper;
}

- (void)stop {
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
  ProfileIOS* profile = self.browser->GetProfile();
  if (profile->IsOffTheRecord()) {
    return;
  }

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
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

- (void)showTabGroupConfirmationForAction:(TabGroupActionType)actionType
                                groupItem:(TabGroupItem*)tabGroupItem
                               sourceView:(UIView*)sourceView {
  _tabGroupConfirmationCoordinator = [[TabGroupConfirmationCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      actionType:actionType
                      sourceView:sourceView];
  __weak TabStripCoordinator* weakSelf = self;
  _tabGroupConfirmationCoordinator.action = ^{
    switch (actionType) {
      case TabGroupActionType::kUngroupTabGroup:
        [weakSelf ungroupTabGroup:tabGroupItem];
        break;
      case TabGroupActionType::kDeleteTabGroup:
        [weakSelf deleteTabGroup:tabGroupItem];
        break;
    }
  };

  [_tabGroupConfirmationCoordinator start];
  self.tabStripViewController.tabGroupConfirmationHandler =
      _tabGroupConfirmationCoordinator;
}

- (void)showTabStripTabGroupSnackbarAfterClosingGroups:
    (int)numberOfClosedGroups {
  if (!IsTabGroupSyncEnabled() ||
      self.browser->GetProfile()->IsOffTheRecord()) {
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
    [tabGridHandler showTabGroupsPanelAnimated:NO];
  };

  // Create and config the snackbar.
  NSString* messageLabel =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_TAB_GROUP_SNACKBAR_LABEL, numberOfClosedGroups));
  MDCSnackbarMessage* message = CreateSnackbarMessage(messageLabel);
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = openTabGroupPanelAction;
  action.title = l10n_util::GetNSString(IDS_IOS_TAB_GROUP_SNACKBAR_ACTION);
  message.action = action;

  id<SnackbarCommands> snackbarCommandsHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarCommandsHandler showSnackbarMessage:message];
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

// Helper method to close a tab group and dismiss the confirmation coordinator.
- (void)deleteTabGroup:(TabGroupItem*)tabGroupItem {
  if (tabGroupItem) {
    [_mediator deleteGroup:tabGroupItem];
  }
  [_tabGroupConfirmationCoordinator stop];
  _tabGroupConfirmationCoordinator = nil;
}

// Helper method to ungroup a tab group and dismiss the confirmation
// coordinator.
- (void)ungroupTabGroup:(TabGroupItem*)tabGroupItem {
  if (tabGroupItem) {
    [_mediator ungroupGroup:tabGroupItem];
  }
  [_tabGroupConfirmationCoordinator stop];
  _tabGroupConfirmationCoordinator = nil;
}

@end
