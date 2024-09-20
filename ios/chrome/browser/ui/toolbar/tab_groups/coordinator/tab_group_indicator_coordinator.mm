// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/strings/sys_string_conversions.h"
#import "components/saved_tab_groups/tab_group_sync_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_or_edit_tab_group_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_action_type.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_confirmation_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_mediator.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_mediator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"
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
  _view = [[TabGroupIndicatorView alloc] init];
  _view.displayedOnNTP = _displayedOnNTP;
  _view.incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  _view.toolbarHeightDelegate = self.toolbarHeightDelegate;
  tab_groups::TabGroupSyncService* tabGroupSyncService =
      tab_groups::TabGroupSyncServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  _mediator = [[TabGroupIndicatorMediator alloc]
          initWithProfile:self.browser->GetProfile()
      tabGroupSyncService:tabGroupSyncService
                 consumer:_view
             webStateList:self.browser->GetWebStateList()];
  _mediator.delegate = self;
  _view.mutator = _mediator;
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
