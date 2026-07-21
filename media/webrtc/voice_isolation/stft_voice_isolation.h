// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_STFT_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_STFT_VOICE_ISOLATION_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/pffft/src/pffft.h"

namespace media {

class WindowedFft;

// Creates a VoiceIsolation that accepts a wave signal and transforms it using
// the Fourier transform. The size of the DFT is equal to the number of input
// frames. It performs two STFT operations with a hop size of half the input
// size. The second half of the previous call is cached.
class COMPONENT_EXPORT(MEDIA_WEBRTC) StftVoiceIsolation
    : public VoiceIsolationComponent {
 public:
  explicit StftVoiceIsolation(
      std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation);
  ~StftVoiceIsolation() override;

  void ProcessAudio(base::span<const float> input,
                    base::span<float> output) override;

  size_t FrameSize() const override;

  size_t FramesPerSecond() const override;

 private:
  const size_t fft_size_;
  std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation_;
  std::unique_ptr<WindowedFft> windowed_fft_;
  base::AlignedHeapArray<float> internal_input_buffer_;
  base::AlignedHeapArray<float> internal_output_buffer_;
};

// Class to compute two consecutive STFTs of fft_size with a hop size of
// fft_size / 2. It uses a Hann window with the same size of the FFT and
// introduces an algorithmic delay of fft_size / 2 samples (zero-padding) to
// reconstruct the signal in time domain.
class COMPONENT_EXPORT(MEDIA_WEBRTC) WindowedFft {
 public:
  explicit WindowedFft(int fft_size);
  ~WindowedFft();

  void ForwardTransform(base::span<const float> input_audio,
                        base::span<float> output_dfts);
  void InverseTransform(base::span<float> input_dfts,
                        base::span<float> output_audio);

 private:
  const unsigned int fft_size_;
  const std::vector<float> fft_window_;
  const std::vector<float> inv_fft_window_;
  base::AlignedHeapArray<float> forward_buffer_;
  base::AlignedHeapArray<float> temp_buffer_;
  base::AlignedHeapArray<float> inverse_buffer_;
  std::unique_ptr<PFFFT_Setup, decltype(&pffft_destroy_setup)> fft_state_;
  // pffft requires memory to work to avoid using the stack.
  base::AlignedHeapArray<float> fft_workplace_;
  FRIEND_TEST_ALL_PREFIXES(VoiceIsolationWindowedFftTest, WindowOlaProperty);
};

}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_STFT_VOICE_ISOLATION_H_
