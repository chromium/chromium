// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/band_split_voice_isolation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/trace_event/trace_event.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"

namespace media {

namespace {

// Floats per 48kHz DFT (960) and per 16kHz low-band DFT (320, 0 to 8kHz).
constexpr size_t kFullBandSize =
    BandSplitVoiceIsolation::kFrameSize / BandSplitVoiceIsolation::kNumDfts;
constexpr size_t kLowBandSize = BandSplitVoiceIsolation::kInnerFrameSize /
                                BandSplitVoiceIsolation::kNumDfts;
// PFFFT ordered real layout: [0] = DC, [1] = Nyquist, then (re, im) pairs.
constexpr size_t kNyquistIndex = 1;
constexpr float kForwardScale = 1.0f / BandSplitVoiceIsolation::kBandRatio;
constexpr float kInverseScale = BandSplitVoiceIsolation::kBandRatio;

}  // namespace

BandSplitVoiceIsolation::BandSplitVoiceIsolation(
    std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation)
    : internal_voice_isolation_(std::move(internal_voice_isolation)) {
  CHECK(internal_voice_isolation_);
  CHECK_EQ(internal_voice_isolation_->FrameSize(), kInnerFrameSize);
  CHECK_EQ(internal_voice_isolation_->FramesPerSecond(), kFramesPerSecond);
}

BandSplitVoiceIsolation::~BandSplitVoiceIsolation() = default;

void BandSplitVoiceIsolation::ProcessAudio(base::span<const float> dfts_input,
                                           base::span<float> dfts_output) {
  TRACE_EVENT("audio", "BandSplitVoiceIsolation::ProcessAudio");
  CHECK_EQ(dfts_input.size(), kFrameSize);
  CHECK_EQ(dfts_output.size(), kFrameSize);

  base::span<float> internal_in_span(internal_input_buffer_);
  base::span<const float> internal_out_span(internal_output_buffer_);

  // Copy the lower frequency bins (0 to 8kHz) of each 48kHz FFT into the
  // internal 16kHz FFT buffer. In PFFFT's ordered real FFT format, index 1
  // stores the Nyquist component, so populate the 16kHz Nyquist slot (8kHz)
  // using the real part of the 8kHz bin (index `kLowBandSize`) of the 48kHz
  // FFT.
  for (size_t dft = 0; dft < kNumDfts; ++dft) {
    const size_t inner_offset = dft * kLowBandSize;
    const size_t outer_offset = dft * kFullBandSize;

    internal_in_span.subspan(inner_offset, /*count=*/kLowBandSize)
        .copy_from_nonoverlapping(
            dfts_input.subspan(outer_offset, /*count=*/kLowBandSize));
    internal_input_buffer_[inner_offset + kNyquistIndex] =
        dfts_input[outer_offset + kLowBandSize];
  }

  // Scale down DFT amplitudes to match the 3x shorter FFT window length of the
  // 16kHz representation.
  for (float& coeff : internal_input_buffer_) {
    coeff *= kForwardScale;
  }

  // Run the internal 16kHz voice isolation model on the extracted low band.
  internal_voice_isolation_->ProcessAudio(internal_input_buffer_,
                                          internal_output_buffer_);

  // Scale up DFT amplitudes to match the 48kHz FFT window length before
  // copying into the 48kHz output buffer.
  for (float& coeff : internal_output_buffer_) {
    coeff *= kInverseScale;
  }

  // Zero out the entire 48kHz output buffer to clear all high-frequency bands
  // above 8kHz.
  std::fill(dfts_output.begin(), dfts_output.end(), 0.0f);

  // Copy the processed 16kHz low-band coefficients back into the lower bins of
  // each 48kHz FFT. Then restore the 8kHz component from the 16kHz Nyquist slot
  // back to its bin position in the 48kHz FFT, and zero the 48kHz Nyquist slot
  // (24kHz).
  for (size_t dft = 0; dft < kNumDfts; ++dft) {
    const size_t inner_offset = dft * kLowBandSize;
    const size_t outer_offset = dft * kFullBandSize;

    dfts_output.subspan(outer_offset, /*count=*/kLowBandSize)
        .copy_from_nonoverlapping(
            internal_out_span.subspan(inner_offset, /*count=*/kLowBandSize));
    dfts_output[outer_offset + kLowBandSize] =
        internal_output_buffer_[inner_offset + kNyquistIndex];
    dfts_output[outer_offset + kNyquistIndex] = 0.0f;
  }
}

size_t BandSplitVoiceIsolation::FrameSize() const {
  return kFrameSize;
}

size_t BandSplitVoiceIsolation::FramesPerSecond() const {
  return kFramesPerSecond;
}

void BandSplitVoiceIsolation::ClearBuffers() {
  // `internal_input_buffer_` and `internal_output_buffer_` need no reset: every
  // ProcessAudio() call overwrites them completely before reading them, so they
  // carry no state between calls.
  internal_voice_isolation_->ClearBuffers();
}

base::TimeDelta BandSplitVoiceIsolation::AlgorithmicDelay() const {
  return internal_voice_isolation_->AlgorithmicDelay();
}

}  // namespace media
