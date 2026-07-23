// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_TFLITE_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_TFLITE_VOICE_ISOLATION_H_

#include <cstddef>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_span.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {

// Denoiser using a TfLite model internally. Ideally the provided model will
// remove all noise in the input signal preserving only human speech.
class COMPONENT_EXPORT(MEDIA_WEBRTC) TfLiteVoiceIsolation
    : public VoiceIsolationComponent {
 public:
  ~TfLiteVoiceIsolation() override;

  // This method takes a stack of two complex DFT coefficients of 20ms each with
  // a 50% overlap.
  void ProcessAudio(base::span<const float> input_dfts,
                    base::span<float> output_dfts) override;

  size_t FrameSize() const override;
  size_t FramesPerSecond() const override;

  // `model` needs to outlive this TfLiteVoiceIsolation instance.
  static std::unique_ptr<TfLiteVoiceIsolation> MaybeCreate(
      const tflite::FlatBufferModel* model);

 private:
  explicit TfLiteVoiceIsolation(
      std::unique_ptr<tflite::Interpreter> interpreter);

  std::unique_ptr<tflite::Interpreter> interpreter_;
  base::raw_span<float> input_tensor_span_;
  base::raw_span<float> output_tensor_span_;
  std::vector<float> bias_;
  std::vector<float> temp_output_;
  const size_t frame_size_;
};
}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_TFLITE_VOICE_ISOLATION_H_
