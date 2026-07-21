// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/stft_voice_isolation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/pffft/src/pffft.h"

namespace media {

namespace {
// Similar to std::transform but checking that both arguments have the same
// size.
template <typename BinaryOp>
void transform(base::span<const float> input_a,
               base::span<const float> input_b,
               base::span<float> output,
               BinaryOp binary_operation) {
  CHECK_EQ(input_a.size(), input_b.size());
  CHECK_EQ(input_b.size(), output.size());
  std::transform(std::begin(input_a), std::end(input_a), std::begin(input_b),
                 output.begin(), binary_operation);
}

// Similar to std::transform but checking that both arguments have the same
// size and with support for ternary operations.
template <typename TernaryOp>
void transform(base::span<const float> input_a,
               base::span<const float> input_b,
               base::span<const float> input_c,
               base::span<float> output,
               TernaryOp ternary_operation) {
  CHECK_EQ(input_a.size(), input_b.size());
  CHECK_EQ(input_a.size(), input_c.size());
  CHECK_EQ(input_a.size(), output.size());

  auto firstA = input_a.begin();
  auto lastA = input_a.end();
  auto firstB = input_b.begin();
  auto firstC = input_c.begin();
  auto firstOut = output.begin();
  for (; firstA != lastA; ++firstA, ++firstB, ++firstC, ++firstOut) {
    *firstOut = ternary_operation(*firstA, *firstB, *firstC);
  }
}

std::vector<float> ComputeHannWindow(int window_size) {
  CHECK_GE(window_size, 2);
  CHECK_EQ(window_size % 2, 0);
  std::vector<float> window(window_size);
  int N = window_size - 1;

  // Uses Hann window as defined in https://en.wikipedia.org/wiki/Hann_function:
  // w[n] = sin^2(pi * n / N), 0<= n <= N
  // where window_size = N+1.
  for (int n = 0; n < window_size; ++n) {
    auto s = sin(std::numbers::pi * n / N);
    window[n] = s * s;
  }
  return window;
}

std::vector<float> ComputeInvHannWindow(base::span<const float> window) {
  std::vector<float> inv_window(window.size());

  const int half_window_size = window.size() / 2;
  for (int i = 0; i < half_window_size; ++i) {
    double square_1 = window[i] * window[i];
    double square_2 =
        window[half_window_size + i] * window[half_window_size + i];
    double sum_of_squares = square_1 + square_2;
    inv_window[i] = window[i] / sum_of_squares;
    inv_window[half_window_size + i] =
        window[half_window_size + i] / sum_of_squares;
  }
  return inv_window;
}
}  // namespace

WindowedFft::WindowedFft(int fft_size)
    : fft_size_(fft_size),
      fft_window_(ComputeHannWindow(fft_size_)),
      inv_fft_window_(ComputeInvHannWindow(fft_window_)),
      forward_buffer_(base::AlignedUninit<float>(fft_size, 16)),
      temp_buffer_(base::AlignedUninit<float>(fft_size, 16)),
      inverse_buffer_(base::AlignedUninit<float>(fft_size / 2, 16)),
      fft_state_(pffft_new_setup(fft_size_, PFFFT_REAL), &pffft_destroy_setup),
      fft_workplace_(base::AlignedUninit<float>(fft_size, 16)) {
  std::fill(forward_buffer_.begin(), forward_buffer_.end(), 0.0f);
  std::fill(temp_buffer_.begin(), temp_buffer_.end(), 0.0f);
  std::fill(inverse_buffer_.begin(), inverse_buffer_.end(), 0.0f);
  std::fill(fft_workplace_.begin(), fft_workplace_.end(), 0.0f);
  DVLOG(1) << "WindowedFft fft_size=" << fft_size_;
}

WindowedFft::~WindowedFft() = default;

void WindowedFft::ForwardTransform(base::span<const float> input_audio,
                                   base::span<float> output_dfts) {
  CHECK_EQ(input_audio.size(), fft_size_);
  CHECK_EQ(output_dfts.size(), 2 * fft_size_);

  auto apply_window = [](float a, float b) { return a * b; };

  auto input_audio_first_half = input_audio.first(fft_size_ / 2);
  auto input_audio_second_half = input_audio.subspan(fft_size_ / 2);

  auto output_dfts_first_half = output_dfts.first(fft_size_);
  auto output_dfts_second_half = output_dfts.subspan(fft_size_);

  const auto window_first_half = base::span(fft_window_).first(fft_size_ / 2);
  const auto window_second_half =
      base::span(fft_window_).subspan(fft_size_ / 2);
  auto forward_buffer_first_half =
      base::span(forward_buffer_).first(fft_size_ / 2);
  auto forward_buffer_second_half =
      base::span(forward_buffer_).subspan(fft_size_ / 2);

  // First half: apply second half of the window and put in first half of
  // internal FFT input. Then do FFT, put in first half of internal input.
  media::transform(input_audio_first_half, window_second_half,
                   forward_buffer_second_half, apply_window);

  pffft_transform_ordered(fft_state_.get(), forward_buffer_.data(),
                          temp_buffer_.data(), fft_workplace_.data(),
                          PFFFT_FORWARD);

  output_dfts_first_half.copy_from_nonoverlapping(temp_buffer_);

  // Second half: apply full window to all input and use the full internal
  // buffer. The do FFT, put in second half of internal input.
  media::transform(input_audio, fft_window_, forward_buffer_, apply_window);
  pffft_transform_ordered(fft_state_.get(), forward_buffer_.data(),
                          temp_buffer_.data(), fft_workplace_.data(),
                          PFFFT_FORWARD);
  output_dfts_second_half.copy_from_nonoverlapping(temp_buffer_);

  // Prepare buffer for next call.
  media::transform(input_audio_second_half, window_first_half,
                   forward_buffer_first_half, apply_window);
}

void WindowedFft::InverseTransform(base::span<float> input_dfts,
                                   base::span<float> output_audio) {
  CHECK_EQ(input_dfts.size(), 2 * fft_size_);
  CHECK_EQ(output_audio.size(), fft_size_);
  CHECK_EQ(inv_fft_window_.size(), fft_size_);

  const float inverse_fft_size = 1.f / fft_size_;
  auto apply_window = [inverse_fft_size](float a, float b) {
    return a * b * inverse_fft_size;
  };
  auto apply_window_and_accumulate = [apply_window](float a, float b, float c) {
    return apply_window(a, b) + c;
  };

  const auto temp_first_half =
      base::span<const float>(temp_buffer_).first(fft_size_ / 2);
  const auto temp_second_half =
      base::span<const float>(temp_buffer_).subspan(fft_size_ / 2);
  const auto window_first_half =
      base::span<const float>(inv_fft_window_).first(fft_size_ / 2);
  const auto window_second_half =
      base::span<const float>(inv_fft_window_).subspan(fft_size_ / 2);
  base::span<float> input_first_dft = input_dfts.first(fft_size_);
  base::span<float> input_second_dft = input_dfts.subspan(fft_size_);
  auto output_audio_first_half = output_audio.first(fft_size_ / 2);
  auto output_audio_second_half = output_audio.subspan(fft_size_ / 2);

  // Copy output from last call into output audio.
  output_audio_first_half.copy_from_nonoverlapping(inverse_buffer_);

  // We first compute the first inverse FFT, apply the second half of the window
  // and accumulate into first half of the output audio.
  pffft_transform_ordered(fft_state_.get(), input_first_dft.data(),
                          temp_buffer_.data(), fft_workplace_.data(),
                          PFFFT_BACKWARD);

  media::transform(temp_first_half, window_first_half, output_audio_first_half,
                   output_audio_first_half, apply_window_and_accumulate);
  media::transform(temp_second_half, window_second_half,
                   output_audio_second_half, apply_window);

  pffft_transform_ordered(fft_state_.get(), input_second_dft.data(),
                          temp_buffer_.data(), fft_workplace_.data(),
                          PFFFT_BACKWARD);

  media::transform(temp_first_half, window_first_half, output_audio_second_half,
                   output_audio_second_half, apply_window_and_accumulate);

  // Keep the last half output for the next call.
  media::transform(temp_second_half, window_second_half, inverse_buffer_,
                   apply_window);
}

StftVoiceIsolation::StftVoiceIsolation(
    std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation)
    : fft_size_(internal_voice_isolation->FrameSize() / 2),
      internal_voice_isolation_(std::move(internal_voice_isolation)),
      windowed_fft_(std::make_unique<WindowedFft>(fft_size_)),
      internal_input_buffer_(
          base::AlignedUninit<float>(internal_voice_isolation_->FrameSize(),
                                     16)),
      internal_output_buffer_(
          base::AlignedUninit<float>(internal_voice_isolation_->FrameSize(),
                                     16)) {
  CHECK_GT(fft_size_, 0u);
  DVLOG(1) << "StftVoiceIsolation fft_size=" << fft_size_
           << " frame_size=" << FrameSize();
}

StftVoiceIsolation::~StftVoiceIsolation() = default;

void StftVoiceIsolation::ProcessAudio(base::span<const float> waveform_input,
                                      base::span<float> waveform_output) {
  CHECK_EQ(waveform_input.size(), FrameSize());
  CHECK_EQ(waveform_output.size(), FrameSize());

  windowed_fft_->ForwardTransform(waveform_input, internal_input_buffer_);

  internal_voice_isolation_->ProcessAudio(internal_input_buffer_,
                                          internal_output_buffer_);

  windowed_fft_->InverseTransform(internal_output_buffer_, waveform_output);
}

size_t StftVoiceIsolation::FrameSize() const {
  return fft_size_;
}

size_t StftVoiceIsolation::FramesPerSecond() const {
  return internal_voice_isolation_->FramesPerSecond();
}

}  // namespace media
