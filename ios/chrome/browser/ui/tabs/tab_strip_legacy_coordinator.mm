// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tabs/tab_strip_legacy_coordinator.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_presentation.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@protocol TabStripContaining;

@interface TabStripLegacyCoordinator ()
@property(nonatomic, assign) BOOL started;
@property(nonatomic, strong) TabStripController* tabStripController;
@end

@implementation TabStripLegacyCoordinator
@synthesize presentationProvider = _presentationProvider;
@synthesize started = _started;
@synthesize tabStripController = _tabStripController;
@synthesize animationWaitDuration = _animationWaitDuration;
@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (UIView<TabStripContaining>*)view {
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

- (void)tabStripSizeDidChange {
  [self.tabStripController tabStripSizeDidChange];
}

- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler {
  self.tabStripController.panGestureHandler = panGestureHandler;
}

- (id<ViewRevealingAnimatee>)animatee {
  return self.tabStripController.animatee;
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  DCHECK(self.presentationProvider);
  TabStripStyle style =
      self.browser->GetBrowserState()->IsOffTheRecord() ? INCOGNITO : NORMAL;
  self.tabStripController = [[TabStripController alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           style:style];
  self.tabStripController.presentationProvider = self.presentationProvider;
  self.tabStripController.animationWaitDuration = self.animationWaitDuration;
  [self.presentationProvider showTabStripView:[self.tabStripController view]];
  self.started = YES;
}

- (void)stop {
  self.started = NO;
  [self.tabStripController disconnect];
  self.tabStripController = nil;
  self.presentationProvider = nil;
}

@end
