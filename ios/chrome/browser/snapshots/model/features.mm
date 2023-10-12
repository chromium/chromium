// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/features.h"

BASE_FEATURE(kGreySnapshotOptimization,
             "GreySnapshotOptimization",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<GreySnapshotOptimizationLevel>::Option
    kGreySnapshotOptimizationLevelOptions[] = {
        {GreySnapshotOptimizationLevel::kDoNotStoreToDisk,
         "do-not-store-to-disk"},
        {GreySnapshotOptimizationLevel::kDoNotStoreToDiskAndCache,
         "do-not-store-to-disk-and-cache"}};

constexpr base::FeatureParam<GreySnapshotOptimizationLevel>
    kGreySnapshotOptimizationLevelParam{
        &kGreySnapshotOptimization, "level",
        GreySnapshotOptimizationLevel::kDoNotStoreToDisk,
        &kGreySnapshotOptimizationLevelOptions};
