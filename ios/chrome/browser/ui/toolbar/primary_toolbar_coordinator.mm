// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/orchestrator/omnibox_focus_orchestrator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/navigation/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarCoordinator () <PrimaryToolbarViewControllerDelegate> {
  // Observer that updates |toolbarViewController| for fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
}

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// Redefined as PrimaryToolbarViewController.
@property(nonatomic, strong) PrimaryToolbarViewController* viewController;
// The coordinator for the location bar in the toolbar.
@property(nonatomic, strong) LocationBarCoordinator* locationBarCoordinator;
// Orchestrator for the expansion animation.
@property(nonatomic, strong) OmniboxFocusOrchestrator* orchestrator;
// Whether the omnibox focusing should happen with animation.
@property(nonatomic, assign) BOOL enableAnimationsForOmniboxFocus;

@end

@implementation PrimaryToolbarCoordinator

@dynamic viewController;
@synthesize popupPresenterDelegate = _popupPresenterDelegate;
@synthesize delegate = _delegate;

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  if (self.started)
    return;

  self.enableAnimationsForOmniboxFocus = YES;

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(FakeboxFocuser)];

  self.viewController = [[PrimaryToolbarViewController alloc] init];
  self.viewController.shouldHideOmniboxOnNTP =
      !self.browser->GetBrowserState()->IsOffTheRecord();
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.viewController.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands, OmniboxCommands>>(
          self.browser->GetCommandDispatcher());
  self.viewController.delegate = self;

  self.orchestrator = [[OmniboxFocusOrchestrator alloc] init];
  self.orchestrator.toolbarAnimatee = self.viewController;

  [self setUpLocationBar];

  // Button factory requires that the omnibox commands are set up, which is
  // done by the location bar.
  self.viewController.buttonFactory = [self buttonFactoryWithType:PRIMARY];

  self.viewController.locationBarViewController =
      self.locationBarCoordinator.locationBarViewController;
  self.orchestrator.locationBarAnimatee =
      [self.locationBarCoordinator locationBarAnimatee];

  self.orchestrator.editViewAnimatee =
      [self.locationBarCoordinator editViewAnimatee];

    _fullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        FullscreenController::FromBrowser(self.browser), self.viewController);

  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [super stop];
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [self.locationBarCoordinator stop];
  _fullscreenUIUpdater = nullptr;
  self.started = NO;
}

#pragma mark - Public

- (id<ActivityServicePositioner>)activityServicePositioner {
  return self.viewController;
}

- (void)showPrerenderingAnimation {
  [self.viewController showPrerenderingAnimation];
}

- (BOOL)isOmniboxFirstResponder {
  return [self.locationBarCoordinator isOmniboxFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  return [self.locationBarCoordinator showingOmniboxPopup];
}

- (void)transitionToLocationBarFocusedState:(BOOL)focused {
  if (self.viewController.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassUnspecified) {
    return;
  }

  [self.orchestrator
      transitionToStateOmniboxFocused:focused
                      toolbarExpanded:focused && !IsRegularXRegularSizeClass(
                                                     self.viewController)
                             animated:self.enableAnimationsForOmniboxFocus];
}

- (id<ViewRevealingAnimatee>)animatee {
  return self.viewController;
}

- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler {
  self.viewController.panGestureHandler = panGestureHandler;
}

#pragma mark - PrimaryToolbarViewControllerDelegate

- (void)viewControllerTraitCollectionDidChange:
    (UITraitCollection*)previousTraitCollection {
  BOOL omniboxFocused = self.isOmniboxFirstResponder ||
                        [self.locationBarCoordinator showingOmniboxPopup];
  [self.orchestrator
      transitionToStateOmniboxFocused:omniboxFocused
                      toolbarExpanded:omniboxFocused &&
                                      !IsRegularXRegularSizeClass(
                                          self.viewController)
                             animated:NO];
}

- (void)exitFullscreen {
    FullscreenController::FromBrowser(self.browser)->ExitFullscreen();
}

#pragma mark - NewTabPageControllerDelegate

- (UIResponder<UITextInput>*)fakeboxScribbleForwardingTarget {
  return self.locationBarCoordinator.omniboxScribbleForwardingTarget;
}

#pragma mark - FakeboxFocuser

- (void)focusOmniboxNoAnimation {
  self.enableAnimationsForOmniboxFocus = NO;
  [self fakeboxFocused];
  self.enableAnimationsForOmniboxFocus = YES;
  // If the pasteboard is containing a URL, the omnibox popup suggestions are
  // displayed as soon as the omnibox is focused.
  // If the fake omnibox animation is triggered at the same time, it is possible
  // to see the NTP going up where the real omnibox should be displayed.
  if ([self.locationBarCoordinator omniboxPopupHasAutocompleteResults])
    [self onFakeboxAnimationComplete];
}

- (void)fakeboxFocused {
  [self.locationBarCoordinator focusOmniboxFromFakebox];
}

- (void)onFakeboxBlur {
  // Hide the toolbar if the NTP is currently displayed.
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (webState && IsVisibleURLNewTabPage(webState)) {
    self.viewController.view.hidden = IsSplitToolbarMode(self.viewController);
  }
}

- (void)onFakeboxAnimationComplete {
  self.viewController.view.hidden = NO;
}

#pragma mark - Protected override

- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState {
  [super updateToolbarForSideSwipeSnapshot:webState];

  BOOL isNTP = IsVisibleURLNewTabPage(webState);

  // Don't do anything for a live non-ntp tab.
  if (webState == self.browser->GetWebStateList()->GetActiveWebState() &&
      !isNTP) {
    [self.locationBarCoordinator.locationBarViewController.view setHidden:NO];
  } else {
    self.viewController.view.hidden = NO;
    [self.locationBarCoordinator.locationBarViewController.view setHidden:YES];
  }
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [super resetToolbarAfterSideSwipeSnapshot];
  [self.locationBarCoordinator.locationBarViewController.view setHidden:NO];
}

#pragma mark - Private

// Sets the location bar up.
- (void)setUpLocationBar {
  self.locationBarCoordinator =
      [[LocationBarCoordinator alloc] initWithBaseViewController:nil
                                                         browser:self.browser];
  self.locationBarCoordinator.delegate = self.delegate;
  self.locationBarCoordinator.popupPresenterDelegate =
      self.popupPresenterDelegate;
  [self.locationBarCoordinator start];
}

@end
