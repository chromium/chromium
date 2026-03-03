// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_SCHEDULING_OUTCOME_H_
#define IOS_CHROME_APP_TASK_SCHEDULING_OUTCOME_H_

// Outcomes of scheduling a task request.
// LINT.IfChange
enum class TaskSchedulingOutcome {
  // Task was successfully scheduled and will run when its stage is reached.
  kScheduled = 0,
  // Task was dropped because another task with a different Gaia ID is already
  // pending.
  kDroppedGaiaMismatch = 1,
  kMaxValue = kDroppedGaiaMismatch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml)

#endif  // IOS_CHROME_APP_TASK_SCHEDULING_OUTCOME_H_
