// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FEATURES_H_

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"

// Feature flag to enable new snapshot system written in Swift.
BASE_DECLARE_FEATURE(kSnapshotInSwift);

// Feature flag to enable the grey snapshot optimization.
BASE_DECLARE_FEATURE(kGreySnapshotOptimization);

// Enum class to represent the optimization level, used for
// kGreySnapshotOptimization.
enum class GreySnapshotOptimizationLevel {
  kDoNotStoreToDisk,
  kDoNotStoreToDiskAndCache,
};

// Feature param under kGreySnapshotOptimization to select the optimization
// level.
extern const base::FeatureParam<GreySnapshotOptimizationLevel>
    kGreySnapshotOptimizationLevelParam;

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_FEATURES_H_
