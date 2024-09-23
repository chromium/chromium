// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_METRICS_H_
#define IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_METRICS_H_

// Histogram name for appState init stage when handleRefreshWithCompletion: is
// run in background.
extern const char kInitStageDuringBackgroundRefreshHistogram[];

// Enum for the IOS.BackgroundRefresh.InitStage histogram.
// Keep in sync with "InitStageDuringBackgroundRefreshType".
enum class InitStageDuringBackgroundRefreshActions {
  kUnknown = 0,
  kInitStageStart = 1,
  kInitStageBrowserBasic = 2,
  kInitStageSafeMode = 3,
  kInitStageVariationsSeed = 4,
  kInitStageBrowserObjectsForBackgroundHandlers = 5,
  kInitStageEnterprise = 6,
  kInitStageBrowserObjectsForUI = 7,
  kInitStageNormalUI = 8,
  kInitStageFirstRun = 9,
  kInitStageChoiceScreen = 10,
  kInitStageFinal = 11,
  kMaxValue = kInitStageFinal,
};

// Histogram name to track task request submission errors when there are some
// tasks to be run in background.
extern const char kBGTaskSchedulerErrorHistogram[];

// Enum for the IOS.BackgroundRefresh.BGTaskSchedulerError histogram.
// Keep in sync with "BGTaskSchedulerErrorType".
enum class BGTaskSchedulerErrorActions {
  kUnknown = 0,
  kSuccess = 1,
  kErrorCodeUnavailable = 2,
  kErrorCodeNotPermitted = 3,
  kErrorCodeTooManyPendingTaskRequests = 4,
  kMaxValue = kErrorCodeTooManyPendingTaskRequests,
};

#endif  // IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_METRICS_H_
