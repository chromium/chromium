// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_SPECTRAL_VOICE_ISOLATION_COMPONENT_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_SPECTRAL_VOICE_ISOLATION_COMPONENT_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"

namespace media {

// A voice isolation component that processes DFTs and also reports a speech
// mask: for each frequency bin, the fraction of the input magnitude that it
// estimates to be speech. Components that wrap it can use the mask to process
// content that the denoiser does not see, such as the band above 8 kHz when
// the denoiser runs at 16 kHz.
class COMPONENT_EXPORT(MEDIA_WEBRTC) SpectralVoiceIsolationComponent
    : public VoiceIsolationComponent {
 public:
  // Returns the speech mask of the last ProcessAudio() call for the first of
  // its input DFTs: one value in [0, 1] per frequency bin, from DC to Nyquist,
  // which is `dft_size / 2 + 1` values for DFTs of `dft_size` floats. All
  // values are 1 before the first call and after ClearBuffers(). The span has
  // a constant size and stays valid for the lifetime of the component;
  // ProcessAudio() and ClearBuffers() update the values that it points to.
  //
  // Timing offset: Because the mask is estimated over the window of the first
  // input DFT of the call, it lags the second input DFT of the call by half a
  // frame (10 ms at 50 calls per second with 50%-overlapping 20 ms windows).
  // TODO(crbug.com/572657351): Estimate a speech mask for the second input DFT
  // as well so that callers do not use a mask that lags the second DFT by 10
  // ms.
  //
  // Boundary limitations:
  // - DC and Nyquist (bins 0 and `dft_size / 2`, i.e. 160 for 16 kHz DFTs):
  //   In TfLiteVoiceIsolation, these bins bypass the model and are copied from
  //   `input_dfts`, so `SpeechMask()[0]` and `SpeechMask()[dft_size / 2]` after
  //   overlap-add and Hann reanalysis only measure unit gain mixed with window
  //   leakage from adjacent bins.
  // - Top bins near 8 kHz (bins 158..160) under BandSplitVoiceIsolation:
  //   SpeechMaskEstimator assumes a consistent 16 kHz Hann STFT, whereas
  //   BandSplitVoiceIsolation truncates 48 kHz Hann DFTs at 8 kHz and puts the
  //   real part of the 8 kHz bin into the 16 kHz Nyquist slot. Within the Hann
  //   main lobe of the 8 kHz cut (bins 158..160, 7.9..8.0 kHz), overlap-add
  //   followed by Hann reanalysis is not an identity transform: for an
  //   identity denoiser on 48 kHz white noise, the mask stays >= 0.999 across
  //   bins 1..152 (up to 7.6 kHz) but drops to ~0.7 at bins 158..159 and down
  //   to ~0.18 at bin 160. Callers should therefore stay at or below ~7.6 kHz
  //   (bin 152).
  // TODO(crbug.com/572659339): Fix the boundary limitations of the speech mask
  // at DC, Nyquist, and the top bins near the 8 kHz band-split cut.
  virtual base::span<const float> SpeechMask() const = 0;
};

}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_SPECTRAL_VOICE_ISOLATION_COMPONENT_H_
