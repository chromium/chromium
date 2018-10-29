// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_positioner.h"
#import "ios/chrome/browser/ui/orchestrator/omnibox_focus_orchestrator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarCoordinator ()<OmniboxPopupPositioner,
                                        PrimaryToolbarViewControllerDelegate> {
  // Observer that updates |toolbarViewController| for fullscreen events.
  std::unique_ptr<FullscreenControllerObserver> _fullscreenObserver;
}

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// Redefined as PrimaryToolbarViewController.
@property(nonatomic, strong) PrimaryToolbarViewController* viewController;
// The coordinator for the location bar in the toolbar.
@property(nonatomic, strong) LocationBarCoordinator* locationBarCoordinator;
// Orchestrator for the expansion animation.
@property(nonatomic, strong) OmniboxFocusOrchestrator* orchestrator;

@end

@implementation PrimaryToolbarCoordinator

@dynamic viewController;
@synthesize started = _started;
@synthesize commandDispatcher = _commandDispatcher;
@synthesize delegate = _delegate;
@synthesize locationBarCoordinator = _locationBarCoordinator;
@synthesize orchestrator = _orchestrator;
@synthesize URLLoader = _URLLoader;

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.commandDispatcher);
  if (self.started)
    return;

  [self.commandDispatcher startDispatchingToTarget:self
                                       forProtocol:@protocol(FakeboxFocuser)];

  self.viewController = [[PrimaryToolbarViewController alloc] init];
  self.viewController.buttonFactory = [self buttonFactoryWithType:PRIMARY];
  self.viewController.dispatcher = self.dispatcher;
  self.viewController.delegate = self;

  self.orchestrator = [[OmniboxFocusOrchestrator alloc] init];
  self.orchestrator.toolbarAnimatee = self.viewController;

  [self setUpLocationBar];
  self.viewController.locationBarView = self.locationBarCoordinator.view;
  self.orchestrator.locationBarAnimatee =
      [self.locationBarCoordinator locationBarAnimatee];

  self.orchestrator.editViewAnimatee =
      [self.locationBarCoordinator editViewAnimatee];

  _fullscreenObserver =
      std::make_unique<FullscreenUIUpdater>(self.viewController);
  FullscreenControllerFactory::GetInstance()
      ->GetForBrowserState(self.browserState)
      ->AddObserver(_fullscreenObserver.get());

  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [super stop];
  [self.commandDispatcher stopDispatchingToTarget:self];
  [self.locationBarCoordinator stop];
  FullscreenControllerFactory::GetInstance()
      ->GetForBrowserState(self.browserState)
      ->RemoveObserver(_fullscreenObserver.get());
  _fullscreenObserver = nullptr;
  self.started = NO;
}

#pragma mark - PrimaryToolbarCoordinator

- (id<ActivityServicePositioner>)activityServicePositioner {
  return self.viewController;
}

- (id<OmniboxFocuser>)omniboxFocuser {
  return self.locationBarCoordinator;
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
                             animated:YES];
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
  FullscreenControllerFactory::GetInstance()
      ->GetForBrowserState(self.browserState)
      ->ExitFullscreen();
}

#pragma mark - FakeboxFocuser

- (void)fakeboxFocused {
  [self.locationBarCoordinator focusOmniboxFromFakebox];
}

- (void)onFakeboxBlur {
  // Hide the toolbar if the NTP is currently displayed.
  web::WebState* webState = self.webStateList->GetActiveWebState();
  if (webState && IsVisibleUrlNewTabPage(webState)) {
    self.viewController.view.hidden = IsSplitToolbarMode();
  }
}

- (void)onFakeboxAnimationComplete {
  self.viewController.view.hidden = NO;
}

// TODO(crbug.com/786940): This protocol should move to the ViewController
// owning the Toolbar. This can wait until the omnibox and toolbar refactoring
// is more advanced.
#pragma mark OmniboxPopupPositioner methods.

- (UIView*)popupParentView {
  return self.viewController.view.superview;
}

- (UIViewController*)popupParentViewController {
  return self.viewController.parentViewController;
}

#pragma mark - Protected override

- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState {
  [super updateToolbarForSideSwipeSnapshot:webState];

  BOOL isNTP = IsVisibleUrlNewTabPage(webState);

  // Don't do anything for a live non-ntp tab.
  if (webState == self.webStateList->GetActiveWebState() && !isNTP) {
    [self.locationBarCoordinator.view setHidden:NO];
  } else {
    self.viewController.view.hidden = NO;
    [self.locationBarCoordinator.view setHidden:YES];
  }
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [super resetToolbarAfterSideSwipeSnapshot];
  [self.locationBarCoordinator.view setHidden:NO];
}

#pragma mark - Private

// Sets the location bar up.
- (void)setUpLocationBar {
  self.locationBarCoordinator = [[LocationBarCoordinator alloc] init];

  self.locationBarCoordinator.browserState = self.browserState;
  self.locationBarCoordinator.dispatcher =
      base::mac::ObjCCastStrict<CommandDispatcher>(self.dispatcher);
  self.locationBarCoordinator.commandDispatcher = self.commandDispatcher;
  self.locationBarCoordinator.URLLoader = self.URLLoader;
  self.locationBarCoordinator.delegate = self.delegate;
  self.locationBarCoordinator.webStateList = self.webStateList;
  self.locationBarCoordinator.popupPositioner = self;
  [self.locationBarCoordinator start];
}

@end
