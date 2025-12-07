// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_TYPES_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_TYPES_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_kind.h"

// Block invoked when a snapshot image has been retrieved.
using SnapshotRetrievedBlock = void (^)(UIImage* image);

// Represents a snapshot operation.
enum class SnapshotOperation {
  kUpdateSnapshot,
  kRetrieveColorSnapshot,
  kRetrieveGreyscaleSnapshot,
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_TYPES_H_
