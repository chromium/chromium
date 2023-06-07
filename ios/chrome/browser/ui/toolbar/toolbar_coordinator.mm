// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"
#import "ios/chrome/browser/ui/orchestrator/omnibox_focus_orchestrator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinatee.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinator () <PrimaryToolbarViewControllerDelegate,
                                  ToolbarCommands>

/// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
/// Coordinator for the location bar containing the omnibox.
@property(nonatomic, strong) LocationBarCoordinator* locationBarCoordinator;
/// Coordinator for the primary toolbar at the top of the screen.
@property(nonatomic, strong)
    PrimaryToolbarCoordinator* primaryToolbarCoordinator;
/// Coordinator for the secondary toolbar at the bottom of the screen.
@property(nonatomic, strong)
    SecondaryToolbarCoordinator* secondaryToolbarCoordinator;

/// Orchestrator for the omnibox focus animation.
@property(nonatomic, strong) OmniboxFocusOrchestrator* orchestrator;
/// Whether the omnibox is currently focused.
@property(nonatomic, assign) BOOL locationBarFocused;

@end

@implementation ToolbarCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  CHECK(browser);
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    // Initialize both coordinators here as they might be referenced before
    // `start`.
    _primaryToolbarCoordinator =
        [[PrimaryToolbarCoordinator alloc] initWithBrowser:browser];
    _secondaryToolbarCoordinator =
        [[SecondaryToolbarCoordinator alloc] initWithBrowser:browser];
  }
  return self;
}

- (void)start {
  if (self.started) {
    return;
  }

  Browser* browser = self.browser;
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(ToolbarCommands)];

  self.locationBarCoordinator =
      [[LocationBarCoordinator alloc] initWithBrowser:browser];
  self.locationBarCoordinator.delegate = self.omniboxFocusDelegate;
  self.locationBarCoordinator.popupPresenterDelegate =
      self.popupPresenterDelegate;
  [self.locationBarCoordinator start];

  self.primaryToolbarCoordinator.locationBarCoordinator =
      self.locationBarCoordinator;
  self.primaryToolbarCoordinator.viewControllerDelegate = self;
  [self.primaryToolbarCoordinator start];
  [self.secondaryToolbarCoordinator start];

  self.orchestrator = [[OmniboxFocusOrchestrator alloc] init];
  self.orchestrator.toolbarAnimatee =
      self.primaryToolbarCoordinator.toolbarAnimatee;
  self.orchestrator.locationBarAnimatee =
      [self.locationBarCoordinator locationBarAnimatee];
  self.orchestrator.editViewAnimatee =
      [self.locationBarCoordinator editViewAnimatee];

  [self updateToolbarsLayout];

  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started) {
    return;
  }
  [super stop];

  [self.locationBarCoordinator stop];
  self.locationBarCoordinator = nil;
  [self.primaryToolbarCoordinator stop];
  [self.secondaryToolbarCoordinator stop];

  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];

  self.started = NO;
}

#pragma mark - Public

- (UIViewController*)primaryToolbarViewController {
  return self.primaryToolbarCoordinator.viewController;
}

- (UIViewController*)secondaryToolbarViewController {
  return self.secondaryToolbarCoordinator.viewController;
}

- (id<SharingPositioner>)sharingPositioner {
  return self.primaryToolbarCoordinator.SharingPositioner;
}

- (void)updateToolbar {
  [self.primaryToolbarCoordinator updateToolbar];
}

- (BOOL)isLoadingPrerenderer {
  return self.primaryToolbarCoordinator.isLoadingPrerenderer;
}

#pragma mark ViewRevealing

- (id<ViewRevealingAnimatee>)viewRevealingAnimatee {
  CHECK(self.primaryToolbarCoordinator.animatee);
  return self.primaryToolbarCoordinator.animatee;
}

- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler {
  [self.primaryToolbarCoordinator setPanGestureHandler:panGestureHandler];
}

#pragma mark SnapshotProviding

- (id<SideSwipeToolbarSnapshotProviding>)primaryToolbarSnapshotProvider {
  return self.primaryToolbarCoordinator;
}

- (id<SideSwipeToolbarSnapshotProviding>)secondaryToolbarSnapshotProvider {
  return self.secondaryToolbarCoordinator;
}

#pragma mark Omnibox and LocationBar

- (void)transitionToLocationBarFocusedState:(BOOL)focused {
  if (self.traitEnvironment.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassUnspecified) {
    return;
  }

  [self.orchestrator
      transitionToStateOmniboxFocused:focused
                      toolbarExpanded:focused && !IsRegularXRegularSizeClass(
                                                     self.traitEnvironment)
                             animated:self.primaryToolbarCoordinator
                                          .enableAnimationsForOmniboxFocus];
  self.locationBarFocused = focused;
}

- (BOOL)isOmniboxFirstResponder {
  return [self.locationBarCoordinator isOmniboxFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  return [self.locationBarCoordinator showingOmniboxPopup];
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  for (id<NewTabPageControllerDelegate> coordinator in self.coordinators) {
    [coordinator setScrollProgressForTabletOmnibox:progress];
  }
}

- (UIResponder<UITextInput>*)fakeboxScribbleForwardingTarget {
  for (id<NewTabPageControllerDelegate> coordinator in self.coordinators) {
    if (coordinator.fakeboxScribbleForwardingTarget) {
      return coordinator.fakeboxScribbleForwardingTarget;
    }
  }
  return nil;
}

#pragma mark - PopupMenuUIUpdating

- (void)updateUIForOverflowMenuIPHDisplayed {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater updateUIForOverflowMenuIPHDisplayed];
  }
}

- (void)updateUIForIPHDismissed {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater updateUIForIPHDismissed];
  }
}

#pragma mark - PrimaryToolbarViewControllerDelegate

- (void)viewControllerTraitCollectionDidChange:
    (UITraitCollection*)previousTraitCollection {
  [self updateToolbarsLayout];
}

- (void)close {
  if (self.locationBarFocused) {
    id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationCommandsHandler dismissModalDialogs];
  }
}

#pragma mark - SideSwipeToolbarInteracting

- (BOOL)isInsideToolbar:(CGPoint)point {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    // The toolbar frame is inset by -1 because CGRectContainsPoint does
    // include points on the max X and Y edges, which will happen frequently
    // with edge swipes from the right side.
    CGRect toolbarFrame =
        CGRectInset([coordinator viewController].view.bounds, -1, -1);
    CGPoint pointInToolbarCoordinates =
        [[coordinator viewController].view convertPoint:point fromView:nil];
    if (CGRectContainsPoint(toolbarFrame, pointInToolbarCoordinates)) {
      return YES;
    }
  }
  return NO;
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  for (id<ToolbarCommands> coordinator in self.coordinators) {
    [coordinator triggerToolbarSlideInAnimation];
  }
}

#pragma mark - Private

/// Returns primary and secondary coordinator in a array. Helper to call method
/// on both coordinators.
- (NSArray<id<ToolbarCoordinatee>>*)coordinators {
  return @[ self.primaryToolbarCoordinator, self.secondaryToolbarCoordinator ];
}

/// Returns the trait environment of the toolbars.
- (id<UITraitEnvironment>)traitEnvironment {
  return self.primaryToolbarViewController;
}

/// Updates toolbars layout whith current omnibox focus state.
- (void)updateToolbarsLayout {
  BOOL omniboxFocused =
      self.isOmniboxFirstResponder || self.showingOmniboxPopup;
  [self.orchestrator
      transitionToStateOmniboxFocused:omniboxFocused
                      toolbarExpanded:omniboxFocused &&
                                      !IsRegularXRegularSizeClass(
                                          self.traitEnvironment)
                             animated:NO];
}

@end
