// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_CALCULATORS_DOWNSCALE_CALCULATOR_WEBGPU_H_
#define SERVICES_VIDEO_EFFECTS_CALCULATORS_DOWNSCALE_CALCULATOR_WEBGPU_H_

#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_base.h"

namespace mediapipe {
class CalculatorContract;
class CalculatorContext;
}  // namespace mediapipe

namespace video_effects {

class DownscaleCalculatorWebGpu : public mediapipe::CalculatorBase {
 public:
  static constexpr char kCalculatorName[] = "DownscaleCalculatorWebGpu";

  // Tag for the input stream that will carry `mediapipe::GpuBuffer`s with the
  // source for the copy.
  static constexpr char kInputStreamTag[] = "TEXTURE_IN";
  // Tag for the output stream that will carry `mediapipe::GpuBuffer`s with the
  // result of the downscaling operation.
  static constexpr char kOutputStreamTag[] = "TEXTURE_OUT";

  DownscaleCalculatorWebGpu();
  ~DownscaleCalculatorWebGpu() override;

  static absl::Status GetContract(mediapipe::CalculatorContract* cc);

  absl::Status Open(mediapipe::CalculatorContext* cc) override;
  absl::Status Process(mediapipe::CalculatorContext* cc) override;
  absl::Status Close(mediapipe::CalculatorContext* cc) override;

 private:
  wgpu::Device device_;
  wgpu::Buffer texture_copy_uniforms_buffer_;
  wgpu::RenderPipeline render_pipeline_;
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_CALCULATORS_DOWNSCALE_CALCULATOR_WEBGPU_H_
