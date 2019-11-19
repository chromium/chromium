// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/fullscreen/legacy_toolbar_ui_updater.h"

#include <memory>

#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacyToolbarUIUpdater ()<CRWWebStateObserver,
                                     WebStateListObserving> {
  // The bridge for WebStateList observation.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // The bridge for WebState observation.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}
// The owner passed on initialization.
@property(nonatomic, readonly, strong) id<ToolbarHeightProviderForFullscreen>
    owner;
// The WebStateList whose navigations are driving this updater.
@property(nonatomic, readonly) WebStateList* webStateList;
// The active WebState in |webStateList|.
@property(nonatomic, assign) web::WebState* webState;

// Updates |state| using |owner|.
- (void)updateState;

@end

@implementation LegacyToolbarUIUpdater
@synthesize toolbarUI = _toolbarUI;
@synthesize owner = _owner;
@synthesize webStateList = _webStateList;
@synthesize webState = _webState;

- (nullable instancetype)
initWithToolbarUI:(nonnull ToolbarUIState*)toolbarUI
     toolbarOwner:(nonnull id<ToolbarHeightProviderForFullscreen>)owner
     webStateList:(nonnull WebStateList*)webStateList {
  if (self = [super init]) {
    _toolbarUI = toolbarUI;
    DCHECK(_toolbarUI);
    _owner = owner;
    DCHECK(_owner);
    _webStateList = webStateList;
    DCHECK(_webStateList);
  }
  return self;
}

#pragma mark Accessors

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState)
    return;
  if (_webState) {
    DCHECK(_webStateObserver);
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver = nullptr;
  }
  _webState = webState;
  if (_webState) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserver.get());
    [self updateState];
  }
}

#pragma mark Public

- (void)startUpdating {
  DCHECK(!_webStateListObserver);
  DCHECK(!_webStateObserver);
  _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  self.webStateList->AddObserver(_webStateListObserver.get());
  self.webState = self.webStateList->GetActiveWebState();
  [self updateState];
}

- (void)stopUpdating {
  DCHECK(_webStateListObserver);
  DCHECK(!self.webState || _webStateObserver);
  self.webStateList->RemoveObserver(_webStateListObserver.get());
  self.webState = nullptr;
}

- (void)updateState {
  self.toolbarUI.collapsedHeight = [self.owner collapsedTopToolbarHeight];
  self.toolbarUI.expandedHeight = [self.owner expandedTopToolbarHeight];
  self.toolbarUI.bottomToolbarHeight = [self.owner bottomToolbarHeight];
}

#pragma mark CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  // For user-initiated loads, the toolbar is updated when the navigation is
  // started.
  if (!navigation->IsRendererInitiated())
    [self updateState];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  self.webState = nullptr;
}

#pragma mark WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK_EQ(self.webStateList, webStateList);
  if (newWebState == webStateList->GetActiveWebState())
    self.webState = newWebState;
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  DCHECK_EQ(self.webStateList, webStateList);
  self.webState = newWebState;
}

@end
