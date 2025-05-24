// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_CALCULATORS_IN_PLACE_COPY_CALCULATOR_WEBGPU_H_
#define SERVICES_VIDEO_EFFECTS_CALCULATORS_IN_PLACE_COPY_CALCULATOR_WEBGPU_H_

#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_base.h"

namespace mediapipe {
class CalculatorContract;
class CalculatorContext;
}  // namespace mediapipe

namespace video_effects {

// Calculator that performs a copy of the texture provided in one of the input
// streams, with the destination being provided in another _input_ stream. The
// output of the operation will then be sent to the output stream.
class InPlaceCopyCalculatorWebGpu : public mediapipe::CalculatorBase {
 public:
  static constexpr char kCalculatorName[] = "InPlaceCopyCalculatorWebGpu";

  // Tag for the input stream that will carry `mediapipe::GpuBuffer`s with the
  // source for the copy.
  static constexpr char kInputStreamTag[] = "TEXTURE_IN";
  // Tag for the input stream that will carry `mediapipe::GpuBuffer`s with the
  // destination for the copy.
  static constexpr char kOutputInputStreamTag[] = "TEXTURE_OUT_IN";
  // Tag for the output stream that will carry `mediapipe::GpuBuffer`s with the
  // result of the copy. Those will be the same buffers that were passed in to
  // the `kOutputInputStreamTag` stream.
  static constexpr char kOutputStreamTag[] = "TEXTURE_OUT";

  InPlaceCopyCalculatorWebGpu();
  ~InPlaceCopyCalculatorWebGpu() override;

  static absl::Status GetContract(mediapipe::CalculatorContract* cc);

  absl::Status Open(mediapipe::CalculatorContext* cc) override;
  absl::Status Process(mediapipe::CalculatorContext* cc) override;
  absl::Status Close(mediapipe::CalculatorContext* cc) override;
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_CALCULATORS_IN_PLACE_COPY_CALCULATOR_WEBGPU_H_
