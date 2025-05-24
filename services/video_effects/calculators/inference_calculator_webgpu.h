// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_CALCULATORS_INFERENCE_CALCULATOR_WEBGPU_H_
#define SERVICES_VIDEO_EFFECTS_CALCULATORS_INFERENCE_CALCULATOR_WEBGPU_H_

#include "services/on_device_model/ml/chrome_ml_api.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_base.h"

namespace mediapipe {
class CalculatorContract;
class CalculatorContext;
}  // namespace mediapipe

namespace video_effects {

// Runs ML inference on the provided input frame, using the model configured
// by the `video_effects::StaticConfig`.
//
// Side packets:
// 0) video_effects::StaticConfig to be used when initializing the calculator.
//
// Inputs:
// 0) `video_effects::RuntimeConfig` with the configuration to be used for
//    processing the input frame.
// 1) `mediapipe::GpuBuffer` with the input video frame.
//
// Outputs:
// 0) `mediapipe::GpuBuffer` with the output frame.
class InferenceCalculatorWebGpu : public mediapipe::CalculatorBase {
 public:
  static constexpr char kCalculatorName[] = "InferenceCalculatorWebGpu";

  static constexpr char kStaticConfigInputSidePacketStreamTag[] =
      "STATIC_CONFIG";
  static constexpr char kRuntimeConfigInputStreamTag[] = "RUNTIME_CONFIG";
  static constexpr char kInputTextureStreamTag[] = "TEXTURE_IN";
  static constexpr char kOutputTextureStreamTag[] = "TEXTURE_OUT";

  InferenceCalculatorWebGpu();
  ~InferenceCalculatorWebGpu() override;

  static absl::Status GetContract(mediapipe::CalculatorContract* cc);

  absl::Status Open(mediapipe::CalculatorContext* cc) override;
  absl::Status Process(mediapipe::CalculatorContext* cc) override;
  absl::Status Close(mediapipe::CalculatorContext* cc) override;

 private:
  // Handle to the inference engine used for running the model. It will be set
  // in `Open()` and reset in `Close()`.
  ChromeMLInferenceEngine inference_engine_;
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_CALCULATORS_INFERENCE_CALCULATOR_WEBGPU_H_
