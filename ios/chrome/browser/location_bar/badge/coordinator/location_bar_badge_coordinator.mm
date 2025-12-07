// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_coordinator.h"

#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_view_controller.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_coordinator_delegate.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator_delegate.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/omnibox_util.h"

@interface LocationBarBadgeCoordinator () <
    ContextualPanelEntrypointCommands,
    ContextualPanelEntrypointMediatorDelegate,
    LocationBarBadgeMediatorDelegate>
@end

@implementation LocationBarBadgeCoordinator {
  // The mediator for location bar badge.
  LocationBarBadgeMediator* _mediator;
  // The mediator for contextual panel badge and chip.
  ContextualPanelEntrypointMediator* _contextualPanelEntryPointMediator;
  // Observer that updates LocationBarBadgeViewController for
  // fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _locationBarBadgeFullscreenUIUpdater;
  // The AnimatedFullscreenDisabler to disable fullscreen momentarily as the
  // large entrypoint is shown.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> _animatedFullscreenDisabler;
  // Command dispatcher.
  CommandDispatcher* _dispatcher;
  // Pref service.
  raw_ptr<PrefService> _prefService;
}

#pragma mark - Public

- (void)start {
  _viewController = [[LocationBarBadgeViewController alloc] init];
  _viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(self.browser);
  _dispatcher = self.browser->GetCommandDispatcher();
  if (IsContextualPanelEnabled()) {
    [self createContextualPanelEntryPointMediator];
  }
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
  _prefService = self.browser->GetProfile()->GetPrefs();
  _mediator = [[LocationBarBadgeMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
                   tracker:tracker
               prefService:_prefService
             geminiService:BwgServiceFactory::GetForProfile(self.profile)];
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _viewController.mutator = _mediator;
  id<BWGCommands> BWGCommandHandler =
      HandlerForProtocol(_dispatcher, BWGCommands);
  _mediator.BWGCommandHandler = BWGCommandHandler;
  [_dispatcher startDispatchingToTarget:_mediator
                            forProtocol:@protocol(LocationBarBadgeCommands)];
}

- (void)stop {
  _viewController = nil;
  [self stopContextualPanelEntrypointMediator];
  [_dispatcher stopDispatchingToTarget:_mediator];
  _dispatcher = nil;
  [_mediator disconnect];
  _mediator = nil;
  _locationBarBadgeFullscreenUIUpdater = nullptr;
  _animatedFullscreenDisabler = nullptr;
}

- (void)addIncognitoBadgeViewController:
    (IncognitoBadgeViewController*)incognitoViewController {
  self.viewController.incognitoBadgeViewController = incognitoViewController;
}

// TODO(crbug.com/454351425): Remove pragma when Contextual Panel Entry Point is
// integrated with LocationBarBadgeMediator.
#pragma mark - ContextualPanelEntrypointMediatorDelegate
#pragma mark - LocationBarBadgeMediatorDelegate

- (BOOL)canShowLargeContextualPanelEntrypoint:
    (ContextualPanelEntrypointMediator*)mediator {
  // TODO(crbug.com/450006763): Connect coordinator to LocationBarCoordinator.
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

- (void)notifyContextualPanelEntrypointIPHDismissed {
  [self enableFullscreen];
  [_contextualPanelEntryPointMediator.consumer setEntrypointColored:NO];
}

- (void)cancelContextualPanelEntrypointLoudMoment {
  [_contextualPanelEntryPointMediator
      cancelContextualPanelEntrypointLoudMoment];
}

#pragma mark - Private

// TODO(crbug.com/454351425): Remove when Contextual Panel Entry Point is
// integrated with LocationBarBadgeMediator.
// Creates a Contextual Panel entry point mediator.
- (void)createContextualPanelEntryPointMediator {
  WebStateList* webStateList = self.browser->GetWebStateList();

  [_dispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(ContextualPanelEntrypointCommands)];

  id<ContextualSheetCommands> contextualSheetHandler =
      HandlerForProtocol(_dispatcher, ContextualSheetCommands);
  id<ContextualPanelEntrypointIPHCommands> entrypointHelpHandler =
      HandlerForProtocol(_dispatcher, ContextualPanelEntrypointIPHCommands);

  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);

  _contextualPanelEntryPointMediator =
      [[ContextualPanelEntrypointMediator alloc]
            initWithWebStateList:webStateList
               engagementTracker:engagementTracker
          contextualSheetHandler:contextualSheetHandler
           entrypointHelpHandler:entrypointHelpHandler];
  _contextualPanelEntryPointMediator.delegate = self;
  _contextualPanelEntryPointMediator.visibilityDelegate = _viewController;

  _contextualPanelEntryPointMediator.consumer = _viewController;
  _viewController.contextualPanelEntryPointMutator =
      _contextualPanelEntryPointMediator;

  _locationBarBadgeFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      FullscreenController::FromBrowser(self.browser), self.viewController);
}

// Cleans up ContextualPanelEntrypointMediator.
- (void)stopContextualPanelEntrypointMediator {
  [_dispatcher stopDispatchingToTarget:self];

  [_contextualPanelEntryPointMediator disconnect];
  _contextualPanelEntryPointMediator.consumer = nil;
  _contextualPanelEntryPointMediator.delegate = nil;
  _contextualPanelEntryPointMediator = nil;
}

@end
