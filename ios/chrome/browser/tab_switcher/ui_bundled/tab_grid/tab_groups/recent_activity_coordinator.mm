// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_coordinator.h"

#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/recent_activity_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_mediator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_view_controller.h"
#import "ios/web/public/web_state.h"

@interface RecentActivityCoordinator () <RecentActivityCommands>
@end

@implementation RecentActivityCoordinator {
  // A mediator of the recent activity.
  RecentActivityMediator* _mediator;
  // A view controller of the recent activity.
  RecentActivityViewController* _viewController;
  // A shared tab group currently displayed.
  base::WeakPtr<const TabGroup> _tabGroup;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                  tabGroup:
                                      (base::WeakPtr<const TabGroup>)tabGroup {
  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(
          browser->GetProfile());
  CHECK(IsSharedTabGroupsJoinEnabled(collaboration_service));
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _tabGroup = tabGroup;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[RecentActivityViewController alloc] init];

  ProfileIOS* profile = self.profile;
  _mediator = [[RecentActivityMediator alloc]
            initWithTabGroup:_tabGroup
            messagingService:collaboration::messaging::
                                 MessagingBackendServiceFactory::GetForProfile(
                                     profile)
               faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                 profile)
                 syncService:tab_groups::TabGroupSyncServiceFactory::
                                 GetForProfile(profile)
             shareKitService:ShareKitServiceFactory::GetForProfile(profile)
                webStateList:self.browser->GetWebStateList()
      webStateCreationParams:web::WebState::CreateParams(profile)];
  _mediator.consumer = _viewController;
  _mediator.recentActivityHandler = self;

  _viewController.mutator = _mediator;
  _viewController.faviconDataSource = _mediator;
  _viewController.applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  UISheetPresentationController* sheetPresentationController =
      navigationController.sheetPresentationController;
  sheetPresentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
      YES;
  sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
  ];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  _mediator = nil;
  if (_viewController) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    _viewController = nil;
  }
  [super stop];
}

#pragma mark - RecentActivityCommands

- (void)dismissViewAndExitTabGrid {
  id<TabGridCommands> tabGridHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), TabGridCommands);
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [tabGridHandler exitTabGrid];
                         }];
}

- (void)showManageScreenForGroup:(const TabGroup*)group {
  if (group != _tabGroup.get()) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }
  id<TabGroupsCommands> tabGroupsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);
  base::WeakPtr<const TabGroup> weakGroup = group->GetWeakPtr();
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [tabGroupsHandler showManageForGroup:weakGroup];
                         }];
}

- (void)showTabGroupEditForGroup:(const TabGroup*)group {
  if (group != _tabGroup.get()) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }
  id<TabGroupsCommands> tabGroupsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [tabGroupsHandler showTabGroupEditionForGroup:group];
                         }];
}

@end
