// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#import <memory>

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/orchestrator/omnibox_focus_orchestrator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_mediator.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/navigation/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarCoordinator () <PrimaryToolbarMediatorDelegate,
                                         PrimaryToolbarViewControllerDelegate> {
  // Observer that updates `toolbarViewController` for fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
  PrerenderService* _prerenderService;
}

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// Mediator for this toolbar.
@property(nonatomic, strong) PrimaryToolbarMediator* primaryToolbarMediator;
// Redefined as PrimaryToolbarViewController.
@property(nonatomic, strong) PrimaryToolbarViewController* viewController;
// The coordinator for the location bar in the toolbar.
@property(nonatomic, strong) LocationBarCoordinator* locationBarCoordinator;
// Orchestrator for the expansion animation.
@property(nonatomic, strong) OmniboxFocusOrchestrator* orchestrator;
// Whether the omnibox focusing should happen with animation.
@property(nonatomic, assign) BOOL enableAnimationsForOmniboxFocus;
// Whether the omnibox is currently focused.
@property(nonatomic, assign) BOOL locationBarFocused;

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

  self.primaryToolbarMediator = [[PrimaryToolbarMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()];
  self.primaryToolbarMediator.delegate = self;

  // LocationBarCoordinator dispatches OmniboxCommands therefore Location Bar
  // setup should be done before using OmniboxCommands handler (below).
  [self setUpLocationBar];

  self.viewController = [[PrimaryToolbarViewController alloc] init];
  self.viewController.shouldHideOmniboxOnNTP =
      !self.browser->GetBrowserState()->IsOffTheRecord();
  self.viewController.omniboxCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  self.viewController.popupMenuCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PopupMenuCommands);
  self.viewController.delegate = self;

  self.orchestrator = [[OmniboxFocusOrchestrator alloc] init];
  self.orchestrator.toolbarAnimatee = self.viewController;

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
  _prerenderService = PrerenderServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());

  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [super stop];
  self.primaryToolbarMediator.delegate = nil;
  [self.primaryToolbarMediator disconnect];
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
  self.locationBarFocused = focused;
}

- (id<ViewRevealingAnimatee>)animatee {
  return self.viewController;
}

- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler {
  self.viewController.panGestureHandler = panGestureHandler;
}

- (void)updateToolbar {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!webState)
    return;

  BOOL isPrerendered =
      (_prerenderService && _prerenderService->IsLoadingPrerender());

  // Please note, this notion of isLoading is slightly different from WebState's
  // IsLoading().
  BOOL isToolbarLoading =
      webState->IsLoading() &&
      !webState->GetLastCommittedURL().SchemeIs(kChromeUIScheme);

  if (isPrerendered && isToolbarLoading)
    [self showPrerenderingAnimation];

  id<FindInPageCommands> findInPageCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), FindInPageCommands);
  [findInPageCommandsHandler showFindUIIfActive];

  id<TextZoomCommands> textZoomCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TextZoomCommands);
  [textZoomCommandsHandler showTextZoomUIIfActive];

  // There are times when the NTP can be hidden but before the visibleURL
  // changes.  This can leave the BVC in a blank state where only the bottom
  // toolbar is visible. Instead, if possible, use the NewTabPageTabHelper
  // IsActive() value rather than checking -IsVisibleURLNewTabPage.
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  BOOL isNTP = NTPHelper && NTPHelper->IsActive();
  BOOL isOffTheRecord = self.browser->GetBrowserState()->IsOffTheRecord();
  BOOL canShowTabStrip = IsRegularXRegularSizeClass(self.viewController);

  // Hide the toolbar when displaying content suggestions without the tab
  // strip, without the focused omnibox, and for UI Refresh, only when in
  // split toolbar mode.
  BOOL hideToolbar = isNTP && !isOffTheRecord &&
                     ![self isOmniboxFirstResponder] &&
                     ![self showingOmniboxPopup] && !canShowTabStrip &&
                     IsSplitToolbarMode(self.viewController);

  [self.viewController.view setHidden:hideToolbar];
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

- (void)close {
  if (self.locationBarFocused) {
    id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationCommandsHandler dismissModalDialogs];
  }
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

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  [self.viewController triggerToolbarSlideInAnimationFromBelow:NO];
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
