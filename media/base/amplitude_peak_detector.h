// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AMPLITUDE_PEAK_DETECTOR_H_
#define MEDIA_BASE_AMPLITUDE_PEAK_DETECTOR_H_

#include "base/synchronization/lock.h"
#include "media/base/audio_bus.h"
#include "media/base/media_export.h"

namespace media {
// Helper class which acts as a filter to detect jumps in audio signal
// amplitude. When there is a large increase in amplitude, it will run its
// provided callback.
//
// This class can be used to start/stop tracing events, to measure internal
// audio latency. Traces should be started right after detecting a peak in the
// audio input, and stopped right before sending a peak to be played out; this
// should estimating the end-to-end latency from microphone input to speakers.
//
// An example test page with instructions can be found under
// third_party/blink/manual_tests/audio_latency.html, and more general
// documentation can be found under docs/media/latency_tracing.md.
//
// Note: does nothing if the "audio.latency" tracing category is disabled.
//
// Note: AmplitudePeakDetector expects only one thread to be calling FindPeak().
// It's ok for that thread to change occasionnaly, since a platform's realtime
// audio threads can sometimes change (e.g. when there is a device change).
// Multiple threads calling FindPeak() simultaneously would be a product bug.
class MEDIA_EXPORT AmplitudePeakDetector {
 public:
  using PeakDetectedCB = base::RepeatingClosure;

  explicit AmplitudePeakDetector(PeakDetectedCB peak_detected_cb);
  ~AmplitudePeakDetector();

  AmplitudePeakDetector(const AmplitudePeakDetector&) = delete;
  AmplitudePeakDetector& operator=(const AmplitudePeakDetector&) = delete;

  // Detects increases in amplitude, and runs `peak_detected_cb_` when we cross
  // a threshold (but not when amplitude falls back below the threshold).
  void FindPeak(const void* data, int frames, int bytes_per_sample);
  void FindPeak(const AudioBus* audio_bus);

  void SetIsTracingEnabledForTests(bool is_tracing_enabled);

 private:
  bool AreFramesLoud(const AudioBus* audio_bus);
  bool AreFramesLoud(const void* data, int frames, int bytes_per_sample);

  void MaybeReportPeak(bool are_frames_loud);

  const PeakDetectedCB peak_detected_cb_;

  base::Lock lock_;
  bool in_a_peak_ GUARDED_BY(lock_) = false;

  bool is_tracing_enabled_;
};

}  // namespace media

#endif  // MEDIA_BASE_AMPLITUDE_PEAK_DETECTOR_H_
