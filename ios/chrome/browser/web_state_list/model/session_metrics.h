// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_SESSION_METRICS_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_SESSION_METRICS_H_

#import "base/supports_user_data.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// Flags used to control which metrics are recorded by RecordSessionMetrics.
// They can be combined with bitwise "or" operator to record multiple metrics
// at the same time.
enum class MetricsToRecordFlags : unsigned int {
  kNoMetrics = 0,
  // kClosedTabCount(1), kOpenedTabCount(2) no longer used.
  kActivatedTabCount = 1 << 2,
};

// This enum allow for type safety when create a set of MetricsToRecordFlags.
enum class MetricsToRecordFlagSet : unsigned int {};

// Combines flags into a value that can be passed to RecordSessionMetrics.
MetricsToRecordFlagSet operator|(MetricsToRecordFlagSet set,
                                 MetricsToRecordFlags flag);
MetricsToRecordFlagSet operator|(MetricsToRecordFlags lhs,
                                 MetricsToRecordFlags rhs);

// Class used to hold sessions specific metrics (count of tab
// open, closed, ...).
class SessionMetrics : public base::SupportsUserData::Data {
 public:
  // Retrieves the instance of SessionMetrics that is attached
  // to the specified ProfileIOS.
  static SessionMetrics* FromProfile(ProfileIOS* profile);

  // Constructable by tests.
  SessionMetrics();

  ~SessionMetrics() override;
  SessionMetrics(const SessionMetrics&) = delete;
  SessionMetrics& operator=(const SessionMetrics&) = delete;

  // Record metrics counters specified by `metrics_to_record` which is a
  // bitwise "or" combination of MetricsToRecordFlags. All other metrics
  // are cleared.
  void RecordAndClearSessionMetrics(MetricsToRecordFlagSet flag_set);
  void RecordAndClearSessionMetrics(MetricsToRecordFlags flag);

  // Called when the corresponding event is triggered so that
  // the appropriate metric is incremented.
  void OnWebStateActivated();

 private:
  // Reset metrics counters.
  void ResetSessionMetrics();

  // Counters for metrics.
  unsigned int activated_web_state_counter_ = 0;
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_SESSION_METRICS_H_
