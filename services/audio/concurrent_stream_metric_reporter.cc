// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/concurrent_stream_metric_reporter.h"

#include "base/metrics/histogram_functions.h"

namespace audio {

void ConcurrentStreamMetricReporter::OnInputStreamActive() {
  ++active_input_stream_count_;
  if (active_input_stream_count_ == 1) {
    // Reset metric when recording starts.
    max_concurrent_output_streams_metric_ = active_output_stream_count_;
  }
}

void ConcurrentStreamMetricReporter::OnInputStreamInactive() {
  DCHECK_GE(active_input_stream_count_, 1);
  --active_input_stream_count_;
  if (active_input_stream_count_ == 0) {
    // Report metric when recording ends.
    base::UmaHistogramCustomCounts("Media.Audio.MaxOutputStreamsPerInputStream",
                                   max_concurrent_output_streams_metric_, 1, 50,
                                   50);
  }
}

void ConcurrentStreamMetricReporter::OnOutputStreamActive() {
  ++active_output_stream_count_;
  // Report output stream count increases during recording.
  if (active_input_stream_count_ >= 1 &&
      active_output_stream_count_ > max_concurrent_output_streams_metric_) {
    max_concurrent_output_streams_metric_ = active_output_stream_count_;
  }
}

void ConcurrentStreamMetricReporter::OnOutputStreamInactive() {
  DCHECK_GE(active_output_stream_count_, 1);
  --active_output_stream_count_;
}

}  // namespace audio
