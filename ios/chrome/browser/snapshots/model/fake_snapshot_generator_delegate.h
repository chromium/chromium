// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FAKE_SNAPSHOT_GENERATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FAKE_SNAPSHOT_GENERATOR_DELEGATE_H_

#import "ios/chrome/browser/snapshots/model/model_swift.h"

// Fake SnapshotGeneratorDelegate that does nothing. Can be used as a
// base for unit tests that only implement a subset of the protocol.
@interface FakeSnapshotGeneratorDelegate : NSObject <SnapshotGeneratorDelegate>

// The view to be snapshotted.
@property(nonatomic, strong) UIView* view;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FAKE_SNAPSHOT_GENERATOR_DELEGATE_H_
