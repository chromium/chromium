// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_coordinator.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_ios.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_mediator.h"
#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_view_controller.h"
#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/coordinator/tab_group_indicator_coordinator.h"

@interface PrimaryToolbarCoordinator () <PrimaryToolbarViewControllerDelegate>

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// Redefined as PrimaryToolbarViewController.
@property(nonatomic, strong) PrimaryToolbarViewController* viewController;

@end

@implementation PrimaryToolbarCoordinator {
  // Coordinator for the tab group indicator.
  TabGroupIndicatorCoordinator* _tabGroupIndicatorCoordinator;

  // Mediator for this toolbar.
  PrimaryToolbarMediator* _mediator;
}

@dynamic viewController;

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  if (self.started) {
    return;
  }

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();

  BOOL isOffTheRecord = self.isOffTheRecord;

  self.viewController = [[PrimaryToolbarViewController alloc] init];
  self.viewController.shouldHideOmniboxOnNTP = !isOffTheRecord;
  self.viewController.omniboxCommandsHandler =
      HandlerForProtocol(dispatcher, OmniboxCommands);
  self.viewController.popupMenuCommandsHandler =
      HandlerForProtocol(dispatcher, PopupMenuCommands);
  CHECK(self.viewControllerDelegate);
  self.viewController.delegate = self;
  self.viewController.toolbarHeightDelegate = self.toolbarHeightDelegate;
  self.viewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);

  // Button factory requires that the omnibox commands are set up, which is
  // done by the location bar.
  self.viewController.buttonFactory =
      [self buttonFactoryWithType:ToolbarType::kPrimary];

  if (DefaultBrowserBannerPromoAppAgent* agent =
          [self activeBannerPromoAppAgent]) {
    _mediator = [[PrimaryToolbarMediator alloc]
        initWithDefaultBrowserBannerPromoAppAgent:agent];
    _mediator.settingsHandler =
        HandlerForProtocol(dispatcher, SettingsCommands);
    self.viewController.bannerPromoDelegate = _mediator;

    agent.UICurrentlySupportsPromo = [self viewControllerSupportsBannerPromo];

    _mediator.consumer = self.viewController;
  }

  [super start];
  self.started = YES;

  // The `_tabGroupIndicatorCoordinator` should be configured after the
  // `AdaptiveToolbarCoordinator` to gain access to the `PrimaryToolbarView`.
  _tabGroupIndicatorCoordinator = [[TabGroupIndicatorCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  _tabGroupIndicatorCoordinator.toolbarHeightDelegate =
      self.toolbarHeightDelegate;
  [_tabGroupIndicatorCoordinator start];
  [self.viewController
      setTabGroupIndicatorView:_tabGroupIndicatorCoordinator.view];
}

- (void)stop {
  if (!self.started) {
    return;
  }
  [super stop];
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [_tabGroupIndicatorCoordinator stop];
  _tabGroupIndicatorCoordinator = nil;

  [_mediator disconnect];

  self.started = NO;
}

#pragma mark - Public

- (id<SharingPositioner>)SharingPositioner {
  return self.viewController;
}

- (id<ToolbarAnimatee>)toolbarAnimatee {
  CHECK(self.viewController);
  return self.viewController;
}

#pragma mark - Subclassing

- (BOOL)hasTabGridButton {
  return !IsSplitToolbarMode(self.viewController);
}

- (BOOL)shouldPointArrowDownForTabGridIPH {
  return NO;
}

#pragma mark - Private

// Returns whether the banner promo is supported given the current view
// controller state.
- (BOOL)viewControllerSupportsBannerPromo {
  return !self.viewController.locationBarIsExpanded &&
         !_tabGroupIndicatorCoordinator.viewVisible;
}

// Returns the active banner promo app agent if it is available currently.
- (DefaultBrowserBannerPromoAppAgent*)activeBannerPromoAppAgent {
  if (self.isOffTheRecord) {
    return nil;
  }

  return [DefaultBrowserBannerPromoAppAgent
      agentFromApp:self.browser->GetSceneState().profileState.appState];
}

#pragma mark - GuidedTourCommands

- (void)highlightViewInStep:(GuidedTourStep)step {
  if ([self hasTabGridButton] && step == GuidedTourStep::kNTP) {
    [self.viewController IPHHighlightTabGridButton:YES];
  }
}

- (void)stepCompleted:(GuidedTourStep)step {
  if ([self hasTabGridButton] && step == GuidedTourStep::kNTP) {
    [self.viewController IPHHighlightTabGridButton:NO];
  }
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  [self.viewController triggerToolbarSlideInAnimationFromBelow:NO];
}

- (void)indicateLensOverlayVisible:(BOOL)lensOverlayVisible {
  // NO-OP
}

#pragma mark - PrimaryToolbarViewControllerDelegate

- (void)viewControllerTraitCollectionDidChange:
    (UITraitCollection*)previousTraitCollection {
  [self.viewControllerDelegate
      viewControllerTraitCollectionDidChange:previousTraitCollection];

  [self activeBannerPromoAppAgent].UICurrentlySupportsPromo =
      [self viewControllerSupportsBannerPromo];
}

- (void)close {
  [self.viewControllerDelegate close];
}

- (void)locationBarExpandedInViewController:
    (PrimaryToolbarViewController*)viewController {
  [self.viewControllerDelegate
      locationBarExpandedInViewController:viewController];

  [self activeBannerPromoAppAgent].UICurrentlySupportsPromo =
      [self viewControllerSupportsBannerPromo];
}

- (void)locationBarContractedInViewController:
    (PrimaryToolbarViewController*)viewController {
  [self.viewControllerDelegate
      locationBarContractedInViewController:viewController];

  [self activeBannerPromoAppAgent].UICurrentlySupportsPromo =
      [self viewControllerSupportsBannerPromo];
}

- (void)viewController:(PrimaryToolbarViewController*)viewController
    tabGroupIndicatorVisibilityUpdated:(BOOL)visible {
  [self activeBannerPromoAppAgent].UICurrentlySupportsPromo =
      [self viewControllerSupportsBannerPromo];
}

- (ToolbarCancelButtonStyle)styleForCancelButtonInToolbar {
  return [self.viewControllerDelegate styleForCancelButtonInToolbar];
}

@end
