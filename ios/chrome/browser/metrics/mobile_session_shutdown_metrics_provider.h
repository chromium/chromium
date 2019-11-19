// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MOBILE_SESSION_SHUTDOWN_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_MOBILE_SESSION_SHUTDOWN_METRICS_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {
class MetricsService;
}

// Exposed for testing purposes only.
// Values of the UMA Stability.MobileSessionShutdownType histogram.
enum MobileSessionShutdownType {
  SHUTDOWN_IN_BACKGROUND = 0,
  SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING,
  SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING,
  SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING,
  SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING,
  FIRST_LAUNCH_AFTER_UPGRADE,
  SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN,
  MOBILE_SESSION_SHUTDOWN_TYPE_COUNT,
};

// Percentage of battery level which is assumed low enough to have possibly
// been the reason for the previous session ending in an unclean shutdown.
// Percent rpresented by a value between 0 and 1.
extern const float kCriticallyLowBatteryLevel;

class MobileSessionShutdownMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit MobileSessionShutdownMetricsProvider(
      metrics::MetricsService* metrics_service);
  ~MobileSessionShutdownMetricsProvider() override;

  // metrics::MetricsProvider
  bool HasPreviousSessionData() override;
  void ProvidePreviousSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 protected:
  // Returns the shutdown type of the last session.
  MobileSessionShutdownType GetLastShutdownType();

  // Provides information on the last session environment, used to decide what
  // stability metrics to provide in ProvidePreviousSessionData.
  // These methods are virtual to be overridden in the tests.
  // The default implementations return the real values.

  // Whether this is the first time the app is launched after an upgrade.
  virtual bool IsFirstLaunchAfterUpgrade();

  // Whether there are crash reports to upload.
  virtual bool HasCrashLogs();

  // Whether there was a memory warning shortly before last shutdown.
  virtual bool ReceivedMemoryWarningBeforeLastShutdown();

  // Whether the main thread was frozen on previous session termination.
  virtual bool LastSessionEndedFrozen();

 private:
  metrics::MetricsService* metrics_service_;
  DISALLOW_COPY_AND_ASSIGN(MobileSessionShutdownMetricsProvider);
};

#endif  // IOS_CHROME_BROWSER_METRICS_MOBILE_SESSION_SHUTDOWN_METRICS_PROVIDER_H_
