// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_ECHO_INFORMATION_H_
#define MEDIA_WEBRTC_ECHO_INFORMATION_H_

#include "base/component_export.h"
#include "base/threading/thread_checker.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {

// A helper class to log echo information in general and AEC2
// quality in particular.
class COMPONENT_EXPORT(MEDIA_WEBRTC) EchoInformation {
 public:
  EchoInformation();
  virtual ~EchoInformation();

  // Updates stats, and reports metrics as UMA stats every 5 seconds.
  // Must be called every time AudioProcessing::ProcessStream() is called.
  void UpdateAecStats(
      const webrtc::AudioProcessingStats& audio_processing_stats);

  // Reports AEC divergent filter metrics as UMA and resets the associated data.
  void ReportAndResetAecDivergentFilterStats();

 private:
  // Counter to store a new value for the divergent filter fraction metric in
  // AEC2, once every second.
  int divergent_filter_stats_time_ms_;

  // Total number of times we queried for the divergent filter fraction metric.
  int num_divergent_filter_fraction_;

  // Number of non-zero divergent filter fraction metrics.
  int num_non_zero_divergent_filter_fraction_;

  // Ensures that this class is accessed on the same thread.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(EchoInformation);
};

}  // namespace media

#endif  // MEDIA_WEBRTC_ECHO_INFORMATION_H_
