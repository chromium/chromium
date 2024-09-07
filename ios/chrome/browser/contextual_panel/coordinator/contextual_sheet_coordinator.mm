// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/contextual_sheet_coordinator.h"

#import "ios/chrome/browser/contextual_panel/coordinator/contextual_sheet_presenter.h"
#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_coordinator.h"
#import "ios/chrome/browser/contextual_panel/ui/contextual_sheet_view_controller.h"
#import "ios/chrome/browser/contextual_panel/ui/trait_collection_change_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent_observing.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/common/ui/util/ui_util.h"

@interface ContextualSheetCoordinator () <OmniboxPositionBrowserAgentObserving,
                                          TraitCollectionChangeDelegate>

@end

@implementation ContextualSheetCoordinator {
  ContextualSheetViewController* _viewController;

  PanelContentCoordinator* _panelContentCoordinator;

  // The AnimatedFullscreenDisabler to disable fullscreen when the bottom
  // omnibox + contextual sheet are open.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> _animatedFullscreenDisabler;

  // Bridge to observe the OmniboxPositionBrowserAgent.
  std::unique_ptr<OmniboxPositionBrowserAgentObserverBridge> _observerBridge;
}

- (void)start {
  OmniboxPositionBrowserAgent* browserAgent =
      OmniboxPositionBrowserAgent::FromBrowser(self.browser);
  _observerBridge = std::make_unique<OmniboxPositionBrowserAgentObserverBridge>(
      self, browserAgent);

  // On iPad, let the panel coordinator present directly using iOS's built-in
  // UISheetController.
  if (IsRegularXRegularSizeClass(self.baseViewController)) {
    [self createPanelCoordinatorWithBaseViewController:self.baseViewController];
  } else {
    // On iPhone/iPad multiwindow, use the custom sheet this coordinator
    // provides.
    __weak __typeof(self) weakSelf = self;
    [self addViewControllerToBaseViewControllerAnimated:YES
                                     panelAdditionBlock:^{
                                       [weakSelf
                                           createPanelCoordinatorAtStartBlock];
                                     }];
  }
}

- (void)stop {
  [_panelContentCoordinator stop];
  _panelContentCoordinator = nil;

  if (_viewController) {
    [self removeViewControllerFromBaseViewController];
  }

  _animatedFullscreenDisabler = nullptr;
  _observerBridge = nullptr;
}

- (void)traitCollectionDidChangeForViewController:
    (UIViewController*)viewController {
  // Use existence of `_viewController` as a signal for whether the new layout
  // has been prepared.
  if (IsRegularXRegularSizeClass(self.baseViewController) && _viewController) {
    [self removeViewControllerFromBaseViewController];
    [_panelContentCoordinator
        presentFromNewBaseViewController:self.baseViewController];
  } else if (!IsRegularXRegularSizeClass(self.baseViewController) &&
             !_viewController) {
    __weak __typeof(self) weakSelf = self;
    [self addViewControllerToBaseViewControllerAnimated:NO
                                     panelAdditionBlock:^{
                                       [weakSelf embedPanelCoordinator];
                                     }];
  }
}

// Helper block for creating the panel coordinator in a block from -start.
- (void)createPanelCoordinatorAtStartBlock {
  [self createPanelCoordinatorWithBaseViewController:_viewController];
}

// Creates the panel coordinator using the given base view controller for the
// new coordinator.
- (void)createPanelCoordinatorWithBaseViewController:
    (UIViewController*)baseViewController {
  _panelContentCoordinator = [[PanelContentCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:self.browser];
  _panelContentCoordinator.traitCollectionDelegate = self;
  [_panelContentCoordinator start];
}

// Embeds the panel coordinator into the right parent view controller.
- (void)embedPanelCoordinator {
  [_panelContentCoordinator embedInParentViewController:_viewController];
}

// Removes this coordinator' view controller from the base.
- (void)removeViewControllerFromBaseViewController {
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];

  _viewController = nil;
}

// Adds this coordinator's view controller to the base view controller.
// `additionBlock` is the block that actually adds the panel's content into this
// coordinator's view controller.
- (void)addViewControllerToBaseViewControllerAnimated:(BOOL)animated
                                   panelAdditionBlock:
                                       (void (^)(void))additionBlock {
  _viewController = [[ContextualSheetViewController alloc] init];
  _viewController.contextualSheetHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ContextualSheetCommands);
  _viewController.traitCollectionDelegate = self;

  [self.baseViewController addChildViewController:_viewController];

  [self.presenter insertContextualSheet:_viewController.view];

  [_viewController didMoveToParentViewController:self.baseViewController];

  OmniboxPositionBrowserAgent* browserAgent =
      OmniboxPositionBrowserAgent::FromBrowser(self.browser);
  if (browserAgent->IsCurrentLayoutBottomOmnibox()) {
    [self disableFullscreen];
  }

  if (additionBlock) {
    additionBlock();
  }

  if (animated) {
    [_viewController animateAppearance];
  }
}

- (void)disableFullscreen {
  _animatedFullscreenDisabler =
      std::make_unique<AnimatedScopedFullscreenDisabler>(
          FullscreenController::FromBrowser(self.browser));
  _animatedFullscreenDisabler->StartAnimation();
}

- (void)enableFullscreen {
  _animatedFullscreenDisabler = nullptr;
}

#pragma mark - Boolean Observer

- (void)omniboxPositionBrowserAgent:(OmniboxPositionBrowserAgent*)browser_agent
       isCurrentLayoutBottomOmnibox:(BOOL)isCurrentLayoutBottomOmnibox {
  if (isCurrentLayoutBottomOmnibox) {
    [self disableFullscreen];
  } else {
    [self enableFullscreen];
  }
}

@end
