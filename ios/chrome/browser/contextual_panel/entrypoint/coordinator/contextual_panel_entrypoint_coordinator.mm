// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_coordinator.h"

#import "base/check.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_coordinator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_view_controller.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_visibility_delegate.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/ui/util/omnibox_util.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"

@interface ContextualPanelEntrypointCoordinator () <
    ContextualPanelEntrypointCommands,
    ContextualPanelEntrypointMediatorDelegate> {
  // Observer that updates ContextualPanelEntrypointViewController for
  // fullscreen events.
  std::unique_ptr<FullscreenUIUpdater>
      _contextualPanelEntrypointFullscreenUIUpdater;

  // The AnimatedFullscreenDisabler to disable fullscreen momentarily as the
  // large entrypoint is shown.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> _animatedFullscreenDisabler;
}

// The mediator for this coordinator.
@property(nonatomic, strong) ContextualPanelEntrypointMediator* mediator;

@end

@implementation ContextualPanelEntrypointCoordinator

- (void)start {
  [super start];
  _viewController = [[ContextualPanelEntrypointViewController alloc] init];
  _viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(self.browser);
  _viewController.visibilityDelegate = self.visibilityDelegate;

  WebStateList* webStateList = self.browser->GetWebStateList();
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();

  [dispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(ContextualPanelEntrypointCommands)];

  id<ContextualSheetCommands> contextualSheetHandler =
      HandlerForProtocol(dispatcher, ContextualSheetCommands);
  id<ContextualPanelEntrypointIPHCommands> entrypointHelpHandler =
      HandlerForProtocol(dispatcher, ContextualPanelEntrypointIPHCommands);

  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());

  _mediator = [[ContextualPanelEntrypointMediator alloc]
        initWithWebStateList:webStateList
           engagementTracker:engagementTracker
      contextualSheetHandler:contextualSheetHandler
       entrypointHelpHandler:entrypointHelpHandler];
  _mediator.delegate = self;

  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;

  _contextualPanelEntrypointFullscreenUIUpdater =
      std::make_unique<FullscreenUIUpdater>(
          FullscreenController::FromBrowser(self.browser), self.viewController);
}

- (void)stop {
  CHECK(_viewController);
  CHECK(_mediator);

  [super stop];

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher stopDispatchingToTarget:self];

  _animatedFullscreenDisabler = nullptr;

  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator.delegate = nil;
  _mediator = nil;

  _viewController.mutator = nil;
  _viewController = nil;
  _contextualPanelEntrypointFullscreenUIUpdater = nullptr;
}

#pragma mark ContextualPanelEntrypointMediatorDelegate

- (BOOL)canShowLargeContextualPanelEntrypoint:
    (ContextualPanelEntrypointMediator*)mediator {
  return [self.delegate canShowLargeContextualPanelEntrypoint:self];
}

- (void)setLocationBarLabelCenteredBetweenContent:
            (ContextualPanelEntrypointMediator*)mediator
                                         centered:(BOOL)centered {
  [self.delegate setLocationBarLabelCenteredBetweenContent:self
                                                  centered:centered];
}

- (void)enableFullscreen {
  _animatedFullscreenDisabler = nullptr;
}

- (void)disableFullscreen {
  _animatedFullscreenDisabler =
      std::make_unique<AnimatedScopedFullscreenDisabler>(
          FullscreenController::FromBrowser(self.browser));
  _animatedFullscreenDisabler->StartAnimation();
}

- (BOOL)isBottomOmniboxActive {
  return IsCurrentLayoutBottomOmnibox(self.browser);
}

- (CGPoint)helpAnchorUsingBottomOmnibox:(BOOL)isBottomOmnibox {
  return [self.viewController helpAnchorUsingBottomOmnibox:isBottomOmnibox];
}

#pragma mark - ContextualPanelEntrypointCommands

- (void)contextualPanelEntrypointIPHWasDismissed {
  [self enableFullscreen];
  [_mediator.consumer setEntrypointColored:NO];
}

@end
