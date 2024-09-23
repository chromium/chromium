// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/amplitude_peak_detector.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_sample_types.h"

namespace media {

constexpr float kLoudnessThreshold = 0.5;  // Corresponds to approximately -6dbs

AmplitudePeakDetector::AmplitudePeakDetector(PeakDetectedCB peak_detected_cb)
    : peak_detected_cb_(std::move(peak_detected_cb)) {
  // For performance reasons, we only check whether we are tracing once, at
  // construction time, since we don't expect this category to be enabled often.
  // This comes at a usability cost: tracing must be started before a website
  // creates any streams. Refreshing a page after starting a trace might not be
  // enough force the recreation of streams too: one must close the tab,
  // navigate to the chrome://media-internals audio tab, and wait for all
  // streams to disappear (usually 2-10s).
  is_tracing_enabled_ = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("audio.latency"),
                                     &is_tracing_enabled_);
}

AmplitudePeakDetector::~AmplitudePeakDetector() = default;

void AmplitudePeakDetector::SetIsTracingEnabledForTests(
    bool is_tracing_enabled) {
  is_tracing_enabled_ = is_tracing_enabled;
}

void AmplitudePeakDetector::FindPeak(const void* data,
                                     int frames,
                                     int bytes_per_sample) {
  if (!is_tracing_enabled_) [[likely]] {
    return;
  }

  MaybeReportPeak(AreFramesLoud(data, frames, bytes_per_sample));
}

void AmplitudePeakDetector::FindPeak(const AudioBus* audio_bus) {
  if (!is_tracing_enabled_) [[likely]] {
    return;
  }

  MaybeReportPeak(AreFramesLoud(audio_bus));
}

template <class T>
bool IsDataLoud(const T* audio_data,
                int frames,
                const T min_loudness,
                const T max_loudness) {
  int n = 0;
  do {
    if (audio_data[n] < min_loudness || audio_data[n] > max_loudness) {
      return true;
    }
  } while (++n < frames);

  return false;
}

template <class T>
bool LoudDetector(const void* data, int frames) {
  const T* audio_data = reinterpret_cast<const T*>(data);

  constexpr T min_loudness =
      FixedSampleTypeTraits<T>::FromFloat(-kLoudnessThreshold);
  constexpr T max_loudness =
      FixedSampleTypeTraits<T>::FromFloat(kLoudnessThreshold);

  return IsDataLoud<T>(audio_data, frames, min_loudness, max_loudness);
}

template <>
bool LoudDetector<float>(const void* data, int frames) {
  return IsDataLoud<float>(reinterpret_cast<const float*>(data), frames,
                           -kLoudnessThreshold, kLoudnessThreshold);
}

// Returns whether if any of the samples in `audio_bus` surpass
// `kLoudnessThreshold`.
bool AmplitudePeakDetector::AreFramesLoud(const AudioBus* audio_bus) {
  DCHECK(!audio_bus->is_bitstream_format());

  for (int ch = 0; ch < audio_bus->channels(); ++ch) {
    if (LoudDetector<float>(audio_bus->channel(ch), audio_bus->frames())) {
      return true;
    }
  }
  return false;
}

// Returns whether if any of the samples in `data` surpass `kLoudnessThreshold`.
bool AmplitudePeakDetector::AreFramesLoud(const void* data,
                                          int frames,
                                          int bytes_per_sample) {
  switch (bytes_per_sample) {
    case 1:
      return LoudDetector<uint8_t>(data, frames);

    case 2:
      return LoudDetector<int16_t>(data, frames);

    case 4:
      return LoudDetector<int32_t>(data, frames);
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  };
}

void AmplitudePeakDetector::MaybeReportPeak(bool are_frames_loud) {
  // We never expect two threads to be calling into the peak detector at the
  // same time. However, some platform implementations can unpredictably change
  // underlying realtime audio threads (e.g. during a device change).
  // This prevents us from using a ThreadChecker, which is bound to a specific
  // thread ID.
  // Instead, check that there is never be contention on `lock_`. If there ever
  // was, we would know there is a valid threading issue that needs to be
  // investigated.
  lock_.AssertNotHeld();
  base::AutoLock auto_lock(lock_);

  // No change.
  if (in_a_peak_ == are_frames_loud) {
    return;
  }

  // TODO(tguilbert): consider only "exiting" a peak after a few consecutive
  // quiet buffers; this should reduce the chance of accidentally detecting
  // another rising edge.
  in_a_peak_ = are_frames_loud;

  // Volume has transitioned from quiet to loud; we found a rising edge.
  // `is_trace_start` indicates whether to start or stop the trace, whether we
  // are tracing audio input or output respectively.
  if (in_a_peak_) {
    peak_detected_cb_.Run();
  }
}

}  // namespace media
