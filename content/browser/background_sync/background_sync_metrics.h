// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_METRICS_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_METRICS_H_

#include "content/common/content_export.h"

namespace content {

// This class contains the logic for recording usage metrics for the Background
// Sync API. It is stateless, containing only static methods, so it can be used
// by any of the Background Sync code, without needing to be instantiated
// explicitly.
class CONTENT_EXPORT BackgroundSyncMetrics {
 public:
  // ResultPattern is used by Histograms, append new entries at the end.
  enum ResultPattern {
    RESULT_PATTERN_SUCCESS_FOREGROUND = 0,
    RESULT_PATTERN_SUCCESS_BACKGROUND = 1,
    RESULT_PATTERN_FAILED_FOREGROUND = 2,
    RESULT_PATTERN_FAILED_BACKGROUND = 3,
    RESULT_PATTERN_MAX = RESULT_PATTERN_FAILED_BACKGROUND,
  };

  BackgroundSyncMetrics() = delete;
  BackgroundSyncMetrics(const BackgroundSyncMetrics&) = delete;
  BackgroundSyncMetrics& operator=(const BackgroundSyncMetrics&) = delete;

  // Records the result of a single one-shot sync event firing.
  static void RecordOneShotEventResult(bool success,
                                       bool finished_in_foreground);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_METRICS_H_
