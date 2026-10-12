// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_SPEECH_MASK_ESTIMATOR_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_SPEECH_MASK_ESTIMATOR_H_

#include <cstddef>

#include "base/component_export.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "media/webrtc/voice_isolation/stft_voice_isolation.h"

namespace media {

// Estimates the speech mask of a denoiser that processes two 50%-overlapping
// DFTs per call, like TfLiteVoiceIsolation: per frequency bin, the magnitude
// of the denoised spectrum divided by that of the noisy input, capped at 1.
//
// The denoised DFTs are overlap-added first, and the resulting waveform is
// analyzed again with a single Hann-windowed DFT. Overlap-add cancels the
// parts of the per-DFT denoiser outputs that the overlapping windows do not
// agree on, which a ratio of the raw denoised DFTs would count as speech. The
// waveform spans the window of the first DFT of the call, so the mask belongs
// to the first input DFT and adds no delay.
//
// Boundary limitations: This estimator assumes that `input_dfts` and
// `denoised_dfts` come from a consistent Hann-windowed STFT with 50% overlap.
// When DC and Nyquist bypass the denoiser and are copied from `input_dfts`,
// `mask()[0]` and `mask()[dft_size / 2]` after overlap-add and Hann reanalysis
// only measure unit gain mixed with window leakage from adjacent bins. In
// addition, when the DFTs are produced by truncating a wider-band Hann STFT
// (as BandSplitVoiceIsolation does at 8 kHz, placing the real part of the
// 8 kHz bin into the 16 kHz Nyquist slot), overlap-add followed by Hann
// reanalysis is not an identity transform within the Hann main lobe of the
// 8 kHz cut (bins 158..160 for 320-point 16 kHz DFTs): for an identity
// denoiser on 48 kHz white noise, the mask stays >= 0.999 across bins 1..152
// (up to 7.6 kHz) but drops to ~0.7 at bins 158..159 and down to ~0.18 at
// bin 160. Callers under BandSplitVoiceIsolation should therefore stay at or
// below ~7.6 kHz (bin 152).
// TODO(crbug.com/572659339): Fix the boundary limitations of the speech mask at
// DC, Nyquist, and the top bins near the 8 kHz band-split cut.
class COMPONENT_EXPORT(MEDIA_WEBRTC) SpeechMaskEstimator {
 public:
  // Byte alignment required for `denoised_dfts.data()` in Estimate(), matching
  // the PFFFT (SSE) alignment requirement of WindowedFft::InverseTransform().
  static constexpr size_t kDftAlignment = 16;

  // `dft_size` is the number of samples of each DFT window, which is also the
  // number of floats of each DFT in PFFFT ordered real layout.
  explicit SpeechMaskEstimator(size_t dft_size);
  ~SpeechMaskEstimator();

  SpeechMaskEstimator(const SpeechMaskEstimator&) = delete;
  SpeechMaskEstimator& operator=(const SpeechMaskEstimator&) = delete;

  // Updates mask(). `input_dfts` holds the two noisy DFTs of a call and
  // `denoised_dfts` the two matching denoised DFTs, `2 * dft_size` floats each.
  // `denoised_dfts.data()` must be aligned to `kDftAlignment` bytes.
  // Consecutive calls must pass consecutive DFTs, since overlap-add carries the
  // second half of the last denoised DFT over to the next call.
  void Estimate(base::span<const float> input_dfts,
                base::span<const float> denoised_dfts);

  // Returns the mask of the last Estimate() call: `dft_size / 2 + 1` values in
  // [0, 1], from DC to Nyquist. All ones before the first call and after
  // Clear(). The span stays valid for the lifetime of this object.
  base::span<const float> mask() const { return mask_; }

  // Clears the overlap-add history and resets mask() to all ones.
  void Clear();

 private:
  const size_t dft_size_;

  // Overlap-adds the denoised DFTs and analyzes the result again.
  WindowedFft windowed_fft_;

  // Denoised waveform over the window of the first DFT: `dft_size` samples.
  base::HeapArray<float> denoised_waveform_;

  // Output of WindowedFft::ForwardTransform(); its second DFT is the DFT of
  // `denoised_waveform_`.
  base::HeapArray<float> reanalyzed_dfts_;

  base::HeapArray<float> mask_;
};

}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_SPEECH_MASK_ESTIMATOR_H_
