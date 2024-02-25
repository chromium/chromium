// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"

@implementation FakeSnapshotGeneratorDelegate

@synthesize view = _view;

- (BOOL)canTakeSnapshotWithWebStateInfo:(WebStateSnapshotInfo*)webStateInfo {
  return YES;
}

- (void)willUpdateSnapshotWithWebStateInfo:(WebStateSnapshotInfo*)webStateInfo {
}

- (UIEdgeInsets)snapshotEdgeInsetsWithWebStateInfo:
    (WebStateSnapshotInfo*)webStateInfo {
  return UIEdgeInsetsZero;
}

- (NSArray<UIView*>*)snapshotOverlaysWithWebStateInfo:
    (WebStateSnapshotInfo*)webStateInfo {
  return nil;
}

- (UIView*)baseViewWithWebStateInfo:(WebStateSnapshotInfo*)webStateInfo {
  return self.view;
}

@end
