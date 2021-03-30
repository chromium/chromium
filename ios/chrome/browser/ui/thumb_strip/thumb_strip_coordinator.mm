// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_coordinator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/thumb_strip_commands.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the view that is revealed. The thumb strip has a height equal to a
// small grid cell + edge insets (top and bottom) from thumb strip layout.
const CGFloat kThumbStripHeight =
    kGridCellSizeSmall.height +
    2 * kGridLayoutLineSpacingCompactCompactLimitedWidth;
}  // namespace

@interface ThumbStripCoordinator () <ThumbStripCommands,
                                     ThumbStripNavigationConsumer>

@property(nonatomic, strong) ThumbStripMediator* mediator;

// The initial state for the pan handler.
@property(nonatomic, assign) ViewRevealState initialState;

@property(nonatomic, assign) BOOL started;

@end

@implementation ThumbStripCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              initialState:(ViewRevealState)initialState {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _initialState = initialState;
  }
  return self;
}

- (void)start {
  self.started = YES;
  CGFloat baseViewHeight = self.baseViewController.view.frame.size.height;
  self.panHandler = [[ViewRevealingVerticalPanHandler alloc]
      initWithPeekedHeight:kThumbStripHeight
       revealedCoverHeight:kBVCHeightTabGrid
            baseViewHeight:baseViewHeight
              initialState:self.initialState];

  self.mediator = [[ThumbStripMediator alloc] init];
  self.mediator.consumer = self;
  if (self.regularBrowser) {
    [self.regularBrowser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ThumbStripCommands)];
    self.mediator.regularWebStateList = self.regularBrowser->GetWebStateList();
  }
  if (self.incognitoBrowser) {
    [self.incognitoBrowser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ThumbStripCommands)];
    self.mediator.incognitoWebStateList =
        self.incognitoBrowser->GetWebStateList();
  }
  self.mediator.webViewScrollViewObserver = self.panHandler;
}

- (void)stop {
  self.started = NO;
  self.mediator.regularWebStateList = nil;
  self.mediator.incognitoWebStateList = nil;
  self.mediator.webViewScrollViewObserver = nil;
  self.panHandler = nil;
  self.mediator = nil;

  if (self.regularBrowser) {
    [self.regularBrowser->GetCommandDispatcher()
        stopDispatchingForProtocol:@protocol(ThumbStripCommands)];
  }
  self.regularBrowser = nullptr;
  // Dispatching is stopped in -setIncognitoBrowser.
  self.incognitoBrowser = nullptr;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  if (_incognitoBrowser) {
    [_incognitoBrowser->GetCommandDispatcher()
        stopDispatchingForProtocol:@protocol(ThumbStripCommands)];
  }
  _incognitoBrowser = incognitoBrowser;
  if (!self.started) {
    return;
  }
  if (_incognitoBrowser) {
    [_incognitoBrowser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ThumbStripCommands)];
  }
  self.mediator.incognitoWebStateList =
      _incognitoBrowser ? _incognitoBrowser->GetWebStateList() : nullptr;
}

#pragma mark - ThumbStripNavigationConsumer

- (void)navigationDidStart {
  [self closeThumbStrip];
}

#pragma mark - ThumbStripCommands

- (void)closeThumbStrip {
  [self.panHandler setNextState:ViewRevealState::Hidden animated:YES];
}

@end
