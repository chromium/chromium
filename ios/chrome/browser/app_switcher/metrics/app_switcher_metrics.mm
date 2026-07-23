// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_switcher/metrics/app_switcher_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/time/time.h"

void RecordAppSwitcherAISummarizationEntrypoint() {
  base::RecordAction(
      base::UserMetricsAction("IOS.AISummarization.AppSwitcherEntrypoint"));
}

void RecordAppSwitcherFetchOutcome(bool success) {
  base::UmaHistogramBoolean("IOS.AppSwitcher.FetchOutcome", success);
}

void RecordAppSwitcherFetchDuration(base::TimeDelta duration) {
  base::UmaHistogramTimes("IOS.AppSwitcher.FetchDuration", duration);
}
