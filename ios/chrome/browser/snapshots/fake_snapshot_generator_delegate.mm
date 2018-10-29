// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/fake_snapshot_generator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeSnapshotGeneratorDelegate

@synthesize view = _view;

- (BOOL)canTakeSnapshotForWebState:(web::WebState*)webState {
  return YES;
}

- (UIEdgeInsets)snapshotEdgeInsetsForWebState:(web::WebState*)webState {
  return UIEdgeInsetsZero;
}

- (NSArray<SnapshotOverlay*>*)snapshotOverlaysForWebState:
    (web::WebState*)webState {
  return nil;
}

- (void)willUpdateSnapshotForWebState:(web::WebState*)webState {
}

- (void)didUpdateSnapshotForWebState:(web::WebState*)webState
                           withImage:(UIImage*)snapshot {
}

- (UIView*)viewForWebState:(web::WebState*)webState {
  return self.view;
}

@end
