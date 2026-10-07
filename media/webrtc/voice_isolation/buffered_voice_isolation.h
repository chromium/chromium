// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_BUFFERED_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_BUFFERED_VOICE_ISOLATION_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"

namespace media {

// Wraps two consecutive external calls into a single call to the internal
// VoiceIsolationComponent by buffering the input between calls. Compared to the
// internal component, FrameSize() is halved and FramesPerSecond() is doubled.
//
// Adds an algorithmic latency of FrameSize() samples (one external frame): the
// output of call k corresponds to the input of call k-1, and the first call
// after construction or ClearBuffers() outputs silence.
//
// ProcessAudio() supports in-place processing: `input` and `output` may refer
// to the same buffer.
class COMPONENT_EXPORT(MEDIA_WEBRTC) BufferedVoiceIsolation
    : public VoiceIsolationComponent {
 public:
  // Number of external frames combined into one internal frame.
  static constexpr size_t kNumBufferedFrames = 2;

  explicit BufferedVoiceIsolation(
      std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation);
  ~BufferedVoiceIsolation() override;

  BufferedVoiceIsolation(const BufferedVoiceIsolation&) = delete;
  BufferedVoiceIsolation& operator=(const BufferedVoiceIsolation&) = delete;

  void ProcessAudio(base::span<const float> input,
                    base::span<float> output) override;

  size_t FrameSize() const override;

  size_t FramesPerSecond() const override;

  void ClearBuffers() override;

  // The delay of the internal component plus one external frame.
  base::TimeDelta AlgorithmicDelay() const override;

 private:
  // Which half of the internal frame the next external call fills.
  enum class HalfFrame { kFirst, kSecond };

  // The member order matters: the constructor initializes the buffers and
  // sizes from `internal_voice_isolation_`.
  const std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation_;
  std::vector<float> internal_input_;
  std::vector<float> internal_output_;
  const size_t frame_size_;
  const size_t frames_per_second_;
  HalfFrame next_half_ = HalfFrame::kFirst;
};

}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_BUFFERED_VOICE_ISOLATION_H_
