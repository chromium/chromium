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
  Flush();
}

void ReaderModeMetricsHelper::CancelReaderHeuristicRecording() {
  // Reset `last_reader_mode_state_` before calling flush to ensure that
  // any existing state is not recorded since this is replaced by cancelation.
  last_reader_mode_state_.reset();
  Flush();

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

void ReaderModeMetricsHelper::RecordReaderDistillerTriggered() {
  distiller_timer_ = std::make_unique<base::ElapsedTimer>();
  last_reader_mode_state_ = ReaderModeState::kDistillationStarted;
}

void ReaderModeMetricsHelper::RecordReaderDistillerCompleted(
    ReaderModeDistillerResult result) {
  last_reader_mode_state_ = ReaderModeState::kDistillationCompleted;

  CHECK(distiller_timer_);
  base::TimeDelta elapsed = distiller_timer_->Elapsed();
  base::UmaHistogramTimes(kReaderModeDistillerLatencyHistogram, elapsed);

  const ukm::SourceId source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Distiller_Latency(source_id)
        .SetLatency(elapsed.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
    ukm::builders::IOS_ReaderMode_Distiller_Result(source_id)
        .SetResult(static_cast<int64_t>(result))
        .Record(ukm::UkmRecorder::Get());
  }
}

void ReaderModeMetricsHelper::RecordReaderShown() {
  last_reader_mode_state_.reset();
  base::UmaHistogramEnumeration(kReaderModeStateHistogram,
                                ReaderModeState::kReaderShown);
}

void ReaderModeMetricsHelper::Flush() {
  if (last_reader_mode_state_.has_value()) {
    base::UmaHistogramEnumeration(kReaderModeStateHistogram,
                                  last_reader_mode_state_.value());
    last_reader_mode_state_.reset();
  }
  distiller_timer_.reset();
  heuristic_timer_.reset();
}
