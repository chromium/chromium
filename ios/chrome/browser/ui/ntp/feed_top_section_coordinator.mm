// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section_coordinator.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section_mediator.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FeedTopSectionCoordinator ()

@property(nonatomic, strong) FeedTopSectionMediator* feedTopSectionMediator;

@end

@implementation FeedTopSectionCoordinator

@synthesize viewController = _viewController;

- (void)start {
  FeedTopSectionViewController* feedTopSectionViewController =
      [[FeedTopSectionViewController alloc] init];
  _viewController = feedTopSectionViewController;
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  FeedTopSectionMediator* feedTopSectionMediator =
      [[FeedTopSectionMediator alloc] initWithBrowserState:browserState];
  self.feedTopSectionMediator = feedTopSectionMediator;
  [feedTopSectionMediator setUp];
}

- (void)stop {
  _viewController = nil;
  self.feedTopSectionMediator = nil;
}

@end
