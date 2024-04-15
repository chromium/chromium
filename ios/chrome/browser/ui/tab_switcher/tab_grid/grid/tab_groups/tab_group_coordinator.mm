// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_coordinator.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_transition_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"
#import "ios/web/public/web_state_id.h"

@interface TabGroupCoordinator () <GridViewControllerDelegate>
@end

@implementation TabGroupCoordinator {
  // Mediator for tab groups.
  TabGroupMediator* _mediator;
  // View controller for tab groups.
  TabGroupViewController* _viewController;
  // Transition delegate for the animation to show/hide a Tab Group.
  TabGroupTransitionDelegate* _transitionDelegate;
  // Context Menu helper for the tabs.
  TabContextMenuHelper* _tabContextMenuHelper;
  // Tab group to display.
  const TabGroup* _tabGroup;
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
  }
  return self;
}

- (UIViewController*)viewController {
  return _viewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  id<TabGroupsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);
  _viewController = [[TabGroupViewController alloc]
      initWithHandler:handler
           lightTheme:!self.browser->GetBrowserState()->IsOffTheRecord()
             tabGroup:_tabGroup];

  _viewController.gridViewController.delegate = self;

  _mediator = [[TabGroupMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
                  tabGroup:_tabGroup->GetWeakPtr()
                  consumer:_viewController
              gridConsumer:_viewController.gridViewController];
  _mediator.browser = self.browser;
  _mediator.tabGroupsHandler = handler;

  _tabContextMenuHelper = [[TabContextMenuHelper alloc]
        initWithBrowserState:self.browser->GetBrowserState()
      tabContextMenuDelegate:self.tabContextMenuDelegate];

  _viewController.mutator = _mediator;
  _viewController.gridViewController.mutator = _mediator;
  _viewController.gridViewController.menuProvider = _tabContextMenuHelper;
  _viewController.gridViewController.dragDropHandler = _mediator;

  _viewController.modalPresentationStyle = UIModalPresentationCustom;
  _transitionDelegate = [[TabGroupTransitionDelegate alloc]
      initWithTabGroupViewController:_viewController];
  _transitionDelegate.smallerMotions = self.smallerMotions;
  _viewController.transitioningDelegate = _transitionDelegate;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  _mediator = nil;
  _tabContextMenuHelper = nil;

  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didSelectItemWithID:(web::WebStateID)itemID {
  BOOL incognito = self.browser->GetBrowserState()->IsOffTheRecord();
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
    [_mediator selectItemWithID:itemID pinned:NO];
  }

  id<TabGroupsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);

  [handler hideTabGroup];
  [handler showActiveTab];
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
            didSelectGroup:(const TabGroup*)group {
  NOTREACHED_NORETURN();
}

// TODO(crbug.com/1457146): Remove once inactive tabs do not depends on it
// anymore.
- (void)gridViewController:(BaseGridViewController*)gridViewController
        didCloseItemWithID:(web::WebStateID)itemID {
  NOTREACHED_NORETURN();
}

- (void)gridViewControllerDidMoveItem:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count {
  // No-op.
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didRemoveItemWIthID:(web::WebStateID)itemID {
  // No-op.
}

- (void)gridViewControllerDragSessionWillBegin:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerDragSessionDidEnd:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerScrollViewDidScroll:
    (BaseGridViewController*)gridViewController {
  // No-op.
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
  NOTREACHED_NORETURN();
}

- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (BaseGridViewController*)gridViewController {
  NOTREACHED_NORETURN();
}

@end
