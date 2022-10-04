// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WhatsNewCoordinator ()

@property(nonatomic, strong) WhatsNewMediator* mediator;

@end

@implementation WhatsNewCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  self.mediator = [[WhatsNewMediator alloc] init];
}

- (void)stop {
  self.mediator = nil;
  [super stop];
}

@end
