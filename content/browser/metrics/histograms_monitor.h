// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_METRICS_HISTOGRAMS_MONITOR_H_
#define CONTENT_BROWSER_METRICS_HISTOGRAMS_MONITOR_H_

#include <map>
#include <string_view>

#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "content/common/content_export.h"

namespace content {

// This class handles the monitoring feature of chrome://histograms page,
// which allows the page to be updated with histograms logged since the
// monitoring started.
//
// Note that this class does not handle merging histograms from any
// |HistogramProvider| instances. It also does not handle synchronizing
// histograms from subprocesses. The caller has the responsibility for these
// beforehand.
class CONTENT_EXPORT HistogramsMonitor {
 public:
  HistogramsMonitor();
  ~HistogramsMonitor();

  HistogramsMonitor(const HistogramsMonitor&) = delete;
  HistogramsMonitor& operator=(const HistogramsMonitor&) = delete;

  // Fetches and records a snapshot of the current histograms, as the baseline
  // to compare against in subsequent calls to GetDiff().
  void StartMonitoring(std::string_view query);

  // Gets the histogram diffs between the current histograms and the snapshot
  // recorded in StartMonitoring().
  base::Value::List GetDiff();

 private:
  // Gets the difference between the histograms argument and the stored snapshot
  // recorded in StartMonitoring().
  base::Value::List GetDiffInternal(
      const base::StatisticsRecorder::Histograms& histograms);

  std::string query_;
  std::map<std::string, std::unique_ptr<base::HistogramSamples>>
      histograms_snapshot_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_METRICS_HISTOGRAMS_MONITOR_H_
