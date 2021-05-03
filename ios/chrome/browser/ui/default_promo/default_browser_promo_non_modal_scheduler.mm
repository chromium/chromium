// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_scheduler.h"

#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Default time interval to wait to show the promo after loading a webpage.
// This should allow any initial overlays to be presented first.
const NSTimeInterval kShowPromoWebpageLoadWaitTime = 3;

}  // namespace

@interface DefaultBrowserPromoNonModalScheduler () <WebStateListObserving,
                                                    CRWWebStateObserver,
                                                    OverlayPresenterObserving> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;
  std::unique_ptr<OverlayPresenterObserverBridge> _overlayObserver;
}

// Timer for showing the promo after page load.
@property(nonatomic, strong) NSTimer* showPromoTimer;

// Timer for dismissing the promo after it is shown.
@property(nonatomic, strong) NSTimer* dismissPromoTimer;

// Webstate that the omnibox paste triggring event occured in.
@property(nonatomic, assign) web::WebState* omniboxPasteWebState;

// The handler used to respond to the promo show/hide commands.
@property(nonatomic, readonly) id<DefaultBrowserPromoNonModalCommands> handler;

// Whether or not the promo is currently showing.
@property(nonatomic, assign) BOOL promoIsShowing;

@end

@implementation DefaultBrowserPromoNonModalScheduler

- (instancetype)init {
  if (self = [super init]) {
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _overlayObserver = std::make_unique<OverlayPresenterObserverBridge>(self);
  }
  return self;
}

- (void)logUserPastedInOmnibox {
  // This assumes that the currently active webstate is the one that the paste
  // occured in.
  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  // There should always be an active web state when pasting in the omnibox.
  if (!activeWebState) {
    return;
  }
  // Store the pasted web state, so when that web state's page load finishes,
  // the promo can be shown.
  self.omniboxPasteWebState = activeWebState;
}

- (void)logUserFinishedActivityFlow {
}

- (void)logPromoWasDismissed {
  self.promoIsShowing = NO;
}

- (void)logTabGridEntered {
  [self dismissPromoAnimated:YES];
}

- (void)logPopupMenuEntered {
  [self dismissPromoAnimated:YES];
}

- (void)logUserPerformedPromoAction {
  if (NonModalPromosInstructionsEnabled()) {
    id<ApplicationSettingsCommands> handler =
        HandlerForProtocol(self.dispatcher, ApplicationSettingsCommands);
    [handler showDefaultBrowserSettingsFromViewController:nil];
  } else {
    NSURL* settingsURL =
        [NSURL URLWithString:UIApplicationOpenSettingsURLString];
    [[UIApplication sharedApplication] openURL:settingsURL
                                       options:{}
                             completionHandler:nil];
  }
}

- (void)dismissPromoAnimated:(BOOL)animated {
  [self cancelDismissPromoTimer];
  [self.handler dismissDefaultBrowserNonModalPromoAnimated:animated];
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _forwarder = nullptr;
  }
  _webStateList = webStateList;
  if (_webStateList) {
    _webStateList->AddObserver(_webStateListObserver.get());
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        _webStateList, _webStateObserver.get());
  }
}

- (void)setOverlayPresenter:(OverlayPresenter*)overlayPresenter {
  if (_overlayPresenter) {
    _overlayPresenter->RemoveObserver(_overlayObserver.get());
  }

  _overlayPresenter = overlayPresenter;

  if (_overlayPresenter) {
    _overlayPresenter->AddObserver(_overlayObserver.get());
  }
}

- (id<DefaultBrowserPromoNonModalCommands>)handler {
  return HandlerForProtocol(self.dispatcher,
                            DefaultBrowserPromoNonModalCommands);
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  [self cancelShowPromoTimer];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  if (success && webState == self.omniboxPasteWebState) {
    self.omniboxPasteWebState = nil;
    [self startShowPromoTimer];
  }
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  [self cancelShowPromoTimer];
  [self dismissPromoAnimated:YES];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelBackground) {
    [self.handler dismissDefaultBrowserNonModalPromoAnimated:NO];
  }
}

#pragma mark - Timer Management

- (void)startShowPromoTimer {
  if (self.promoIsShowing || self.showPromoTimer) {
    return;
  }
  self.showPromoTimer =
      [NSTimer scheduledTimerWithTimeInterval:kShowPromoWebpageLoadWaitTime
                                       target:self
                                     selector:@selector(showPromoTimerFinished)
                                     userInfo:nil
                                      repeats:NO];
}

- (void)cancelShowPromoTimer {
  [self.showPromoTimer invalidate];
  self.showPromoTimer = nil;
}

- (void)showPromoTimerFinished {
  if (self.promoIsShowing) {
    return;
  }
  self.showPromoTimer = nil;
  [self.handler showDefaultBrowserNonModalPromo];
  self.promoIsShowing = YES;
  [self startDismissPromoTimer];
}

- (void)startDismissPromoTimer {
  if (self.dismissPromoTimer) {
    return;
  }
  self.dismissPromoTimer = [NSTimer
      scheduledTimerWithTimeInterval:NonModalPromosTimeout()
                              target:self
                            selector:@selector(dismissPromoTimerFinished)
                            userInfo:nil
                             repeats:NO];
}

- (void)cancelDismissPromoTimer {
  [self.dismissPromoTimer invalidate];
  self.dismissPromoTimer = nil;
}

- (void)dismissPromoTimerFinished {
  self.dismissPromoTimer = nil;
  [self.handler dismissDefaultBrowserNonModalPromoAnimated:YES];
}

@end
