// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_FAKE_SNAPSHOT_GENERATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_FAKE_SNAPSHOT_GENERATOR_DELEGATE_H_

#import "ios/chrome/browser/snapshots/snapshot_generator_delegate.h"

// Fake SnapshotGeneratorDelegate that does nothing. Can be used as a
// base for unit test that only implement a subset of the protocol.
@interface FakeSnapshotGeneratorDelegate : NSObject<SnapshotGeneratorDelegate>

// The view to be snapshotted.
@property(nonatomic, strong) UIView* view;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_FAKE_SNAPSHOT_GENERATOR_DELEGATE_H_
