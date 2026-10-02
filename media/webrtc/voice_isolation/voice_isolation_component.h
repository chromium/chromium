// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_COMPONENT_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_COMPONENT_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/time/time.h"

namespace media {

// Result of attempting to initialize VoiceIsolation / VoiceIsolationComponent.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Note: `kSuccess` is never returned as an error.
enum class VoiceIsolationCreationResult {
  kSuccess = 0,
  kInterpreterCreationFailed = 1,
  kDelegateCreationFailed = 2,
  kTensorAllocationFailed = 3,
  kIncompatibleModel = 4,
  kWarmupFailed = 5,
  kMaxValue = kWarmupFailed,
};

// A stage of the voice isolation pipeline. The outermost component, returned
// by VoiceIsolation::CreateComponent() or passed to VoiceIsolation::Create(),
// consumes waveform frames sampled at 48 kHz, so that
// `FrameSize() * FramesPerSecond() == 48000`. Inner stages operate on signals
// derived from it, such as the 48 kHz DFTs consumed by BandSplitVoiceIsolation
// and the 16 kHz low band that it forwards to the model.
// VoiceIsolation::Create() CHECKs the sample rate of the external audio and
// the FrameSize() and FramesPerSecond() of the outermost component.
class COMPONENT_EXPORT(MEDIA_WEBRTC) VoiceIsolationComponent {
 public:
  VoiceIsolationComponent() = default;
  virtual ~VoiceIsolationComponent();
  VoiceIsolationComponent(const VoiceIsolationComponent&) = delete;
  VoiceIsolationComponent& operator=(const VoiceIsolationComponent&) = delete;

  // Processes audio from the `input` span and writes the isolated voice audio
  // into the `output` span. Both spans must have exactly `FrameSize()`
  // elements.
  virtual void ProcessAudio(base::span<const float> input,
                            base::span<float> output) = 0;

  // Returns the exact number of samples that this component expects in the
  // `input` and `output` spans for a single call to ProcessAudio().
  virtual size_t FrameSize() const = 0;

  // Returns the exact number of frames per second this component needs to
  // process in real-time. This is the calling frequency. For raw audio signals
  // `FramesPerSecond()`*`FrameSize()` is equal to the sampling rate.
  virtual size_t FramesPerSecond() const = 0;

  // Clears all internal state.
  virtual void ClearBuffers() = 0;

  // Returns the algorithmic delay this component adds: the time between an
  // input sample and its corresponding output sample. Excludes computation
  // time. Includes the delay of any wrapped components. Constant for the
  // lifetime of the object.
  virtual base::TimeDelta AlgorithmicDelay() const = 0;
};
}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_COMPONENT_H_
