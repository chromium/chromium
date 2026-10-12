// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_TFLITE_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_TFLITE_VOICE_ISOLATION_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_span.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "media/webrtc/voice_isolation/spectral_voice_isolation_component.h"
#include "media/webrtc/voice_isolation/speech_mask_estimator.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {

// Denoiser using a TfLite model internally. Ideally the provided model will
// remove all noise in the input signal preserving only human speech. The model
// has two outputs: the dereverberated output (noise and reverberation
// removed), which ProcessAudio() returns, and the denoising-only output (noise
// removed, reverberation kept), from which the speech mask that SpeechMask()
// returns is estimated.
class COMPONENT_EXPORT(MEDIA_WEBRTC) TfLiteVoiceIsolation
    : public SpectralVoiceIsolationComponent {
 public:
  ~TfLiteVoiceIsolation() override;

  // This method takes a stack of two complex DFT coefficients of 20ms each with
  // a 50% overlap.
  void ProcessAudio(base::span<const float> input_dfts,
                    base::span<float> output_dfts) override;

  size_t FrameSize() const override;
  size_t FramesPerSecond() const override;
  void ClearBuffers() override;
  base::TimeDelta AlgorithmicDelay() const override;

  // 161 values, one per bin of the 16kHz DFTs, from DC to Nyquist. Estimated
  // over the window of the first input DFT of the call (so it lags the second
  // DFT by 10 ms); see SpectralVoiceIsolationComponent::SpeechMask() for the
  // boundary limitations at DC (bin 0), Nyquist (bin 160), and the top bins
  // near 8 kHz (bins 158..160).
  base::span<const float> SpeechMask() const override;

  // `model` needs to outlive this TfLiteVoiceIsolation instance.
  static base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
                        VoiceIsolationCreationResult>
  MaybeCreate(const tflite::FlatBufferModel* model);

 private:
  TfLiteVoiceIsolation(std::unique_ptr<tflite::Interpreter> interpreter,
                       std::vector<float> bias);

  std::unique_ptr<tflite::Interpreter> interpreter_;
  base::raw_span<float> input_tensor_span_;
  base::raw_span<float> output_tensor_span_;
  std::vector<float> bias_;
  std::vector<float> temp_output_;

  // Denoising-only output of the last model call, unpacked into two 16-byte
  // aligned DFTs for SpeechMaskEstimator::Estimate().
  base::AlignedHeapArray<float> mask_estimation_dfts_;
  SpeechMaskEstimator speech_mask_estimator_;
};
}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_TFLITE_VOICE_ISOLATION_H_
