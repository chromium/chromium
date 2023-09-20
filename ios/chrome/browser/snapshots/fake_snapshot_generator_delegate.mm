// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/fake_snapshot_generator_delegate.h"

@implementation FakeSnapshotGeneratorDelegate

@synthesize view = _view;

- (BOOL)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    canTakeSnapshotForWebState:(web::WebState*)webState {
  return YES;
}

- (UIEdgeInsets)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    snapshotEdgeInsetsForWebState:(web::WebState*)webState {
  return UIEdgeInsetsZero;
}

- (NSArray<UIView*>*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
           snapshotOverlaysForWebState:(web::WebState*)webState {
  return nil;
}

- (void)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    willUpdateSnapshotForWebState:(web::WebState*)webState {
}

- (UIView*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
         baseViewForWebState:(web::WebState*)webState {
  return self.view;
}

@end
