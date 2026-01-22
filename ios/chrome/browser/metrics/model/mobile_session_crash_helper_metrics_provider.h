// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_MOBILE_SESSION_CRASH_HELPER_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_MOBILE_SESSION_CRASH_HELPER_METRICS_PROVIDER_H_

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum MobileSessionShutdownType {
  SHUTDOWN_IN_BACKGROUND = 0,
  SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING = 1,
  SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING = 2,
  SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING = 3,
  SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING = 4,
  FIRST_LAUNCH_AFTER_UPGRADE = 5,
  // SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN = 6,  // Obsoleted (Jan
  // 2026)
  MOBILE_SESSION_SHUTDOWN_TYPE_COUNT = 7,
};

namespace mobile_session_metrics {

// Logs metrics regarding the previous session's shutdown type that depend
// on crash report analysis (e.g., whether a crash dump exists).
// These metrics are logged after CrashHelper finishes processing intermediate
// dumps to avoid blocking file operations on startup.
// Immediate metrics (e.g., tab counts for clean shutdowns) are handled by
// MobileSessionShutdownMetricsProvider.
void OnProcessIntermediateDumpsFinished(bool has_new_pending_reports);

}  // namespace mobile_session_metrics

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_MOBILE_SESSION_CRASH_HELPER_METRICS_PROVIDER_H_
