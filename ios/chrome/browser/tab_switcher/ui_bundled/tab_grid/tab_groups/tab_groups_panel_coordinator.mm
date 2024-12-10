// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "components/prefs/pref_service.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_action_context.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/disabled_grid_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_mediator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_mediator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_confirmation_coordinator.h"

@interface TabGroupsPanelCoordinator () <TabGroupsPanelMediatorDelegate>
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

  ProfileIOS* profile = self.browser->GetProfile();
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

  _mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:tab_groups::TabGroupSyncServiceFactory::
                                      GetForProfile(profile)
                  shareKitService:ShareKitServiceFactory::GetForProfile(profile)
             collaborationService:collaboration::CollaborationServiceFactory::
                                      GetForProfile(profile)
              regularWebStateList:self.browser->GetWebStateList()
                    faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                      profile)
                 disabledByPolicy:regularModeDisabled
                      browserList:BrowserListFactory::GetForProfile(profile)];

  _mediator.toolbarsMutator = _toolbarsMutator;
  _mediator.tabGridHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), TabGridCommands);
  _mediator.consumer = _gridViewController;
  _mediator.delegate = self;
  _gridViewController.mutator = _mediator;
  _gridViewController.itemDataSource = _mediator;
}

- (void)stop {
  [super stop];

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
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          self.browser->GetProfile());
  tabGroupSyncService->OpenTabGroup(
      syncID,
      std::make_unique<tab_groups::IOSTabGroupActionContext>(self.browser));
}

- (void)tabGroupsPanelMediator:(TabGroupsPanelMediator*)tabGroupsPanelMediator
    showDeleteConfirmationWithSyncID:(const base::Uuid)syncID
                          sourceView:(UIView*)sourceView {
  _tabGroupConfirmationCoordinator = [[TabGroupConfirmationCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      actionType:TabGroupActionType::kDeleteTabGroup
                      sourceView:sourceView];
  __weak TabGroupsPanelCoordinator* weakSelf = self;
  _tabGroupConfirmationCoordinator.action = ^{
    [weakSelf deleteSyncedTabGroup:syncID];
  };

  [_tabGroupConfirmationCoordinator start];
}

#pragma mark - Private

// Deletes a synced tab group and dismisses the confirmation coordinator.
- (void)deleteSyncedTabGroup:(const base::Uuid&)syncID {
  [_mediator deleteSyncedTabGroup:syncID];
  [_tabGroupConfirmationCoordinator stop];
  _tabGroupConfirmationCoordinator = nil;
}

@end
