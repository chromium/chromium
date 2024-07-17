// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/disabled_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_view_controller.h"

@implementation TabGroupsPanelCoordinator {
  // Regular browser.
  base::WeakPtr<Browser> _regularBrowser;
  // Mutator that handles toolbars changes.
  __weak id<GridToolbarsMutator> _toolbarsMutator;
  // Delegate that handles toolbars actions.
  __weak id<TabGridToolbarsMainTabGridDelegate> _toolbarTabGridDelegate;
  // Delegate that handles the screen when the Tab Groups panel is disabled.
  __weak id<DisabledGridViewControllerDelegate> _disabledViewControllerDelegate;
}

- (instancetype)
        initWithBaseViewController:(UIViewController*)baseViewController
                    regularBrowser:(Browser*)regularBrowser
                   toolbarsMutator:(id<GridToolbarsMutator>)toolbarsMutator
            toolbarTabGridDelegate:
                (id<TabGridToolbarsMainTabGridDelegate>)toolbarTabGridDelegate
    disabledViewControllerDelegate:
        (id<DisabledGridViewControllerDelegate>)disabledViewControllerDelegate {
  CHECK(baseViewController);
  CHECK(regularBrowser);
  CHECK(!regularBrowser->GetBrowserState()->IsOffTheRecord());
  CHECK(toolbarsMutator);
  CHECK(disabledViewControllerDelegate);
  self = [super initWithBaseViewController:baseViewController
                                   browser:regularBrowser];
  if (self) {
    _regularBrowser = regularBrowser->AsWeakPtr();
    _toolbarsMutator = toolbarsMutator;
    _toolbarTabGridDelegate = toolbarTabGridDelegate;
    _disabledViewControllerDelegate = disabledViewControllerDelegate;
  }
  return self;
}

- (void)start {
  [super start];

  _gridContainerViewController = [[GridContainerViewController alloc] init];

  BOOL regularModeDisabled =
      IsIncognitoModeForced(self.browser->GetBrowserState()->GetPrefs());
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
      tab_groups::TabGroupSyncServiceFactory::GetForBrowserState(
          _regularBrowser->GetBrowserState());
  WebStateList* regularWebStateList = _regularBrowser->GetWebStateList();

  _mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:tabGroupSyncService
              regularWebStateList:regularWebStateList
                 disabledByPolicy:regularModeDisabled];
  _mediator.toolbarsMutator = _toolbarsMutator;
  _mediator.toolbarTabGridDelegate = _toolbarTabGridDelegate;
  _mediator.consumer = _gridViewController;
  _gridViewController.mutator = _mediator;
  _gridViewController.itemDataSource = _mediator;
}

- (void)stop {
  [super stop];

  [_mediator disconnect];
  _mediator.toolbarsMutator = nil;
  _mediator.toolbarTabGridDelegate = nil;
  _mediator = nil;
  _gridViewController = nil;
  _disabledViewController.delegate = nil;
  _disabledViewController = nil;
  _gridContainerViewController.containedViewController = nil;
  _gridContainerViewController = nil;
}

@end
