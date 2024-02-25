// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_FIRST_USER_ACTION_RECORDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_FIRST_USER_ACTION_RECORDER_H_

#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"

// Histogram names (visible for testing only).
// Each is a 2-element array where the first element is the
// name of the histogram for handsets and the second is the name of the
// histogram for tablets.

// The histogram to plot background duration before 'new task' actions.
extern const char* kFirstUserActionNewTaskHistogramName[];

// The histogram to plot background duration before 'continuation' actions.
extern const char* kFirstUserActionContinuationHistogramName[];

// The histogram to plot background duration before 'expiration' actions.
extern const char* kFirstUserActionExpirationHistogramName[];

// The name of the histogram to plot the type of first user action (see
// `FirstUserActionType`).
extern const char* kFirstUserActionTypeHistogramName[];

// Since it logs user actions while it exists, it should only be instantiated
// while metrics recording is enabled.
class FirstUserActionRecorder {
 public:
  // Indicies of the histogram name arrays for each device family.
  enum DeviceFamilyIndex { HANDSET = 0, TABLET = 1 };

  // Values of the first user action type histogram.
  enum FirstUserActionType {
    NEW_TASK = 0,
    CONTINUATION,
    EXPIRATION,
    START_ON_NTP,
    FIRST_USER_ACTION_TYPE_COUNT,
  };

  explicit FirstUserActionRecorder(base::TimeDelta background_duration);

  FirstUserActionRecorder(const FirstUserActionRecorder&) = delete;
  FirstUserActionRecorder& operator=(const FirstUserActionRecorder&) = delete;

  virtual ~FirstUserActionRecorder();

  // Records that no applicable user action occurred.
  void Expire();

  // Records that the app started with the NTP active.
  void RecordStartOnNTP();

 private:
  // Records metrics if `action_name` indicates the start of a new task or the
  // continuation of an existing task.
  void OnUserAction(const std::string& action_name,
                    base::TimeTicks action_time);

  // Records the appropriate metrics for the given action type.
  void RecordAction(const FirstUserActionType& action_type,
                    const std::string& log_message);

  // Returns true if the specified action should be processed, or false if the
  // action should be ignored.
  bool ShouldProcessAction(const std::string& action_name,
                           base::TimeTicks action_time);

  // Returns true if the given array contains the given string.
  bool ArrayContainsString(const char* to_search[],
                           const size_t to_search_size,
                           const char* to_find);

  // True if running on a tablet.
  const DeviceFamilyIndex device_family_;

  // True if this instance has recorded a 'new task' or 'continuation task'
  // metric.
  bool recorded_action_;

  // True if this instance has rethrown an action.
  bool action_pending_;

  // The amount of time the app was in the background before this recorder was
  // created.
  base::TimeDelta background_duration_;

  // The callback to invoke when an action is recorded.
  base::ActionCallback action_callback_;

  // A potential action that needs to be confirmed if there is no other relevant
  // action.
  base::CancelableOnceClosure rethrow_callback_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_FIRST_USER_ACTION_RECORDER_H_
