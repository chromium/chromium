// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/tflite_voice_isolation.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"
#endif

#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter_builder.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/kernel_util.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/register.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"
#include "third_party/tflite/src/tensorflow/lite/op_resolver.h"

namespace media {
namespace {

constexpr int64_t kModelInputSize = 640;

bool ValidateModelInterpreter(tflite::Interpreter* interpreter) {
  if (!interpreter) {
    return false;
  }
  if (interpreter->inputs().size() != 1) {
    return false;
  }
  CHECK_EQ(tflite::NumElements(interpreter->tensor(interpreter->inputs()[0])),
           kModelInputSize);
  if (!interpreter->typed_input_tensor<float>(0)) {
    return false;
  };

  if (interpreter->outputs().size() != 1) {
    return false;
  }
  CHECK_EQ(tflite::NumElements(interpreter->tensor(interpreter->outputs()[0])),
           2 * kModelInputSize);
  if (!interpreter->typed_output_tensor<float>(0)) {
    return false;
  };

  CHECK_NE(interpreter->typed_input_tensor<float>(0),
           interpreter->typed_output_tensor<float>(0));
  return true;
}

base::span<float> WrapInputTensor(tflite::Interpreter* interpreter,
                                  size_t index) {
  CHECK(interpreter);
  CHECK_LT(index, interpreter->inputs().size());
  float* ptr = interpreter->typed_input_tensor<float>(index);
  CHECK(ptr);
  auto num_elements =
      tflite::NumElements(interpreter->tensor(interpreter->inputs()[index]));
  CHECK_GT(num_elements, 0);
  CHECK_LT(num_elements, 100000);
  // SAFETY: The TfLite interpreter allocates this memory to store the input and
  // output tensors after a call to AllocateTensors that we already called in
  // TfLiteVoiceIsolation::MaybeCreate.
  return UNSAFE_BUFFERS(base::span(ptr, static_cast<size_t>(num_elements)));
}

base::span<float> WrapOutputTensor(tflite::Interpreter* interpreter,
                                   size_t index) {
  CHECK(interpreter);
  CHECK_LT(index, interpreter->outputs().size());
  float* ptr = interpreter->typed_output_tensor<float>(index);
  CHECK(ptr);
  auto num_elements =
      tflite::NumElements(interpreter->tensor(interpreter->outputs()[index]));
  CHECK_GT(num_elements, 0);
  CHECK_LT(num_elements, 100000);
  // SAFETY: The TfLite interpreter allocates this memory to store the input and
  // output tensors after a call to AllocateTensors that we already called in
  // TfLiteVoiceIsolation::MaybeCreate.
  return UNSAFE_BUFFERS(base::span(ptr, static_cast<size_t>(num_elements)));
}
}  // namespace

TfLiteVoiceIsolation::TfLiteVoiceIsolation(
    std::unique_ptr<tflite::Interpreter> interpreter)
    : interpreter_(std::move(interpreter)),
      input_tensor_span_(WrapInputTensor(interpreter_.get(), /*index=*/0)),
      output_tensor_span_(WrapOutputTensor(interpreter_.get(), /*index=*/0)),
      frame_size_(input_tensor_span_.size()) {
  CHECK_EQ(interpreter_->inputs().size(), 1u);
  CHECK_EQ(interpreter_->outputs().size(), 1u);
  const size_t output_elements = output_tensor_span_.size();
  bias_.resize(output_elements);
  temp_output_.resize(output_elements);
  CHECK_EQ(frame_size_ * 2, output_elements);
  CHECK_NE(interpreter_->typed_input_tensor<float>(0),
           interpreter_->typed_output_tensor<float>(0));
  DVLOG(1) << "TfLiteVoiceIsolation frame_size=" << frame_size_;

  // Invoke once with zeros to get the model bias output.
  std::fill(input_tensor_span_.begin(), input_tensor_span_.end(), 0.0f);
  CHECK_EQ(interpreter_->Invoke(), kTfLiteOk);
  base::span(bias_).copy_from_nonoverlapping(output_tensor_span_);
}

std::unique_ptr<TfLiteVoiceIsolation> TfLiteVoiceIsolation::MaybeCreate(
    const tflite::FlatBufferModel* model) {
  CHECK(model);
  optimization_guide::TFLiteOpResolver op_resolver;

  std::unique_ptr<tflite::Interpreter> interpreter;
  auto builder = tflite::InterpreterBuilder(*model, op_resolver);
  if (builder(&interpreter) != kTfLiteOk) {
    return nullptr;
  }

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  // This code is not strictly necessary to run but speeds up execution. We want
  // to use it if available.
  TfLiteXNNPackDelegateOptions opts = TfLiteXNNPackDelegateOptionsDefault();
  opts.num_threads = 1;
  opts.flags |= TFLITE_XNNPACK_DELEGATE_FLAG_ENABLE_LATEST_OPERATORS;

  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(&opts),
                       TfLiteXNNPackDelegateDelete);

  if (interpreter->ModifyGraphWithDelegate(std::move(xnnpack_delegate)) !=
      kTfLiteOk) {
    return nullptr;
  }
#endif

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    return nullptr;
  }

  if (ValidateModelInterpreter(interpreter.get())) {
    return base::WrapUnique(new TfLiteVoiceIsolation(std::move(interpreter)));
  }
  return nullptr;
}

TfLiteVoiceIsolation::~TfLiteVoiceIsolation() = default;

void TfLiteVoiceIsolation::ProcessAudio(base::span<const float> input_dfts,
                                        base::span<float> output_dfts) {
  CHECK_EQ(input_dfts.size(), FrameSize());
  CHECK_EQ(output_dfts.size(), FrameSize());

  constexpr size_t num_ffts = 2;
  const size_t fft_size = input_dfts.size() / num_ffts;
  constexpr size_t kExpectedFftSize = 320;
  CHECK_EQ(fft_size, kExpectedFftSize);

  // Pack input: move over frequency bins to the inference buffer. Offset by 2
  // since DC and Nyquist component are moved out of the buffer already. The
  // model takes size `num_ffts * fft_size`. We pad the omitted bins with 0.
  for (size_t i = 0; i < num_ffts; ++i) {
    const auto input_slice = input_dfts.subspan(i * fft_size + 2, fft_size - 2);
    auto tensor_slice = input_tensor_span_.subspan(i * fft_size, fft_size - 2);
    CHECK_EQ(input_slice.size(), tensor_slice.size());

    tensor_slice.copy_from(input_slice);

    input_tensor_span_[(i + 1) * fft_size - 2] = 0.0f;
    input_tensor_span_[(i + 1) * fft_size - 1] = 0.0f;
  }

  CHECK_EQ(interpreter_->Invoke(), kTfLiteOk);
  base::span(temp_output_).copy_from(output_tensor_span_);

  for (size_t k = 0; k < temp_output_.size(); ++k) {
    temp_output_[k] -= bias_[k];
  }

  const size_t num_output_channels =
      temp_output_.size() / (num_ffts * fft_size);

  const float dereverb_weight = 1.0f;

  CHECK_EQ(num_ffts, 2u);
  CHECK_EQ(num_output_channels, 2u);
  CHECK_GE(fft_size, 2u);

  for (size_t i = 0; i < num_ffts; ++i) {
    const int frame_index = i * fft_size * num_output_channels;
    const auto input_slice = input_dfts.subspan(i * fft_size, fft_size);
    const auto output_slice = output_dfts.subspan(i * fft_size, fft_size);
    CHECK_EQ(output_dfts.size(), static_cast<size_t>(fft_size * 2));

    // Restore the DC and Nyquist components from the input.
    output_slice[0] = input_slice[0];
    output_slice[1] = input_slice[1];

    for (unsigned int j = 0; j < fft_size - 2; j += 2) {
      // Channel 0 contains the dereverbed and denoised signal.
      output_slice[j + 2] =
          dereverb_weight * temp_output_[frame_index + num_output_channels * j];
      output_slice[j + 3] =
          dereverb_weight *
          temp_output_[frame_index + num_output_channels * j + 1];
    }
  }
}

size_t TfLiteVoiceIsolation::FrameSize() const {
  return frame_size_;
}

size_t TfLiteVoiceIsolation::FramesPerSecond() const {
  return 50;
}

}  // namespace media
