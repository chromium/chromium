// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/tflite_voice_isolation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"
#endif

#include "third_party/tflite/src/tensorflow/lite/builtin_ops.h"
#include "third_party/tflite/src/tensorflow/lite/core/c/builtin_op_data.h"
#include "third_party/tflite/src/tensorflow/lite/core/c/common.h"
#include "third_party/tflite/src/tensorflow/lite/core/subgraph.h"
// TODO(crbug.com/575409069): Add a LiteRT shim for resource_variable.h and
// prepend //third_party/tflite:tflite_shim_config in BUILD.gn when
// use_litert_tflite = true is supported.
#include "third_party/tflite/src/tensorflow/lite/experimental/resource/resource_variable.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter_builder.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/kernel_util.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"
#include "third_party/tflite/src/tensorflow/lite/op_resolver.h"

namespace media {
namespace {

// Calls per second of `TfLiteVoiceIsolation::ProcessAudio()` (20 ms frames).
constexpr size_t kFramesPerSecond = 50;

// Reporting interval and histogram bounds for model inference execution timing.
constexpr size_t kMetricsReportingIntervalSeconds = 10;
constexpr size_t kInferencesPerLogPeriod =
    kMetricsReportingIntervalSeconds * kFramesPerSecond;
constexpr base::TimeDelta kInferenceTimeHistogramMin = base::Microseconds(50);
constexpr base::TimeDelta kInferenceTimeHistogramMax = base::Milliseconds(20);
constexpr size_t kInferenceTimeHistogramBuckets = 50;

// The model processes two 16kHz DFTs (20 ms each) per call.
constexpr size_t kNumFfts = 2;

// Floats per 16kHz DFT.
constexpr size_t kExpectedFftSize = 320;

// Floats of the model input tensor: all DFTs of a call.
constexpr size_t kFrameSize = kNumFfts * kExpectedFftSize;
constexpr int64_t kModelInputSize = static_cast<int64_t>(kFrameSize);
constexpr size_t kModelInputBytes = kFrameSize * sizeof(float);
constexpr size_t kModelOutputBytes = 2 * kModelInputBytes;

// The model output interleaves its two outputs for each frequency bin: the
// (re, im) pair of the dereverberated output, then the (re, im) pair of the
// denoising-only output.
constexpr size_t kDenoisingOnlyOutputOffset = 2;
// Upper bound on the number of elements of a model input or output tensor.
constexpr size_t kMaxTensorElements = 100000;

// Weights of the two model outputs in the output of ProcessAudio(). Mostly the
// dereverberated output, plus a little of the denoising-only output, whose
// room reverberation makes the speech sound more natural than the fully
// dereverberated output.
constexpr float kDereverbWeight = 0.9f;
constexpr float kDenoisingOnlyWeight = 1.0f - kDereverbWeight;

// Returns true if `subgraph` contains any control-flow operator other than
// `CALL_ONCE`. See the model requirements on
// `TfLiteVoiceIsolation::MaybeCreate()`.
bool HasControlFlowOpsOtherThanCallOnce(const tflite::Subgraph& subgraph) {
  for (size_t node_index = 0; node_index < subgraph.nodes_size();
       ++node_index) {
    const std::pair<TfLiteNode, TfLiteRegistration>* node_and_registration =
        subgraph.node_and_registration(base::checked_cast<int>(node_index));
    CHECK(node_and_registration);
    switch (node_and_registration->second.builtin_code) {
      case kTfLiteBuiltinCall:
      case kTfLiteBuiltinIf:
      case kTfLiteBuiltinWhile:
      case kTfLiteBuiltinStablehloWhile:
      case kTfLiteBuiltinStablehloCase:
      case kTfLiteBuiltinStablehloComposite:
        return true;
      default:
        break;
    }
  }
  return false;
}

// Checks the primary subgraph and input/output tensor requirements listed on
// `TfLiteVoiceIsolation::MaybeCreate()`.
bool ValidateModelInterpreter(tflite::Interpreter* interpreter) {
  if (!interpreter) {
    return false;
  }
  tflite::Subgraph& primary = interpreter->primary_subgraph();
  if (primary.HasDynamicTensors() ||
      HasControlFlowOpsOtherThanCallOnce(primary)) {
    return false;
  }
  if (interpreter->inputs().size() != 1 ||
      !interpreter->typed_input_tensor<float>(0)) {
    return false;
  }
  const TfLiteTensor* input_tensor =
      interpreter->tensor(interpreter->inputs()[0]);
  if (!input_tensor || input_tensor->allocation_type != kTfLiteArenaRw ||
      tflite::NumElements(input_tensor) != kModelInputSize ||
      input_tensor->bytes != kModelInputBytes) {
    return false;
  }

  if (interpreter->outputs().size() != 1 ||
      !interpreter->typed_output_tensor<float>(0)) {
    return false;
  }
  const TfLiteTensor* output_tensor =
      interpreter->tensor(interpreter->outputs()[0]);
  if (!output_tensor || output_tensor->allocation_type != kTfLiteArenaRw ||
      tflite::NumElements(output_tensor) != 2 * kModelInputSize ||
      output_tensor->bytes != kModelOutputBytes) {
    return false;
  }

  if (interpreter->typed_input_tensor<float>(0) ==
      interpreter->typed_output_tensor<float>(0)) {
    return false;
  }
  return true;
}

// Wraps the data buffer of `tensor` as a span of `T` elements (`T` may be
// const-qualified).
template <typename T>
base::span<T> WrapTensor(
    std::conditional_t<std::is_const_v<T>, const TfLiteTensor, TfLiteTensor>*
        tensor) {
  CHECK(tensor);
  CHECK(tensor->data.raw);
  CHECK_EQ(tensor->bytes % sizeof(T), 0u);

  // SAFETY: `tensor->data.raw` points to `tensor->bytes` valid bytes owned by
  // the tensor, allocated during model initialization or subgraph execution.
  return UNSAFE_BUFFERS(base::span(reinterpret_cast<T*>(tensor->data.raw),
                                   tensor->bytes / sizeof(T)));
}

// Wraps a float32 model input or output tensor, validating its size.
base::span<float> WrapFloatTensor(TfLiteTensor* tensor) {
  CHECK(tensor);
  CHECK_EQ(tensor->type, kTfLiteFloat32);

  const base::span<float> tensor_span = WrapTensor<float>(tensor);
  CHECK_EQ(tensor_span.size(),
           base::checked_cast<size_t>(tflite::NumElements(tensor)));
  CHECK_GT(tensor_span.size(), 0u);
  CHECK_LT(tensor_span.size(), kMaxTensorElements);
  return tensor_span;
}

base::span<float> WrapInputTensor(tflite::Interpreter* interpreter,
                                  size_t index) {
  CHECK(interpreter);
  CHECK_LT(index, interpreter->inputs().size());
  return WrapFloatTensor(interpreter->input_tensor(index));
}

base::span<float> WrapOutputTensor(tflite::Interpreter* interpreter,
                                   size_t index) {
  CHECK(interpreter);
  CHECK_LT(index, interpreter->outputs().size());
  return WrapFloatTensor(interpreter->output_tensor(index));
}

// Returns true if every byte of `tensor` is zero.
bool IsAllZeros(const TfLiteTensor& tensor) {
  return std::ranges::all_of(WrapTensor<const uint8_t>(&tensor),
                             [](uint8_t byte) { return byte == 0; });
}

// Fills every byte of `tensor` with `value`. Uses `std::fill_n` on the raw
// pointer rather than `std::ranges::fill` on `base::span` because libc++'s
// `std::ranges::fill` does not unwrap `base::CheckedContiguousIterator` via
// `std::to_address()`, preventing `memset` lowering on the real-time thread.
void FillTensorBytes(TfLiteTensor* tensor, uint8_t value) {
  const base::span<uint8_t> bytes = WrapTensor<uint8_t>(tensor);
  std::fill_n(bytes.data(), bytes.size(), value);
}

// Returns the indices of the subgraphs invoked by CallOnce ops in the primary
// subgraph. These subgraphs initialize the model resource variables.
// Voice Isolation models only define CallOnce initialization ops in the
// primary subgraph, so other subgraphs are not scanned.
std::vector<int> FindInitSubgraphs(tflite::Interpreter* interpreter) {
  CHECK(interpreter);
  std::vector<int> init_subgraphs;
  const tflite::Subgraph& primary = interpreter->primary_subgraph();
  for (size_t node_index = 0; node_index < primary.nodes_size(); ++node_index) {
    const std::pair<TfLiteNode, TfLiteRegistration>* node_and_registration =
        primary.node_and_registration(base::checked_cast<int>(node_index));
    CHECK(node_and_registration);
    if (node_and_registration->second.builtin_code != kTfLiteBuiltinCallOnce) {
      continue;
    }

    const auto* params = static_cast<const TfLiteCallOnceParams*>(
        node_and_registration->first.builtin_data);
    CHECK(params);
    init_subgraphs.push_back(params->init_subgraph_index);
  }
  return init_subgraphs;
}

}  // namespace

TfLiteVoiceIsolation::TfLiteVoiceIsolation(
    std::unique_ptr<tflite::Interpreter> interpreter,
    std::vector<float> bias)
    : interpreter_(std::move(interpreter)),
      input_tensor_span_(WrapInputTensor(interpreter_.get(), /*index=*/0)),
      output_tensor_span_(WrapOutputTensor(interpreter_.get(), /*index=*/0)),
      bias_(std::move(bias)),
      denoising_only_dfts_(
          base::AlignedUninit<float>(kFrameSize,
                                     SpeechMaskEstimator::kDftAlignment)),
      speech_mask_estimator_(/*dft_size=*/kExpectedFftSize) {
  CHECK_EQ(input_tensor_span_.size(), kFrameSize);
  CHECK_EQ(interpreter_->inputs().size(), 1u);
  CHECK_EQ(interpreter_->outputs().size(), 1u);
  const size_t output_elements = output_tensor_span_.size();
  CHECK_EQ(bias_.size(), output_elements);
  temp_output_.resize(output_elements);
  CHECK_EQ(kFrameSize * 2, output_elements);
  CHECK_NE(interpreter_->typed_input_tensor<float>(0),
           interpreter_->typed_output_tensor<float>(0));
  DVLOG(1) << "TfLiteVoiceIsolation frame_size=" << kFrameSize;
}

bool TfLiteVoiceIsolation::InitResourceVariables() {
  TRACE_EVENT("audio", "TfLiteVoiceIsolation::InitResourceVariables");
  tflite::resource::ResourceMap& resources =
      interpreter_->primary_subgraph().resources();

  // Check the recurrent-state requirements on `MaybeCreate()`: record the
  // post-warmup byte size of every resource variable so we can verify that the
  // CallOnce initialization subgraphs leave each variable with the same byte
  // size, and pre-fill each variable with non-zero bytes so the post-init
  // `IsAllZeros()` check verifies that `init_subgraphs` wrote every byte.
  size_t total_bytes = 0;
  for (const auto& [resource_id, unused_resource] : resources) {
    tflite::resource::ResourceVariable* variable =
        tflite::resource::GetResourceVariable(&resources, resource_id);
    if (!variable) {
      continue;
    }

    TfLiteTensor* tensor = variable->GetTensor();
    if (!tensor || !tensor->data.raw || tensor->bytes == 0) {
      return false;
    }
    FillTensorBytes(tensor, uint8_t{0xFF});
    resource_variables_.push_back(
        {.variable = variable, .bytes = tensor->bytes});
    total_bytes += tensor->bytes;
  }

  // Run the initialization subgraphs the same way the CallOnce kernel does.
  // CallOnce already ran them in the warmup `Invoke()` of `MaybeCreate()` and
  // does not run them again. Running them here after filling each variable with
  // 0xFF verifies that they zero-initialize every resource variable and
  // restores the initial values.
  const std::vector<int> init_subgraphs = FindInitSubgraphs(interpreter_.get());
  for (int subgraph_index : init_subgraphs) {
    tflite::Subgraph* init_subgraph = interpreter_->subgraph(subgraph_index);
    if (subgraph_index <= 0 || !init_subgraph ||
        init_subgraph->AllocateTensors() != kTfLiteOk ||
        init_subgraph->Invoke() != kTfLiteOk ||
        init_subgraph->ReleaseNonPersistentMemory() != kTfLiteOk) {
      return false;
    }
  }

  // Resource variables are shared by all subgraphs. Verify once that every
  // resource variable has its post-warmup byte size and was overwritten with
  // zeros by `init_subgraphs`.
  for (const ResourceVariableState& state : resource_variables_) {
    const TfLiteTensor* tensor = state.variable->GetTensor();
    if (!tensor || !tensor->data.raw || tensor->bytes != state.bytes ||
        !IsAllZeros(*tensor)) {
      return false;
    }
  }
  TRACE_EVENT_INSTANT("audio", "TfLiteVoiceIsolation::InitialState",
                      "resource_variables", resource_variables_.size(),
                      "total_bytes", total_bytes);
  DVLOG(1) << "TfLiteVoiceIsolation resource variables="
           << resource_variables_.size();
  return true;
}

bool TfLiteVoiceIsolation::ResetState() {
  TRACE_EVENT("audio", "TfLiteVoiceIsolation::ResetState", "resource_variables",
              resource_variables_.size());
  // Restore the resource variables in place. `InitResourceVariables()` verified
  // that the initial and post-warmup byte sizes match, and
  // `ResourceVariable::AssignFrom()` during `Invoke()` reuses the existing
  // data buffer when the byte size is unchanged, so no data buffer is
  // allocated in `ResetState()` or `Invoke()`.
  for (const ResourceVariableState& state : resource_variables_) {
    TfLiteTensor* tensor = state.variable->GetTensor();
    if (!tensor || !tensor->data.raw || tensor->bytes != state.bytes) {
      return false;
    }
    FillTensorBytes(tensor, uint8_t{0});
  }

  // Models using `is_variable` tensors instead of resource variables; see the
  // recurrent-state requirements on `MaybeCreate()`. ResetVariableTensors()
  // zeroes internal variable tensor buffers via memset without allocating heap
  // memory or acquiring locks, making it safe for the real-time audio thread.
  return interpreter_->ResetVariableTensors() == kTfLiteOk;
}

base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
               VoiceIsolationCreationResult>
TfLiteVoiceIsolation::MaybeCreate(const tflite::FlatBufferModel* model) {
  CHECK(model);
  optimization_guide::TFLiteOpResolver op_resolver;

  std::unique_ptr<tflite::Interpreter> interpreter;
  auto builder = tflite::InterpreterBuilder(*model, op_resolver);
  if (builder(&interpreter) != kTfLiteOk) {
    return base::unexpected(
        VoiceIsolationCreationResult::kInterpreterCreationFailed);
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
    return base::unexpected(
        VoiceIsolationCreationResult::kDelegateCreationFailed);
  }
#endif

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    return base::unexpected(
        VoiceIsolationCreationResult::kTensorAllocationFailed);
  }

  if (!ValidateModelInterpreter(interpreter.get())) {
    return base::unexpected(VoiceIsolationCreationResult::kIncompatibleModel);
  }

  // Warmup run with zeros to compute model bias output and verify execution.
  base::span<float> input_span =
      WrapInputTensor(interpreter.get(), /*index=*/0);
  std::fill(input_span.begin(), input_span.end(), 0.0f);
  if (interpreter->Invoke() != kTfLiteOk) {
    return base::unexpected(VoiceIsolationCreationResult::kWarmupFailed);
  }

  // Re-validate after `Invoke()` in case an operator in the model resized or
  // reallocated the input or output tensors during warmup.
  if (!ValidateModelInterpreter(interpreter.get())) {
    return base::unexpected(VoiceIsolationCreationResult::kIncompatibleModel);
  }

  base::span<const float> output_span =
      WrapOutputTensor(interpreter.get(), /*index=*/0);
  std::vector<float> bias(output_span.begin(), output_span.end());

  std::unique_ptr<TfLiteVoiceIsolation> voice_isolation = base::WrapUnique(
      new TfLiteVoiceIsolation(std::move(interpreter), std::move(bias)));

  // Computing the bias advanced the model state. Verify that the initial
  // resource-variable state is all zeros, record the variables, and reset the
  // state so that the first processed frame starts from the same state as after
  // `ClearBuffers()`.
  if (!voice_isolation->InitResourceVariables() ||
      !voice_isolation->ResetState()) {
    return base::unexpected(VoiceIsolationCreationResult::kIncompatibleModel);
  }

  return voice_isolation;
}

TfLiteVoiceIsolation::~TfLiteVoiceIsolation() = default;

void TfLiteVoiceIsolation::ProcessAudio(base::span<const float> input_dfts,
                                        base::span<float> output_dfts) {
  TRACE_EVENT("audio", "TfLiteVoiceIsolation::ProcessAudio");
  CHECK_EQ(input_dfts.size(), FrameSize());
  CHECK_EQ(output_dfts.size(), FrameSize());

  // Pack input: move over frequency bins to the inference buffer. Offset by 2
  // since DC and Nyquist component are moved out of the buffer already. The
  // model takes size `kModelInputSize`. We pad the omitted bins with 0.
  for (size_t i = 0; i < kNumFfts; ++i) {
    const auto input_slice =
        input_dfts.subspan(i * kExpectedFftSize + 2, kExpectedFftSize - 2);
    auto tensor_slice =
        input_tensor_span_.subspan(i * kExpectedFftSize, kExpectedFftSize - 2);
    CHECK_EQ(input_slice.size(), tensor_slice.size());

    tensor_slice.copy_from(input_slice);

    input_tensor_span_[(i + 1) * kExpectedFftSize - 2] = 0.0f;
    input_tensor_span_[(i + 1) * kExpectedFftSize - 1] = 0.0f;
  }

  {
    TRACE_EVENT("audio", "TfLiteVoiceIsolation::Inference");
    const base::ElapsedTimer inference_timer;
    CHECK_EQ(interpreter_->Invoke(), kTfLiteOk);
    RecordInferenceExecutionTime(inference_timer.Elapsed());
  }
  base::span(temp_output_).copy_from(output_tensor_span_);

  for (size_t k = 0; k < temp_output_.size(); ++k) {
    temp_output_[k] -= bias_[k];
  }

  const size_t num_output_channels =
      temp_output_.size() / (kNumFfts * kExpectedFftSize);

  CHECK_EQ(num_output_channels, 2u);

  // The denoising-only output of the model removes noise but keeps
  // reverberation. Unpack it with the DC and Nyquist components of the input
  // and estimate the speech mask from it, so that the mask only reflects the
  // noise that the model removes. This runs before the output loop, which may
  // overwrite `input_dfts` if it aliases `output_dfts`.
  const base::span<float> denoising_only_dfts(denoising_only_dfts_);
  for (size_t i = 0; i < kNumFfts; ++i) {
    const size_t frame_index = i * kExpectedFftSize * num_output_channels;
    const base::span<const float> input_slice =
        input_dfts.subspan(i * kExpectedFftSize, kExpectedFftSize);
    const base::span<float> denoising_only_slice =
        denoising_only_dfts.subspan(i * kExpectedFftSize, kExpectedFftSize);

    denoising_only_slice[0] = input_slice[0];
    denoising_only_slice[1] = input_slice[1];

    for (size_t j = 0; j < kExpectedFftSize - 2; j += 2) {
      const size_t denoising_only_index =
          frame_index + num_output_channels * j + kDenoisingOnlyOutputOffset;
      denoising_only_slice[j + 2] = temp_output_[denoising_only_index];
      denoising_only_slice[j + 3] = temp_output_[denoising_only_index + 1];
    }
  }
  speech_mask_estimator_.Estimate(input_dfts, denoising_only_dfts);

  // Mix the dereverberated output (channel 0 of the model) with the
  // denoising-only output unpacked above, so that a little of the room
  // reverberation remains.
  for (size_t i = 0; i < kNumFfts; ++i) {
    const size_t frame_index = i * kExpectedFftSize * num_output_channels;
    const base::span<const float> input_slice =
        input_dfts.subspan(i * kExpectedFftSize, kExpectedFftSize);
    const base::span<const float> denoising_only_slice =
        denoising_only_dfts.subspan(i * kExpectedFftSize, kExpectedFftSize);
    const base::span<float> output_slice =
        output_dfts.subspan(i * kExpectedFftSize, kExpectedFftSize);

    // Restore the DC and Nyquist components from the input.
    output_slice[0] = input_slice[0];
    output_slice[1] = input_slice[1];

    for (size_t j = 0; j < kExpectedFftSize - 2; j += 2) {
      const size_t dereverb_index = frame_index + num_output_channels * j;
      output_slice[j + 2] = kDereverbWeight * temp_output_[dereverb_index] +
                            kDenoisingOnlyWeight * denoising_only_slice[j + 2];
      output_slice[j + 3] = kDereverbWeight * temp_output_[dereverb_index + 1] +
                            kDenoisingOnlyWeight * denoising_only_slice[j + 3];
    }
  }
}

size_t TfLiteVoiceIsolation::FrameSize() const {
  return kFrameSize;
}

size_t TfLiteVoiceIsolation::FramesPerSecond() const {
  return kFramesPerSecond;
}

void TfLiteVoiceIsolation::RecordInferenceExecutionTime(
    base::TimeDelta elapsed) {
  ++inferences_in_log_period_;
  sum_inference_time_in_period_ += elapsed;
  max_inference_time_in_period_ =
      std::max(max_inference_time_in_period_, elapsed);

  if (inferences_in_log_period_ < kInferencesPerLogPeriod) {
    return;
  }

  base::UmaHistogramCustomMicrosecondsTimes(
      "Media.Audio.Capture.VoiceIsolation.ModelInferenceExecutionTime.Average",
      sum_inference_time_in_period_ / kInferencesPerLogPeriod,
      kInferenceTimeHistogramMin, kInferenceTimeHistogramMax,
      kInferenceTimeHistogramBuckets);
  base::UmaHistogramCustomMicrosecondsTimes(
      "Media.Audio.Capture.VoiceIsolation.ModelInferenceExecutionTime.Max",
      max_inference_time_in_period_, kInferenceTimeHistogramMin,
      kInferenceTimeHistogramMax, kInferenceTimeHistogramBuckets);

  inferences_in_log_period_ = 0;
  sum_inference_time_in_period_ = base::TimeDelta();
  max_inference_time_in_period_ = base::TimeDelta();
}

void TfLiteVoiceIsolation::ClearBuffers() {
  speech_mask_estimator_.Clear();
  CHECK(ResetState());
}

base::TimeDelta TfLiteVoiceIsolation::AlgorithmicDelay() const {
  // The production model is causal with zero lookahead.
  // TODO(crbug.com/568614815): Read lookahead delay from TFLite model metadata
  // so future models can declare lookahead dynamically.
  return base::TimeDelta();
}

base::span<const float> TfLiteVoiceIsolation::SpeechMask() const {
  return speech_mask_estimator_.mask();
}

}  // namespace media
