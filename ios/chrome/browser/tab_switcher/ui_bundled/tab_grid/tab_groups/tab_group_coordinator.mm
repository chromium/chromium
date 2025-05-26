// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_coordinator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/collaboration_flow_entry_point.h"
#import "components/collaboration/public/collaboration_flow_type.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "components/data_sharing/public/group_data.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_factory.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_face_pile_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_manage_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/shared_tab_group_last_tab_closed_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/tab_group_grid_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_context_menu/tab_context_menu_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_idle_status_handler.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/shared_tab_group_user_education_coordinator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_mediator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_positioner.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_presentation_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_confirmation_coordinator.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/device_form_factor.h"

using collaboration::CollaborationServiceShareOrManageEntryPoint;
using collaboration::FlowType;
using collaboration::IOSCollaborationControllerDelegate;

namespace {
constexpr CGFloat kTabGroupPresentationDuration = 0.3;
constexpr CGFloat kTabGroupDismissalDuration = 0.25;
constexpr CGFloat kTabGroupBackgroundElementDurationFactor = 0.75;
}  // namespace

@interface TabGroupCoordinator () <
    GridViewControllerDelegate,
    SharedTabGroupUserEducationCoordinatorDelegate,
    TabGroupPresentationCommands>
@end

@implementation TabGroupCoordinator {
  // Mediator for tab groups.
  TabGroupMediator* _mediator;
  // View controller for tab groups.
  TabGroupViewController* _viewController;
  // Context Menu helper for the tabs.
  TabContextMenuHelper* _tabContextMenuHelper;
  // Tab group to display.
  raw_ptr<const TabGroup> _tabGroup;
  // The coordinator for the user education half screen.
  SharedTabGroupUserEducationCoordinator* _userEducationCoordinator;
  // Coordinator that handles confirmation dialog when the last tab of a group
  // is closed.
  TabGroupConfirmationCoordinator* _lastTabClosingAlert;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  tabGroup:(const TabGroup*)tabGroup {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group coordinator outside the "
         "Tab Groups experiment.";
  CHECK(tabGroup) << "You need to pass a tab group in order to display it.";
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _tabGroup = tabGroup;
    _animatedPresentation = YES;
  }
  return self;
}

- (UIViewController*)viewController {
  return _viewController;
}

- (void)stopChildCoordinators {
  [_viewController.gridViewController dismissModals];
  [_userEducationCoordinator stop];
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self setUpViewController];
  ProfileIOS* profile = self.profile;
  Browser* browser = self.browser;

  tab_groups::TabGroupSyncService* tabGroupSyncService =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  ShareKitService* shareKitService =
      ShareKitServiceFactory::GetForProfile(profile);
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  collaboration::messaging::MessagingBackendService* messagingService =
      collaboration::messaging::MessagingBackendServiceFactory::GetForProfile(
          profile);
  data_sharing::DataSharingService* dataSharingService =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);

  _mediator = [[TabGroupMediator alloc]
      initWithWebStateList:browser->GetWebStateList()
       tabGroupSyncService:tabGroupSyncService
           shareKitService:shareKitService
      collaborationService:collaborationService
        dataSharingService:dataSharingService
                  tabGroup:_tabGroup->GetWeakPtr()
                  consumer:_viewController
              gridConsumer:_viewController.gridViewController
                modeHolder:self.modeHolder
          messagingService:messagingService];
  _mediator.browser = browser;
  _mediator.tabGroupsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);
  _mediator.tabGridIdleStatusHandler = self.tabGridIdleStatusHandler;
  _mediator.baseDelegate = self;

  _tabContextMenuHelper = [[TabContextMenuHelper alloc]
             initWithProfile:browser->GetProfile()
      tabContextMenuDelegate:self.tabContextMenuDelegate];

  _viewController.mutator = _mediator;
  _viewController.gridViewController.mutator = _mediator;
  _viewController.gridViewController.gridProvider = _mediator;
  _viewController.gridViewController.menuProvider = _tabContextMenuHelper;
  _viewController.gridViewController.dragDropHandler = _mediator;
  _viewController.gridViewController.snapshotAndfaviconDataSource = _mediator;

  [self displayViewControllerAnimated:self.animatedPresentation];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _tabContextMenuHelper = nil;

  [_userEducationCoordinator stop];
  _userEducationCoordinator = nil;

  [self hideViewControllerAnimated:YES];

  _viewController = nil;
}

#pragma mark - Animation helpers

- (void)displayViewControllerAnimated:(BOOL)animated {
  CHECK(_viewController);

  [self.baseViewController addChildViewController:_viewController];

  UIView* viewAbove = [self.tabGroupPositioner viewAboveTabGroup];
  _viewController.view.frame = self.baseViewController.view.bounds;
  if (viewAbove && viewAbove.superview) {
    [self.baseViewController.view insertSubview:_viewController.view
                                   belowSubview:viewAbove];
  } else {
    [self.baseViewController.view addSubview:_viewController.view];
  }
  [_viewController fadeBlurIn];
  [_viewController didMoveToParentViewController:self.baseViewController];

  if (!animated) {
    [_viewController contentWillAppearAnimated:NO];
    return;
  }

  __weak TabGroupViewController* viewController = _viewController;

  [_viewController prepareForPresentation];

  CGFloat nonGridElementDuration =
      kTabGroupPresentationDuration * kTabGroupBackgroundElementDurationFactor;
  CGFloat topElementDelay =
      kTabGroupPresentationDuration - nonGridElementDuration;

  [UIView animateWithDuration:nonGridElementDuration
                        delay:topElementDelay
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
                     [viewController animateTopElementsPresentation];
                   }
                   completion:nil];

  [UIView animateWithDuration:nonGridElementDuration
                        delay:0
                      options:UIViewAnimationCurveEaseIn
                   animations:^{
                     [viewController fadeBlurIn];
                   }
                   completion:nil];

  [UIView animateWithDuration:kTabGroupPresentationDuration
                        delay:0
                      options:UIViewAnimationCurveEaseInOut
                   animations:^{
                     [viewController animateGridPresentation];
                   }
                   completion:nil];

  // Start the user education if there is an animation to avoid showing it at
  // startup.
  [self startUserEducationIfNeeded];

  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  nil);
}

- (void)hideViewControllerAnimated:(BOOL)animated {
  __weak TabGroupViewController* viewController = _viewController;

  auto completion = ^void {
    [viewController willMoveToParentViewController:nil];
    [viewController.view removeFromSuperview];
    [viewController removeFromParentViewController];
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    nil);
  };

  if (!animated) {
    completion();
  }

  [UIView animateWithDuration:kTabGroupDismissalDuration
      delay:0
      options:UIViewAnimationCurveEaseInOut
      animations:^{
        [viewController animateDismissal];
      }
      completion:^(BOOL finished) {
        completion();
      }];

  CGFloat backgroundDuration =
      kTabGroupDismissalDuration * kTabGroupBackgroundElementDurationFactor;
  [UIView animateWithDuration:backgroundDuration
                        delay:0
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
                     [viewController fadeBlurOut];
                   }
                   completion:nil];
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didSelectItemWithID:(web::WebStateID)itemID {
  BOOL incognito = self.isOffTheRecord;
  if ([_mediator isItemWithIDSelected:itemID]) {
    if (incognito) {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridIncognitoTabGroupOpenCurrentTab"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridRegularTabGroupOpenCurrentTab"));
    }
  } else {
    if (incognito) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabIncognitoGridTabGroupOpenTab"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("MobileTabRegularGridTabGroupOpenTab"));
    }
    [_mediator selectItemWithID:itemID
                         pinned:NO
         isFirstActionOnTabGrid:[self.tabGridIdleStatusHandler status]];
  }

  id<TabGroupsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);

  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  [handler showActiveTab];
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
            didSelectGroup:(const TabGroup*)group {
  NOTREACHED();
}

// TODO(crbug.com/40273478): Remove once inactive tabs do not depends on it
// anymore.
- (void)gridViewController:(BaseGridViewController*)gridViewController
        didCloseItemWithID:(web::WebStateID)itemID {
  NOTREACHED();
}

- (void)gridViewControllerDidMoveItem:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didRemoveItemWithID:(web::WebStateID)itemID {
  // No-op.
}

- (void)gridViewControllerDragSessionWillBeginForTab:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerDragSessionWillBeginForTabGroup:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerDragSessionDidEnd:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerScrollViewDidScroll:
    (BaseGridViewController*)gridViewController {
  [self.viewController gridViewControllerDidScroll];
}

- (void)gridViewControllerDropAnimationWillBegin:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerDropAnimationDidEnd:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)didTapInactiveTabsButtonInGridViewController:
    (BaseGridViewController*)gridViewController {
  NOTREACHED();
}

- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (BaseGridViewController*)gridViewController {
  NOTREACHED();
}

- (void)gridViewControllerDidRequestContextMenu:
    (BaseGridViewController*)gridViewController {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
}

- (void)gridViewControllerDropSessionDidEnter:
    (BaseGridViewController*)gridViewController {
  // No-op
}

- (void)gridViewControllerDropSessionDidExit:
    (BaseGridViewController*)gridViewController {
  // No-op
}

- (void)didTapButtonInActivitySummary:
    (BaseGridViewController*)gridViewController {
  collaboration::messaging::MessagingBackendService* messagingService =
      collaboration::messaging::MessagingBackendServiceFactory::GetForProfile(
          self.profile);
  if (!_tabGroup || !messagingService) {
    return;
  }

  tab_groups::TabGroupSyncService* tabGroupSyncService =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(self.profile);
  std::optional<tab_groups::SavedTabGroup> group =
      tabGroupSyncService->GetGroup(_tabGroup->tab_group_id());
  if (!group.has_value() || !group->collaboration_id().has_value()) {
    return;
  }

  messagingService->ClearDirtyTabMessagesForGroup(
      data_sharing::GroupId(group->collaboration_id().value().value()));
}

#pragma mark - TabGroupPresentationCommands

- (void)showShareKitFlow {
  Browser* browser = self.browser;
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(
          browser->GetProfile());

  if (!_tabGroup || !collaborationService) {
    return;
  }

  std::unique_ptr<IOSCollaborationControllerDelegate> delegate =
      std::make_unique<IOSCollaborationControllerDelegate>(
          browser,
          CreateControllerDelegateParamsFromProfile(
              self.profile, self.baseViewController, FlowType::kShareOrManage));
  tab_groups::TabGroupSyncService* tabGroupSyncService =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(self.profile);
  CollaborationServiceShareOrManageEntryPoint entryPoint =
      tab_groups::utils::IsTabGroupShared(_tabGroup, tabGroupSyncService)
          ? CollaborationServiceShareOrManageEntryPoint::kiOSTabGroupViewManage
          : CollaborationServiceShareOrManageEntryPoint::kiOSTabGroupViewShare;
  collaborationService->StartShareOrManageFlow(
      std::move(delegate), _tabGroup->tab_group_id(), entryPoint);
}

#pragma mark - SharedTabGroupUserEducationCoordinatorDelegate

- (void)userEducationCoordinatorDidDismiss:
    (SharedTabGroupUserEducationCoordinator*)coordinator {
  [_userEducationCoordinator stop];
  _userEducationCoordinator = nil;
}

#pragma mark - Private

// Sets up the `_viewController`.
- (void)setUpViewController {
  // Get the command handler for TabGroupsCommands.
  id<TabGroupsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);

  // Initialize the `_viewController`.
  _viewController =
      [[TabGroupViewController alloc] initWithHandler:handler
                                            incognito:self.isOffTheRecord
                                             tabGroup:_tabGroup];
  _viewController.gridViewController.delegate = self;
  _viewController.presentationHandler = self;
  _viewController.applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
}

// Called when the tab group is presented, to show the user education
// coordinator if necessary.
- (void)startUserEducationIfNeeded {
  tab_groups::TabGroupSyncService* syncService =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(self.profile);

  if (!tab_groups::utils::IsTabGroupShared(_tabGroup, syncService)) {
    return;
  }
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if ([defaults boolForKey:kSharedTabGroupUserEducationShownOnceKey]) {
    return;
  }

  _userEducationCoordinator = [[SharedTabGroupUserEducationCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _userEducationCoordinator.delegate = self;
  [_userEducationCoordinator start];

  // Record the presentation.
  [defaults setBool:YES forKey:kSharedTabGroupUserEducationShownOnceKey];
}

// Removes the shared tab group.
- (void)deleteSharedGroup {
  [_mediator deleteSharedTabGroup:_tabGroup];
}

// Leaves the shared tab group.
- (void)leaveSharedGroup {
  [_mediator leaveSharedTabGroup:_tabGroup];
}

// Closes the given tab and replace it with a new tab.
- (void)addNewTabInsteadOfTab:(web::WebStateID)identifier {
  GURL URL(kChromeUINewTabURL);
  int tabIndex = GetWebStateIndex(self.browser->GetWebStateList(),
                                  WebStateSearchCriteria{
                                      .identifier = identifier,
                                  });
  [_mediator insertNewWebStateAtGridIndex:tabIndex withURL:URL];
  [_mediator closeItemWithID:identifier];
}

#pragma mark - BaseGridMediatorDelegate

- (void)displayLastTabInSharedGroupAlert:(BaseGridMediator*)mediator
                                 lastTab:(web::WebStateID)itemID
                                   group:(const TabGroup*)group {
  SharedTabGroupLastTabAlertCommand* command =
      [[SharedTabGroupLastTabAlertCommand alloc]
               initWithTabID:itemID
                     browser:self.browser
                       group:group
          baseViewController:self.viewController
                  sourceView:ui::GetDeviceFormFactor() ==
                                     ui::DEVICE_FORM_FACTOR_PHONE
                                 ? self.viewController.view
                                 : nil
                     closing:YES];
  id<SharedTabGroupLastTabAlertCommands> lastTabAlertHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         SharedTabGroupLastTabAlertCommands);
  [lastTabAlertHandler showLastTabInSharedGroupAlert:command];
}

@end
