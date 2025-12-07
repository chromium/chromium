// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_UTIL_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_UTIL_H_

#import "ios/chrome/browser/snapshots/model/snapshot_types.h"

// Returns a new SnapshotRetrievedBlock that when invoked will record
// how long the `operation` took and then will invoke `callback`.
SnapshotRetrievedBlock BlockRecordingElapsedTime(SnapshotOperation operation,
                                                 SnapshotRetrievedBlock block);

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_UTIL_H_
