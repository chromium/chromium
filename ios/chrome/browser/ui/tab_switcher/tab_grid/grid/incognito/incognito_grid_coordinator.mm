// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_coordinator.h"

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_mediator.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/disabled_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_coordinator_audience.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_state_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_theme.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_view_controller.h"

@interface IncognitoGridCoordinator ()

// Redefined as readwrite.
@property(nonatomic, readwrite, strong)
    IncognitoGridViewController* gridViewController;

@end

@implementation IncognitoGridCoordinator {
  // Reauth scene agent.
  IncognitoReauthSceneAgent* _reauthAgent;
  // Mediator for incognito reauth.
  IncognitoReauthMediator* _incognitoAuthMediator;
  // Context Menu helper for the tabs.
  TabContextMenuHelper* _tabContextMenuHelper;
  // Whether the incognito mode is currently enabled. This is not computed as we
  // might need to refer to the previous value.
  BOOL _incognitoEnabled;
  // Pointer to the browser. Even if this coordinator super class has a readonly
  // browser property, it is also kept locally as it must be readwrite here.
  base::WeakPtr<Browser> _browser;
  // Mediator of incognito grid.
  IncognitoGridMediator* _mediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           toolbarsMutator:
                               (id<GridToolbarsMutator>)toolbarsMutator
                      gridMediatorDelegate:(id<GridMediatorDelegate>)delegate {
  CHECK(baseViewController);
  CHECK(browser);
  CHECK(toolbarsMutator);
  CHECK(delegate);
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser
                                toolbarsMutator:toolbarsMutator
                           gridMediatorDelegate:delegate])) {
    _browser = browser->AsWeakPtr();
    _incognitoEnabled = !IsIncognitoModeDisabled(
        self.browser->GetProfile()->GetOriginalProfile()->GetPrefs());
  }
  return self;
}

#pragma mark - Property Implementation.

- (IncognitoGridMediator*)mediator {
  CHECK(_mediator)
      << "IncognitoGridCoordinator's -start should be called before.";
  return _mediator;
}

- (IncognitoGridMediator*)incognitoGridMediator {
  CHECK(_mediator)
      << "IncognitoGridCoordinator's -start should be called before.";
  return _mediator;
}

- (Browser*)browser {
  return _browser.get();
}

- (id)gridHandler {
  CHECK(_mediator)
      << "IncognitoGridCoordinator's -start should be called before.";
  return _mediator;
}

#pragma mark - Superclass overrides

- (LegacyGridTransitionLayout*)transitionLayout {
  if (IsTabGroupInGridEnabled()) {
    if (self.tabGroupCoordinator) {
      return [self.tabGroupCoordinator.viewController
                  .gridViewController transitionLayout];
    }
  }
  return [self.gridViewController transitionLayout];
}

#pragma mark - ChromeCoordinator

- (void)start {
  // TODO(crbug.com/40789579): refactor to call setIncognitoBrowser from this
  // function.
  _reauthAgent =
      [IncognitoReauthSceneAgent agentFromScene:self.browser->GetSceneState()];

  _mediator =
      [[IncognitoGridMediator alloc] initWithModeHolder:self.modeHolder];
  _mediator.incognitoDelegate = self;
  _mediator.reauthSceneAgent = _reauthAgent;

  GridContainerViewController* container =
      [[GridContainerViewController alloc] init];
  self.gridContainerViewController = container;

  _tabContextMenuHelper = [[TabContextMenuHelper alloc]
             initWithProfile:self.browser->GetProfile()
      tabContextMenuDelegate:self.tabContextMenuDelegate];

  if (_incognitoEnabled) {
    self.gridViewController = [self createGridViewController];
    container.containedViewController = self.gridViewController;
  } else {
    self.disabledViewController = [self createDisabledViewController];
    container.containedViewController = self.disabledViewController;
  }

  _incognitoAuthMediator =
      [[IncognitoReauthMediator alloc] initWithReauthAgent:_reauthAgent];
  _incognitoAuthMediator.consumer = self.gridViewController;

  [super start];
}

- (void)stop {
  _tabContextMenuHelper = nil;
  _incognitoAuthMediator = nil;
  _reauthAgent = nil;

  [super stop];
}

#pragma mark - Public

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  if (_browser) {
    [_browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  }
  _mediator.browser = incognitoBrowser;
  _browser.reset();
  if (incognitoBrowser) {
    _browser = incognitoBrowser->AsWeakPtr();
    _tabContextMenuHelper.profile = incognitoBrowser->GetProfile();
    [incognitoBrowser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(TabGroupsCommands)];

    _mediator.tabGroupsHandler = self;
  } else {
    _tabContextMenuHelper.profile = nullptr;
  }
}

#pragma mark - IncognitoGridMediatorDelegate

- (void)shouldDisableIncognito:(BOOL)disable {
  if (disable && _incognitoEnabled) {
    self.disabledViewController = [self createDisabledViewController];

    // Changing the containedViewController remove the current one.
    self.gridContainerViewController.containedViewController =
        self.disabledViewController;
    self.gridViewController = nil;
  } else if (!disable && !_incognitoEnabled) {
    self.gridViewController = [self createGridViewController];

    // Changing the containedViewController remove the current one.
    self.gridContainerViewController.containedViewController =
        self.gridViewController;
    self.disabledViewController = nil;
  }

  _incognitoEnabled = !disable;

  [self.audience incognitoGridDidChange];
}

#pragma mark - Private

// Creates and returns a configured Grid. Also sets the consumer in the
// mediator.
- (IncognitoGridViewController*)createGridViewController {
  CHECK(_reauthAgent);
  CHECK(_mediator);
  CHECK(_tabContextMenuHelper);
  IncognitoGridViewController* gridViewController =
      [[IncognitoGridViewController alloc] init];
  gridViewController.reauthHandler = _reauthAgent;
  gridViewController.menuProvider = _tabContextMenuHelper;

  gridViewController.dragDropHandler = _mediator;
  gridViewController.mutator = _mediator;
  gridViewController.gridProvider = _mediator;
  // TODO(crbug.com/40273478): Move the following lines to the grid itself when
  // specific grid file will be created.
  gridViewController.view.accessibilityIdentifier = kIncognitoTabGridIdentifier;
  gridViewController.emptyStateView =
      [[TabGridEmptyStateView alloc] initWithPage:TabGridPageIncognitoTabs];
  gridViewController.emptyStateView.accessibilityIdentifier =
      kTabGridIncognitoTabsEmptyStateIdentifier;
  gridViewController.theme = GridThemeDark;

  _mediator.consumer = gridViewController;

  return gridViewController;
}

// Creates and returns a configured disabled view controller.
- (DisabledGridViewController*)createDisabledViewController {
  DisabledGridViewController* disabledViewController =
      [[DisabledGridViewController alloc]
          initWithPage:TabGridPageIncognitoTabs];
  disabledViewController.delegate = self.disabledTabViewControllerDelegate;

  return disabledViewController;
}

@end
