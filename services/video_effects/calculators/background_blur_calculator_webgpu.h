// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_CALCULATORS_BACKGROUND_BLUR_CALCULATOR_WEBGPU_H_
#define SERVICES_VIDEO_EFFECTS_CALCULATORS_BACKGROUND_BLUR_CALCULATOR_WEBGPU_H_

#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_base.h"

namespace mediapipe {
class CalculatorContract;
class CalculatorContext;
}  // namespace mediapipe

namespace video_effects {

class BackgroundBlurCalculatorWebGpu : public mediapipe::CalculatorBase {
 public:
  static constexpr char kCalculatorName[] = "BackgroundBlurCalculatorWebGpu";

  static constexpr char kRuntimeConfigInputStreamTag[] = "RUNTIME_CONFIG";
  static constexpr char kInputTextureStreamTag[] = "TEXTURE_IN";
  static constexpr char kMaskTextureStreamTag[] = "MASK_IN";
  static constexpr char kOutputTextureStreamTag[] = "TEXTURE_OUT";

  BackgroundBlurCalculatorWebGpu();
  ~BackgroundBlurCalculatorWebGpu() override;

  static absl::Status GetContract(mediapipe::CalculatorContract* cc);

  absl::Status Open(mediapipe::CalculatorContext* cc) override;
  absl::Status Process(mediapipe::CalculatorContext* cc) override;
  absl::Status Close(mediapipe::CalculatorContext* cc) override;

 private:
  wgpu::ComputePipeline compute_pipeline_;
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_CALCULATORS_BACKGROUND_BLUR_CALCULATOR_WEBGPU_H_
