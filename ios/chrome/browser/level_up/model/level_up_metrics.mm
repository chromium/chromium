// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/level_up/model/task_types.h"

namespace {

// Exclusive maximum bucket for `IOS.LevelUp.LevelChanged` and
// `IOS.LevelUp.CurrentLevel`. Leaves headroom for future levels without
// changing the histogram bucket layout.
constexpr int kMaxLevelHistogramBucket = 50;

}  // namespace

void RecordLevelUpTaskStarted(TaskType task_type) {
  base::UmaHistogramEnumeration(kLevelUpTaskStartedHistogram, task_type);
}

void RecordLevelUpTaskCompleted(TaskType task_type) {
  base::UmaHistogramEnumeration(kLevelUpTaskCompletedHistogram, task_type);
}

void RecordLevelUpNotificationsStateChanged(bool enabled) {
  base::UmaHistogramBoolean(kLevelUpNotificationsStateChangedHistogram,
                            enabled);
}

void RecordLevelUpNewTaskNotificationsStateChanged(bool enabled) {
  base::UmaHistogramBoolean(kLevelUpNewTaskNotificationsStateChangedHistogram,
                            enabled);
}

void RecordLevelUpLevelChanged(int level) {
  base::UmaHistogramExactLinear(kLevelUpLevelChangedHistogram, level,
                                kMaxLevelHistogramBucket);
}

void RecordLevelUpCurrentLevel(int level) {
  base::UmaHistogramExactLinear(kLevelUpCurrentLevelHistogram, level,
                                kMaxLevelHistogramBucket);
}

void RecordLevelUpOptInStateChanged(bool opted_in) {
  base::UmaHistogramBoolean(kLevelUpOptInStateChangedHistogram, opted_in);
}
