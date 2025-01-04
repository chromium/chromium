// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FEATURES_H_

#ifdef __cplusplus
#import "base/feature_list.h"

// Feature flag to enable new snapshot system written in Swift.
BASE_DECLARE_FEATURE(kSnapshotInSwift);

// Feature flag to allow more elements that the LRU cache can hold.
BASE_DECLARE_FEATURE(kLargeCapacityInSnapshotLRUCache);

extern "C" {
#endif  // extern "C"

// Returns true if the kLargeCapacityInSnapshotLRUCache feature and the tab
// group feature are enabled.
bool IsLargeCapacityInSnapshotLRUCacheEnabled();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FEATURES_H_
