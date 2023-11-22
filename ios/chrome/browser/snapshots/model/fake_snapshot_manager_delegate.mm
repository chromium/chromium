// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/fake_snapshot_manager_delegate.h"

@implementation FakeSnapshotManagerDelegate

@synthesize view = _view;

- (BOOL)snapshotManager:(SnapshotManager*)snapshotManager
    canTakeSnapshotForWebState:(web::WebState*)webState {
  return YES;
}

- (UIEdgeInsets)snapshotManager:(SnapshotManager*)snapshotManager
    snapshotEdgeInsetsForWebState:(web::WebState*)webState {
  return UIEdgeInsetsZero;
}

- (NSArray<UIView*>*)snapshotManager:(SnapshotManager*)snapshotManager
           snapshotOverlaysForWebState:(web::WebState*)webState {
  return nil;
}

- (void)snapshotManager:(SnapshotManager*)snapshotManager
    willUpdateSnapshotForWebState:(web::WebState*)webState {
}

- (UIView*)snapshotManager:(SnapshotManager*)snapshotManager
         baseViewForWebState:(web::WebState*)webState {
  return self.view;
}

@end
