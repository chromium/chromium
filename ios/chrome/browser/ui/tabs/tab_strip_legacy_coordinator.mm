// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tabs/tab_strip_legacy_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_presentation.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabStripLegacyCoordinator ()
@property(nonatomic, assign) BOOL started;
@property(nonatomic, strong) TabStripController* tabStripController;
@end

@implementation TabStripLegacyCoordinator
@synthesize browserState = _browserState;
@synthesize dispatcher = _dispatcher;
@synthesize longPressDelegate = _longPressDelegate;
@synthesize tabModel = _tabModel;
@synthesize presentationProvider = _presentationProvider;
@synthesize started = _started;
@synthesize tabStripController = _tabStripController;
@synthesize animationWaitDuration = _animationWaitDuration;

- (void)setDispatcher:(id<BrowserCommands, ApplicationCommands>)dispatcher {
  DCHECK(!self.started);
  _dispatcher = dispatcher;
}

- (void)setTabModel:(TabModel*)tabModel {
  DCHECK(!self.started);
  _tabModel = tabModel;
}

- (void)setLongPressDelegate:(id<PopupMenuLongPressDelegate>)longPressDelegate {
  _longPressDelegate = longPressDelegate;
  self.tabStripController.longPressDelegate = longPressDelegate;
}

- (UIView*)view {
  DCHECK(self.started);
  return [self.tabStripController view];
}

- (void)setPresentationProvider:(id<TabStripPresentation>)presentationProvider {
  DCHECK(!self.started);
  _presentationProvider = presentationProvider;
}

- (void)setAnimationWaitDuration:(NSTimeInterval)animationWaitDuration {
  DCHECK(!self.started);
  _animationWaitDuration = animationWaitDuration;
}

- (void)setHighlightsSelectedTab:(BOOL)highlightsSelectedTab {
  DCHECK(self.started);
  self.tabStripController.highlightsSelectedTab = highlightsSelectedTab;
}

- (void)hideTabStrip:(BOOL)hidden {
  [self.tabStripController hideTabStrip:hidden];
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browserState);
  DCHECK(self.tabModel);
  DCHECK(self.dispatcher);
  DCHECK(self.presentationProvider);
  TabStripStyle style =
      self.browserState->IsOffTheRecord() ? INCOGNITO : NORMAL;
  self.tabStripController =
      [[TabStripController alloc] initWithTabModel:self.tabModel
                                             style:style
                                        dispatcher:self.dispatcher];
  self.tabStripController.presentationProvider = self.presentationProvider;
  self.tabStripController.animationWaitDuration = self.animationWaitDuration;
  self.tabStripController.longPressDelegate = self.longPressDelegate;
  [self.presentationProvider showTabStripView:[self.tabStripController view]];
  self.started = YES;
}

- (void)stop {
  self.started = NO;
  [self.tabStripController disconnect];
  self.tabStripController = nil;
  self.dispatcher = nil;
  self.tabModel = nil;
  self.presentationProvider = nil;
}

@end
