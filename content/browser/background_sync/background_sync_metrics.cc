// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace {

content::BackgroundSyncMetrics::ResultPattern EventResultToResultPattern(
    bool success,
    bool finished_in_foreground) {
  if (success) {
    return finished_in_foreground ? content::BackgroundSyncMetrics::
                                        RESULT_PATTERN_SUCCESS_FOREGROUND
                                  : content::BackgroundSyncMetrics::
                                        RESULT_PATTERN_SUCCESS_BACKGROUND;
  }
  return finished_in_foreground
             ? content::BackgroundSyncMetrics::RESULT_PATTERN_FAILED_FOREGROUND
             : content::BackgroundSyncMetrics::RESULT_PATTERN_FAILED_BACKGROUND;
}

}  // namespace

namespace content {

// static
void BackgroundSyncMetrics::RecordOneShotEventResult(
    bool success,
    bool finished_in_foreground) {
  base::UmaHistogramEnumeration(
      "BackgroundSync.Event.OneShotResultPattern",
      EventResultToResultPattern(success, finished_in_foreground),
      static_cast<ResultPattern>(RESULT_PATTERN_MAX + 1));
}

}  // namespace content
