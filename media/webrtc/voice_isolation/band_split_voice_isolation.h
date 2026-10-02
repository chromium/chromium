// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_BAND_SPLIT_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_BAND_SPLIT_VOICE_ISOLATION_H_

#include <array>
#include <cstddef>
#include <memory>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"

namespace media {

// Receives two concatenated FFTs of the same size from a 48kHz signal and
// performs a band split to extract the 16kHz low band for the internal
// component, then reconstructs the 48kHz FFTs by zero-padding the high bands.
// The internal component must be the 16kHz model: two concatenated DFTs of
// 20 ms (320 samples) per call, 50 calls per second. The constructor CHECKs
// this layout.
class COMPONENT_EXPORT(MEDIA_WEBRTC) BandSplitVoiceIsolation
    : public VoiceIsolationComponent {
 public:
  // Two concatenated DFTs per call.
  static constexpr size_t kNumDfts = 2;
  // 48 kHz / 16 kHz.
  static constexpr size_t kBandRatio = 3;
  // Calls per second of both the band split and the inner model.
  static constexpr size_t kFramesPerSecond = 50;
  // Inner 16 kHz model: two 320-float DFTs (20 ms at 16 kHz).
  static constexpr size_t kInnerFrameSize = 640;
  // Outer: two 960-float DFTs (20 ms at 48 kHz).
  static constexpr size_t kFrameSize = kBandRatio * kInnerFrameSize;

  explicit BandSplitVoiceIsolation(
      std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation);
  ~BandSplitVoiceIsolation() override;

  BandSplitVoiceIsolation(const BandSplitVoiceIsolation&) = delete;
  BandSplitVoiceIsolation& operator=(const BandSplitVoiceIsolation&) = delete;

  void ProcessAudio(base::span<const float> dfts_input,
                    base::span<float> dfts_output) override;

  size_t FrameSize() const override;

  size_t FramesPerSecond() const override;

  void ClearBuffers() override;

  // The band split keeps no history of its own, so this is the delay of the
  // internal component.
  base::TimeDelta AlgorithmicDelay() const override;

 private:
  const std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation_;
  std::array<float, kInnerFrameSize> internal_input_buffer_{};
  std::array<float, kInnerFrameSize> internal_output_buffer_{};
};

}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_BAND_SPLIT_VOICE_ISOLATION_H_
