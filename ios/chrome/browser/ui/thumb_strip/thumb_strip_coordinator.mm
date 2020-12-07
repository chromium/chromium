// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_coordinator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the view that is revealed. The thumb strip has a height equal to a
// small grid cell + edge insets (top and bottm) from thumb strip layout.
const CGFloat kThumbStripHeight = 168.0f + 22.0f + 22.0f;
}  // namespace

@interface ThumbStripCoordinator ()

@property(nonatomic, strong) ThumbStripMediator* mediator;

@end

@implementation ThumbStripCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  CGFloat baseViewHeight = self.baseViewController.view.frame.size.height;
  self.panHandler = [[ViewRevealingVerticalPanHandler alloc]
      initWithPeekedHeight:kThumbStripHeight
       revealedCoverHeight:kBVCHeightTabGrid
            baseViewHeight:baseViewHeight];

  self.mediator = [[ThumbStripMediator alloc] init];
  if (self.regularBrowser) {
    self.mediator.regularWebStateList = self.regularBrowser->GetWebStateList();
  }
  if (self.incognitoBrowser) {
    self.mediator.incognitoWebStateList =
        self.incognitoBrowser->GetWebStateList();
  }
  self.mediator.webViewScrollViewObserver = self.panHandler;
}

- (void)stop {
  self.panHandler = nil;
  self.mediator = nil;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  _incognitoBrowser = incognitoBrowser;
  self.mediator.incognitoWebStateList =
      _incognitoBrowser ? _incognitoBrowser->GetWebStateList() : nullptr;
}

@end
