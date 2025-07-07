// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_metrics_helper.h"

#import "base/metrics/histogram_functions.h"
#import "base/timer/elapsed_timer.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"

ReaderModeMetricsHelper::ReaderModeMetricsHelper(web::WebState* web_state)
    : web_state_(web_state) {}

ReaderModeMetricsHelper::~ReaderModeMetricsHelper() {
  if (!last_reader_mode_state_.has_value()) {
    return;
  }
  base::UmaHistogramEnumeration(kReaderModeStateHistogram,
                                last_reader_mode_state_.value());
}

void ReaderModeMetricsHelper::CancelReaderHeuristicRecording() {
  heuristic_timer_.reset();
  last_reader_mode_state_.reset();
  base::UmaHistogramEnumeration(kReaderModeStateHistogram,
                                ReaderModeState::kHeuristicCanceled);
}

void ReaderModeMetricsHelper::RecordReaderHeuristicTriggered() {
  heuristic_timer_ = std::make_unique<base::ElapsedTimer>();
  last_reader_mode_state_ = ReaderModeState::kHeuristicStarted;
}

void ReaderModeMetricsHelper::RecordReaderHeuristicCompleted(
    ReaderModeHeuristicResult result) {
  base::UmaHistogramEnumeration(kReaderModeHeuristicResultHistogram, result);

  // TODO(crbug.com/429174292): Flush the last expected event that is recorded
  // when reader mode is shown.
  last_reader_mode_state_ = ReaderModeState::kHeuristicCompleted;

  const ukm::SourceId result_source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  if (result_source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Heuristic_Result(result_source_id)
        .SetResult(static_cast<int64_t>(result))
        .Record(ukm::UkmRecorder::Get());
  }

  // If the heuristic is canceled before the start delay then skip latency
  // recording.
  if (!heuristic_timer_) {
    return;
  }
  base::TimeDelta elapsed = heuristic_timer_->Elapsed();
  base::UmaHistogramTimes(kReaderModeHeuristicLatencyHistogram, elapsed);

  const ukm::SourceId latency_source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  if (latency_source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Heuristic_Latency(latency_source_id)
        .SetLatency(elapsed.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  }
}
