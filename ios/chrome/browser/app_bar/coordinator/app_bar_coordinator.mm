// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_coordinator.h"

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@implementation AppBarCoordinator {
  AppBarViewController* _viewController;
  AppBarMediator* _mediator;
  raw_ptr<Browser> _incognitoBrowser;
  raw_ptr<Browser> _regularBrowser;
}

- (instancetype)initWithRegularBrowser:(Browser*)regularBrowser
                      incognitoBrowser:(Browser*)incognitoBrowser {
  self = [super init];
  if (self) {
    _incognitoBrowser = incognitoBrowser;
    _regularBrowser = regularBrowser;
  }
  return self;
}

- (void)start {
  _viewController = [[AppBarViewController alloc] init];
  _mediator = [[AppBarMediator alloc] init];
  _mediator.consumer = _viewController;
  _mediator.webStateList = _regularBrowser->GetWebStateList();
  // TODO(crbug.com/472279443): Add incognito browser support.
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
  _regularBrowser = nullptr;
  _incognitoBrowser = nullptr;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return _viewController;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  _incognitoBrowser = incognitoBrowser;
}

@end
