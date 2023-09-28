// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_LOOP_DETECTION_UTIL_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_LOOP_DETECTION_UTIL_H_

namespace crash_util {

// Returns the number of consecutive failed startups ('instant' crashes) prior
// to this one. This method always returns the number of prior failures even
// after calls to increment or reset the value.
int GetFailedStartupAttemptCount();

// Increases the failed startup count. This should be called immediately after
// startup, so that if there is a crash, it is recorded.
// If `flush_immediately` is true, the value will be persisted immediately. If
// `flush_immediately` is false, this should be followed by a call to
// [[NSUserDefaults standardUserDefaults] synchronize]. This is optional to
// allow coallescing of the potentially expensive call during startup.
void IncrementFailedStartupAttemptCount(bool flush_immediately);

// Resets the failed startup count. This should be called once there is some
// indication the user isn't in a crash loop (e.g., some amount of time has
// elapsed, or some deliberate user action has been taken).
void ResetFailedStartupAttemptCount();

// Resets the hidden state of failed startup attempt count for testing.
void ResetFailedStartupAttemptCountForTests();

}  // namespace crash_util

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_LOOP_DETECTION_UTIL_H_
