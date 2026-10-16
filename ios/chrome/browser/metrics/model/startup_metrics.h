// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_STARTUP_METRICS_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_STARTUP_METRICS_H_

#import <Foundation/Foundation.h>

#include <optional>
#include <string_view>

#include "base/time/time.h"

@protocol StartupInformation;

// Timeout beyond which launch reason classification is considered suspicious.
inline constexpr base::TimeDelta kLaunchReasonTimeout = base::Seconds(30);

// LINT.IfChange(ColdStartType)
// Enum values for Startup.IOSColdStartType histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class ColdStartType : int {
  kRegular = 0,
  kFirstRun = 1,
  kAfterDeviceRestore = 2,
  kAfterChromeUpgrade = 3,
  kAfterDeviceRestoreAndChromeUpgrade = 4,
  kUnknownDeviceRestore = 5,
  kUnknownDeviceRestoreAndChromeUpgrade = 6,
  kMaxValue = kUnknownDeviceRestoreAndChromeUpgrade,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/startup/histograms.xml:IOSColdStartType)

// LINT.IfChange(IOSStartupTemperature)
// Startup temperature variants corresponding to the IOSStartupTemperature
// token.
enum class IOSStartupTemperature {
  kNotPrewarmed,
  kActivePrewarm,
  kSuspicious,
  kLaunchedInBackground,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/startup/histograms.xml:IOSStartupTemperature)

// Converts the startup temperature to a string suffix corresponding to the
// IOSStartupTemperature histogram token variant.
std::string_view IOSStartupTemperatureToString(
    IOSStartupTemperature temperature);

// Converts the cold start type to a string suffix corresponding to the
// IOSColdStartType histogram token variant.
std::string_view IOSColdStartTypeToString(ColdStartType type);

// Returns the startup temperature classification for the current cold start.
IOSStartupTemperature GetStartupTemperature(
    id<StartupInformation> startup_information);

// Returns the cold start type for the current session.
ColdStartType GetColdStartType(id<StartupInformation> startup_information);

// Logs the base, temperature variant (if specified), and temperature + cold
// start type (or just cold start type if temperature is not specified)
// sub-variant histograms for a startup duration metric.
void LogStartupDurationWithTemperatureAndType(
    std::string_view histogram_name,
    base::TimeDelta duration,
    std::optional<IOSStartupTemperature> temperature,
    ColdStartType cold_start_type);

// Computes the startup latency based on temperature and time landmarks.
std::optional<base::TimeDelta> ComputeStartupLatency(
    id<StartupInformation> startup_information,
    std::optional<base::TimeDelta> main_to_now_time,
    base::TimeDelta scene_connection_to_now_time,
    IOSStartupTemperature temperature);

// Records a startup latency metric (such as Startup.FirstPaintLatency or
// Startup.FirstNavigationLatency) using the provided timing landmarks.
void RecordStartupLatencyMetric(std::string_view metric_name,
                                id<StartupInformation> startup_information,
                                std::optional<base::TimeDelta> main_to_now_time,
                                base::TimeDelta scene_connection_to_now_time);

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_STARTUP_METRICS_H_
