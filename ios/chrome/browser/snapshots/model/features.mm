// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/features.h"

#import "ios/chrome/browser/shared/public/features/features.h"

BASE_FEATURE(kSnapshotInSwift,
             "SnapshotInSwift",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLargeCapacityInSnapshotLRUCache,
             "LargeCapacityInSnapshotLRUCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsLargeCapacityInSnapshotLRUCacheEnabled() {
  return IsTabGroupInGridEnabled() &&
         base::FeatureList::IsEnabled(kLargeCapacityInSnapshotLRUCache);
}
