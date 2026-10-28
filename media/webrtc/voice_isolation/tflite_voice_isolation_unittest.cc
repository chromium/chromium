// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/tflite_voice_isolation.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/memory/aligned_memory.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "media/webrtc/voice_isolation/speech_mask_estimator.h"
#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"
#include "third_party/tflite/src/tensorflow/lite/version.h"

namespace media {
namespace {

using ::testing::Each;
using ::testing::FloatNear;

// Layout of the TfLiteVoiceIsolation frame: two DFTs of `kDftSize` floats
// each. The first two floats of every DFT hold the DC and Nyquist components,
// which bypass the model. The remaining `kDftSize - 2` floats are fed to the
// model and the last two model inputs are zero padding.
constexpr size_t kNumDfts = 2;
constexpr size_t kDftSize = 320;
constexpr size_t kNumBypassedFloats = 2;

// The speech mask has one value per bin, from DC to Nyquist.
constexpr size_t kSpeechMaskSize = kDftSize / 2 + 1;

// Number of calls of the speech mask tests. The first call primes the
// overlap-add history of the mask estimation, so the checks of the mask values
// start at the second one.
constexpr size_t kNumSpeechMaskCalls = 4;
constexpr size_t kFirstCheckedSpeechMaskCall = 1;

// Seeds of two different white noise test signals.
constexpr uint64_t kFirstNoiseSeed = 1;
constexpr uint64_t kSecondNoiseSeed = 2;

// TfLiteVoiceIsolation::ProcessAudio() returns kDereverbWeight times the
// dereverberated output of the model plus the rest of its denoising-only
// output.
constexpr float kDereverbWeight = 0.9f;

// Constants of the stateful test model. Its second output is
// kModelDenoisingOnlyGain times the first one plus a constant offset, which the
// bias subtraction removes.
constexpr float kModelInputOffset = 1.0f;
constexpr float kModelConvGain = 0.5f;
constexpr float kModelLeakyAlpha = 0.5f;
constexpr float kModelDenoisingOnlyGain = 2.0f;

// Step and period of the deterministic test input. All values are multiples of
// a power of two so that the model output is exact in float32.
constexpr float kTestInputStep = 0.25f;
constexpr int kTestInputPeriod = 17;
constexpr float kTestInputFrameOffset = 0.5f;

float LeakyRelu(float value) {
  return value >= 0.0f ? value : kModelLeakyAlpha * value;
}

// Returns a deterministic input frame that covers both branches of the
// LeakyRelu and changes from frame to frame.
std::vector<float> MakeTestInput(size_t frame_size, int frame_index) {
  std::vector<float> input(frame_size);
  const int half_period = kTestInputPeriod / 2;
  for (size_t i = 0; i < frame_size; ++i) {
    const int step = static_cast<int>(i) % kTestInputPeriod - half_period;
    input[i] = kTestInputStep * step + kTestInputFrameOffset * frame_index;
  }
  return input;
}

// Closed-form reference of `test_model_stateful_1_2_160_2.tflite` as seen
// through TfLiteVoiceIsolation, including the bias subtraction. Per model
// input `k`, with state `s` starting at zero:
//   prev_0 = s, prev_1 = x_0 + 1
//   y_t = x_t + leaky_relu(0.5 * prev_t)
//   s <- x_1 + 1
// The model also outputs a second channel `2 * y_t + 42`, the denoising-only
// output. After the bias subtraction it is twice the first channel, so
// ProcessAudio() returns (0.9 + 0.1 * 2) times the first channel. The second
// channel is not a constant offset of `y_t`, so reading the wrong channel
// changes the output even after the bias subtraction.
class StatefulTestModelReference {
 public:
  StatefulTestModelReference()
      : state_(kDftSize, 0.0f), denoising_only_(kNumDfts * kDftSize, 0.0f) {}

  std::vector<float> ProcessAudio(const std::vector<float>& input) {
    CHECK_EQ(input.size(), kNumDfts * kDftSize);

    // Unpack the model input tensor, zero padding the last two values.
    std::vector<std::vector<float>> x(kNumDfts,
                                      std::vector<float>(kDftSize, 0.0f));
    for (size_t t = 0; t < kNumDfts; ++t) {
      for (size_t k = 0; k < kDftSize - kNumBypassedFloats; ++k) {
        x[t][k] = input[t * kDftSize + kNumBypassedFloats + k];
      }
    }

    // Evaluate the model and the bias (model output for zero input and zero
    // state), then write the mix of both outputs in the TfLiteVoiceIsolation
    // layout. The denoising-only output keeps the DC and Nyquist components of
    // the input, as in TfLiteVoiceIsolation.
    std::vector<float> output(input.size());
    for (size_t t = 0; t < kNumDfts; ++t) {
      output[t * kDftSize] = input[t * kDftSize];
      output[t * kDftSize + 1] = input[t * kDftSize + 1];
      denoising_only_[t * kDftSize] = input[t * kDftSize];
      denoising_only_[t * kDftSize + 1] = input[t * kDftSize + 1];
      for (size_t k = 0; k < kDftSize - kNumBypassedFloats; ++k) {
        const float prev = t == 0 ? state_[k] : x[0][k] + kModelInputOffset;
        const float bias_prev = t == 0 ? 0.0f : kModelInputOffset;
        const float y = x[t][k] + LeakyRelu(kModelConvGain * prev);
        const float bias = LeakyRelu(kModelConvGain * bias_prev);
        const float dereverberated = y - bias;
        const float denoising_only = kModelDenoisingOnlyGain * dereverberated;
        output[t * kDftSize + kNumBypassedFloats + k] =
            kDereverbWeight * dereverberated +
            (1.0f - kDereverbWeight) * denoising_only;
        denoising_only_[t * kDftSize + kNumBypassedFloats + k] = denoising_only;
      }
    }

    // Update the state with the last frame.
    for (size_t k = 0; k < kDftSize; ++k) {
      state_[k] = x[kNumDfts - 1][k] + kModelInputOffset;
    }
    return output;
  }

  // Denoising-only output DFTs of the last ProcessAudio() call.
  const std::vector<float>& denoising_only() const { return denoising_only_; }

 private:
  std::vector<float> state_;
  std::vector<float> denoising_only_;
};

// Feeds `voice_isolation` the calls of `input_dfts` and returns the speech
// mask after each call.
std::vector<std::vector<float>> ProcessAndGetSpeechMasks(
    TfLiteVoiceIsolation& voice_isolation,
    const std::vector<std::vector<float>>& input_dfts) {
  std::vector<std::vector<float>> masks;
  std::vector<float> output(voice_isolation.FrameSize());
  for (const std::vector<float>& call_dfts : input_dfts) {
    voice_isolation.ProcessAudio(call_dfts, output);
    masks.push_back(base::ToVector(voice_isolation.SpeechMask()));
  }
  return masks;
}

constexpr int32_t kModelInputElements = kNumDfts * kDftSize;
constexpr int32_t kModelOutputElements = 2 * kModelInputElements;

FakeModel FinishFakeModel(flatbuffers::FlatBufferBuilder& builder,
                          flatbuffers::Offset<tflite::Model> model_offset) {
  tflite::FinishModelBuffer(builder, model_offset);

  FakeModel fake_model;
  fake_model.buffer = builder.Release();
  fake_model.model = tflite::FlatBufferModel::VerifyAndBuildFromBuffer(
      reinterpret_cast<const char*>(fake_model.buffer.data()),
      fake_model.buffer.size());
  CHECK(fake_model.model);
  return fake_model;
}

// Builds a model whose input tensor has shape [1, 640] (640 elements) and is
// backed by a constant FlatBuffer buffer (`kTfLiteMmapRo`).
FakeModel BuildModelWithConstantInputBuffer() {
  flatbuffers::FlatBufferBuilder builder(8192);

  const std::vector<uint8_t> constant_input_bytes(
      kModelInputElements * sizeof(float), 0);
  std::vector<flatbuffers::Offset<tflite::Buffer>> buffers = {
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder, builder.CreateVector(constant_input_bytes)),
      tflite::CreateBuffer(builder),
  };

  const std::vector<int32_t> input_shape = {1, kModelInputElements};
  const std::vector<int32_t> output_shape = {1, kModelOutputElements};
  std::vector<flatbuffers::Offset<tflite::Tensor>> tensors = {
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/1,
                                 "input"),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/2,
                                 "output"),
  };

  std::vector<flatbuffers::Offset<tflite::OperatorCode>> opcodes = {
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_CONCATENATION),
  };

  const std::vector<int32_t> op_inputs = {0, 0};
  const std::vector<int32_t> op_outputs = {1};
  std::vector<flatbuffers::Offset<tflite::Operator>> operators = {
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/0, &op_inputs, &op_outputs,
          tflite::BuiltinOptions_ConcatenationOptions,
          tflite::CreateConcatenationOptions(builder, /*axis=*/1).Union()),
  };

  const std::vector<int32_t> subgraph_inputs = {0};
  const std::vector<int32_t> subgraph_outputs = {1};
  std::vector<flatbuffers::Offset<tflite::SubGraph>> subgraphs = {
      tflite::CreateSubGraphDirect(builder, &tensors, &subgraph_inputs,
                                   &subgraph_outputs, &operators, "main"),
  };

  return FinishFakeModel(
      builder, tflite::CreateModelDirect(
                   builder, /*version=*/TFLITE_SCHEMA_VERSION, &opcodes,
                   &subgraphs, "constant_input_buffer", &buffers));
}

// Builds a model with a valid primary subgraph and an unreferenced secondary
// subgraph containing an `is_variable` tensor. `Interpreter::AllocateTensors()`
// only allocates the primary subgraph, so `Interpreter::ResetVariableTensors()`
// fails on the unallocated secondary-subgraph variable tensor.
FakeModel BuildModelWithUnallocatedSecondarySubgraphVariable() {
  flatbuffers::FlatBufferBuilder builder(4096);

  std::vector<flatbuffers::Offset<tflite::Buffer>> buffers = {
      tflite::CreateBuffer(builder), tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder), tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
  };

  std::vector<flatbuffers::Offset<tflite::OperatorCode>> opcodes = {
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_CONCATENATION),
  };

  const std::vector<int32_t> input_shape = {1, kModelInputElements};
  const std::vector<int32_t> output_shape = {1, kModelOutputElements};
  std::vector<flatbuffers::Offset<tflite::Tensor>> primary_tensors = {
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/1,
                                 "input"),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/2,
                                 "output"),
  };
  const std::vector<int32_t> op_inputs = {0, 0};
  const std::vector<int32_t> op_outputs = {1};
  std::vector<flatbuffers::Offset<tflite::Operator>> primary_operators = {
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/0, &op_inputs, &op_outputs,
          tflite::BuiltinOptions_ConcatenationOptions,
          tflite::CreateConcatenationOptions(builder, /*axis=*/1).Union()),
  };
  const std::vector<int32_t> primary_inputs = {0};
  const std::vector<int32_t> primary_outputs = {1};

  std::vector<flatbuffers::Offset<tflite::Tensor>> secondary_tensors = {
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/3,
                                 "unallocated_var", /*quantization=*/0,
                                 /*is_variable=*/true),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/4,
                                 "secondary_out"),
  };
  std::vector<flatbuffers::Offset<tflite::Operator>> secondary_operators = {
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/0, &op_inputs, &op_outputs,
          tflite::BuiltinOptions_ConcatenationOptions,
          tflite::CreateConcatenationOptions(builder, /*axis=*/1).Union()),
  };
  const std::vector<int32_t> secondary_inputs = {0};
  const std::vector<int32_t> secondary_outputs = {1};

  std::vector<flatbuffers::Offset<tflite::SubGraph>> subgraphs = {
      tflite::CreateSubGraphDirect(builder, &primary_tensors, &primary_inputs,
                                   &primary_outputs, &primary_operators,
                                   "main"),
      tflite::CreateSubGraphDirect(
          builder, &secondary_tensors, &secondary_inputs, &secondary_outputs,
          &secondary_operators, "unreferenced_subgraph"),
  };

  return FinishFakeModel(
      builder, tflite::CreateModelDirect(
                   builder, /*version=*/TFLITE_SCHEMA_VERSION, &opcodes,
                   &subgraphs, "unallocated_secondary_var", &buffers));
}

enum class ExtraPrimaryResourceVariable {
  kNone,
  kUnassignedByInitSubgraph,
};

// Builds a model whose CallOnce initialization subgraph assigns a 640-element
// int32 tensor initialized to `init_byte_value` to a resource variable, while
// the primary subgraph assigns a `primary_elements`-element int32 tensor to the
// same resource variable during `Invoke()`. When `extra_variable` is
// `kUnassignedByInitSubgraph`, the primary subgraph also assigns an all-zero
// tensor to a second resource variable that the initialization subgraph omits.
// `INT32` is used so the XNNPACK delegate does not claim `ASSIGN_VARIABLE`.
FakeModel BuildModelWithResourceVariable(
    int32_t primary_elements = kModelInputElements,
    uint8_t init_byte_value = 0,
    ExtraPrimaryResourceVariable extra_variable =
        ExtraPrimaryResourceVariable::kNone) {
  flatbuffers::FlatBufferBuilder builder(16384);

  const std::vector<uint8_t> primary_value_bytes(
      primary_elements * sizeof(int32_t), 0);
  const std::vector<uint8_t> init_value_bytes(
      kModelInputElements * sizeof(int32_t), init_byte_value);
  std::vector<flatbuffers::Offset<tflite::Buffer>> buffers = {
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder, builder.CreateVector(primary_value_bytes)),
      tflite::CreateBuffer(builder, builder.CreateVector(init_value_bytes)),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
  };

  std::vector<flatbuffers::Offset<tflite::OperatorCode>> opcodes = {
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_CALL_ONCE),
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_VAR_HANDLE),
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_ASSIGN_VARIABLE),
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_CONCATENATION),
  };

  const std::vector<int32_t> input_shape = {1, kModelInputElements};
  const std::vector<int32_t> output_shape = {1, kModelOutputElements};
  const std::vector<int32_t> primary_var_shape = {1, primary_elements};
  const std::vector<int32_t> resource_shape = {1};

  // Primary subgraph: runs CallOnce(subgraph 1), concatenates the 640-float
  // input into the 1280-float output, and overwrites resource variable "state"
  // with a `primary_elements`-int32 tensor.
  std::vector<flatbuffers::Offset<tflite::Tensor>> primary_tensors = {
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/1,
                                 "input"),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/2,
                                 "output"),
      tflite::CreateTensorDirect(builder, &resource_shape,
                                 tflite::TensorType_RESOURCE, /*buffer=*/3,
                                 "var_handle"),
      tflite::CreateTensorDirect(builder, &primary_var_shape,
                                 tflite::TensorType_INT32, /*buffer=*/4,
                                 "primary_const"),
  };
  const std::vector<int32_t> empty_tensors = {};
  const std::vector<int32_t> concat_inputs = {0, 0};
  const std::vector<int32_t> concat_outputs = {1};
  const std::vector<int32_t> var_handle_outputs = {2};
  const std::vector<int32_t> primary_assign_inputs = {2, 3};
  std::vector<flatbuffers::Offset<tflite::Operator>> primary_operators = {
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/0, &empty_tensors, &empty_tensors,
          tflite::BuiltinOptions_CallOnceOptions,
          tflite::CreateCallOnceOptions(builder, /*init_subgraph_index=*/1)
              .Union()),
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/3, &concat_inputs, &concat_outputs,
          tflite::BuiltinOptions_ConcatenationOptions,
          tflite::CreateConcatenationOptions(builder, /*axis=*/1).Union()),
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/1, &empty_tensors, &var_handle_outputs,
          tflite::BuiltinOptions_VarHandleOptions,
          tflite::CreateVarHandleOptionsDirect(builder, /*container=*/"c",
                                               /*shared_name=*/"state")
              .Union()),
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/2, &primary_assign_inputs, &empty_tensors,
          tflite::BuiltinOptions_AssignVariableOptions,
          tflite::CreateAssignVariableOptions(builder).Union()),
  };
  if (extra_variable ==
      ExtraPrimaryResourceVariable::kUnassignedByInitSubgraph) {
    primary_tensors.push_back(tflite::CreateTensorDirect(
        builder, &resource_shape, tflite::TensorType_RESOURCE, /*buffer=*/7,
        "unassigned_var_handle"));
    const std::vector<int32_t> unassigned_var_handle_outputs = {4};
    const std::vector<int32_t> unassigned_assign_inputs = {4, 3};
    primary_operators.push_back(tflite::CreateOperatorDirect(
        builder, /*opcode_index=*/1, &empty_tensors,
        &unassigned_var_handle_outputs, tflite::BuiltinOptions_VarHandleOptions,
        tflite::CreateVarHandleOptionsDirect(builder, /*container=*/"c",
                                             /*shared_name=*/"unassigned_state")
            .Union()));
    primary_operators.push_back(tflite::CreateOperatorDirect(
        builder, /*opcode_index=*/2, &unassigned_assign_inputs, &empty_tensors,
        tflite::BuiltinOptions_AssignVariableOptions,
        tflite::CreateAssignVariableOptions(builder).Union()));
  }
  const std::vector<int32_t> primary_inputs = {0};
  const std::vector<int32_t> primary_outputs = {1};

  // Init subgraph: initializes resource variable "state" with 640 int32s.
  std::vector<flatbuffers::Offset<tflite::Tensor>> init_tensors = {
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_INT32, /*buffer=*/5,
                                 "init_const"),
      tflite::CreateTensorDirect(builder, &resource_shape,
                                 tflite::TensorType_RESOURCE, /*buffer=*/6,
                                 "init_var_handle"),
  };
  const std::vector<int32_t> init_var_handle_outputs = {1};
  const std::vector<int32_t> init_assign_inputs = {1, 0};
  std::vector<flatbuffers::Offset<tflite::Operator>> init_operators = {
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/1, &empty_tensors, &init_var_handle_outputs,
          tflite::BuiltinOptions_VarHandleOptions,
          tflite::CreateVarHandleOptionsDirect(builder, /*container=*/"c",
                                               /*shared_name=*/"state")
              .Union()),
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/2, &init_assign_inputs, &empty_tensors,
          tflite::BuiltinOptions_AssignVariableOptions,
          tflite::CreateAssignVariableOptions(builder).Union()),
  };

  std::vector<flatbuffers::Offset<tflite::SubGraph>> subgraphs = {
      tflite::CreateSubGraphDirect(builder, &primary_tensors, &primary_inputs,
                                   &primary_outputs, &primary_operators,
                                   "main"),
      tflite::CreateSubGraphDirect(builder, &init_tensors, /*inputs=*/nullptr,
                                   /*outputs=*/nullptr, &init_operators,
                                   "init"),
  };

  return FinishFakeModel(
      builder, tflite::CreateModelDirect(
                   builder, /*version=*/TFLITE_SCHEMA_VERSION, &opcodes,
                   &subgraphs, "resource_var_model", &buffers));
}

// Builds a model whose primary subgraph writes a resource variable without a
// CallOnce initialization subgraph.
FakeModel BuildModelWithResourceVariableWithoutInitSubgraph() {
  flatbuffers::FlatBufferBuilder builder(8192);

  const std::vector<uint8_t> primary_value_bytes(
      kModelInputElements * sizeof(int32_t), 0);
  std::vector<flatbuffers::Offset<tflite::Buffer>> buffers = {
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder, builder.CreateVector(primary_value_bytes)),
  };

  std::vector<flatbuffers::Offset<tflite::OperatorCode>> opcodes = {
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_VAR_HANDLE),
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_ASSIGN_VARIABLE),
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_CONCATENATION),
  };

  const std::vector<int32_t> input_shape = {1, kModelInputElements};
  const std::vector<int32_t> output_shape = {1, kModelOutputElements};
  const std::vector<int32_t> resource_shape = {1};
  std::vector<flatbuffers::Offset<tflite::Tensor>> primary_tensors = {
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/1,
                                 "input"),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/2,
                                 "output"),
      tflite::CreateTensorDirect(builder, &resource_shape,
                                 tflite::TensorType_RESOURCE, /*buffer=*/3,
                                 "var_handle"),
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_INT32, /*buffer=*/4,
                                 "primary_const"),
  };

  const std::vector<int32_t> empty_tensors = {};
  const std::vector<int32_t> concat_inputs = {0, 0};
  const std::vector<int32_t> concat_outputs = {1};
  const std::vector<int32_t> var_handle_outputs = {2};
  const std::vector<int32_t> assign_inputs = {2, 3};
  std::vector<flatbuffers::Offset<tflite::Operator>> primary_operators = {
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/2, &concat_inputs, &concat_outputs,
          tflite::BuiltinOptions_ConcatenationOptions,
          tflite::CreateConcatenationOptions(builder, /*axis=*/1).Union()),
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/0, &empty_tensors, &var_handle_outputs,
          tflite::BuiltinOptions_VarHandleOptions,
          tflite::CreateVarHandleOptionsDirect(builder, /*container=*/"c",
                                               /*shared_name=*/"state")
              .Union()),
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/1, &assign_inputs, &empty_tensors,
          tflite::BuiltinOptions_AssignVariableOptions,
          tflite::CreateAssignVariableOptions(builder).Union()),
  };
  const std::vector<int32_t> primary_inputs = {0};
  const std::vector<int32_t> primary_outputs = {1};
  std::vector<flatbuffers::Offset<tflite::SubGraph>> subgraphs = {
      tflite::CreateSubGraphDirect(builder, &primary_tensors, &primary_inputs,
                                   &primary_outputs, &primary_operators,
                                   "main"),
  };

  return FinishFakeModel(
      builder, tflite::CreateModelDirect(
                   builder, /*version=*/TFLITE_SCHEMA_VERSION, &opcodes,
                   &subgraphs, "resource_var_without_init", &buffers));
}

// Builds a model whose output tensor is produced by a RESHAPE op with a
// non-constant shape tensor (`is_variable = true`), making the subgraph have
// dynamic tensors (`Subgraph::HasDynamicTensors() == true`).
FakeModel BuildModelWithDynamicTensor() {
  flatbuffers::FlatBufferBuilder builder(4096);

  std::vector<flatbuffers::Offset<tflite::Buffer>> buffers = {
      tflite::CreateBuffer(builder), tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder), tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
  };

  std::vector<flatbuffers::Offset<tflite::OperatorCode>> opcodes = {
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_CONCATENATION),
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_RESHAPE),
  };

  const std::vector<int32_t> input_shape = {1, kModelInputElements};
  const std::vector<int32_t> output_shape = {1, kModelOutputElements};
  const std::vector<int32_t> target_shape_dims = {2};
  std::vector<flatbuffers::Offset<tflite::Tensor>> tensors = {
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/1,
                                 "input"),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/2,
                                 "concat_out"),
      tflite::CreateTensorDirect(builder, &target_shape_dims,
                                 tflite::TensorType_INT32, /*buffer=*/3,
                                 "dynamic_shape", /*quantization=*/0,
                                 /*is_variable=*/true),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/4,
                                 "output"),
  };

  const std::vector<int32_t> concat_inputs = {0, 0};
  const std::vector<int32_t> concat_outputs = {1};
  const std::vector<int32_t> reshape_inputs = {1, 2};
  const std::vector<int32_t> reshape_outputs = {3};
  std::vector<flatbuffers::Offset<tflite::Operator>> operators = {
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/0, &concat_inputs, &concat_outputs,
          tflite::BuiltinOptions_ConcatenationOptions,
          tflite::CreateConcatenationOptions(builder, /*axis=*/1).Union()),
      tflite::CreateOperatorDirect(builder, /*opcode_index=*/1, &reshape_inputs,
                                   &reshape_outputs),
  };

  const std::vector<int32_t> subgraph_inputs = {0};
  const std::vector<int32_t> subgraph_outputs = {3};
  std::vector<flatbuffers::Offset<tflite::SubGraph>> subgraphs = {
      tflite::CreateSubGraphDirect(builder, &tensors, &subgraph_inputs,
                                   &subgraph_outputs, &operators, "main"),
  };

  return FinishFakeModel(
      builder, tflite::CreateModelDirect(
                   builder, /*version=*/TFLITE_SCHEMA_VERSION, &opcodes,
                   &subgraphs, "dynamic_tensor", &buffers));
}

// Builds a model whose primary subgraph uses an `IF` control-flow operator to
// invoke a secondary subgraph.
FakeModel BuildModelWithIfOp() {
  flatbuffers::FlatBufferBuilder builder(8192);

  const std::vector<uint8_t> cond_bytes = {1};
  std::vector<flatbuffers::Offset<tflite::Buffer>> buffers = {
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder, builder.CreateVector(cond_bytes)),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
  };

  std::vector<flatbuffers::Offset<tflite::OperatorCode>> opcodes = {
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_IF),
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_CONCATENATION),
  };

  const std::vector<int32_t> input_shape = {1, kModelInputElements};
  const std::vector<int32_t> output_shape = {1, kModelOutputElements};
  const std::vector<int32_t> cond_shape = {1};
  std::vector<flatbuffers::Offset<tflite::Tensor>> primary_tensors = {
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/1,
                                 "input"),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/2,
                                 "output"),
      tflite::CreateTensorDirect(builder, &cond_shape, tflite::TensorType_BOOL,
                                 /*buffer=*/3, "cond"),
  };
  const std::vector<int32_t> if_inputs = {2, 0};
  const std::vector<int32_t> if_outputs = {1};
  std::vector<flatbuffers::Offset<tflite::Operator>> primary_operators = {
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/0, &if_inputs, &if_outputs,
          tflite::BuiltinOptions_IfOptions,
          tflite::CreateIfOptions(builder, /*then_subgraph_index=*/1,
                                  /*else_subgraph_index=*/1)
              .Union()),
  };
  const std::vector<int32_t> primary_inputs = {0};
  const std::vector<int32_t> primary_outputs = {1};

  std::vector<flatbuffers::Offset<tflite::Tensor>> branch_tensors = {
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/4,
                                 "branch_in"),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/5,
                                 "branch_out"),
  };
  const std::vector<int32_t> concat_inputs = {0, 0};
  const std::vector<int32_t> concat_outputs = {1};
  std::vector<flatbuffers::Offset<tflite::Operator>> branch_operators = {
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/1, &concat_inputs, &concat_outputs,
          tflite::BuiltinOptions_ConcatenationOptions,
          tflite::CreateConcatenationOptions(builder, /*axis=*/1).Union()),
  };
  const std::vector<int32_t> branch_inputs = {0};
  const std::vector<int32_t> branch_outputs = {1};

  std::vector<flatbuffers::Offset<tflite::SubGraph>> subgraphs = {
      tflite::CreateSubGraphDirect(builder, &primary_tensors, &primary_inputs,
                                   &primary_outputs, &primary_operators,
                                   "main"),
      tflite::CreateSubGraphDirect(builder, &branch_tensors, &branch_inputs,
                                   &branch_outputs, &branch_operators,
                                   "branch"),
  };

  return FinishFakeModel(
      builder,
      tflite::CreateModelDirect(builder, /*version=*/TFLITE_SCHEMA_VERSION,
                                &opcodes, &subgraphs, "if_op", &buffers));
}

}  // namespace

TEST(TfLiteVoiceIsolation, CreateWorks) {
  auto model = LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(auto voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));
  EXPECT_EQ(voice_isolation->FrameSize(), 640u);
  EXPECT_EQ(voice_isolation->AlgorithmicDelay(), base::TimeDelta());
}

TEST(TfLiteVoiceIsolation, ProcessAudioWorks) {
  auto model = LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(auto voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  std::vector<float> input(voice_isolation->FrameSize(), 1.0f);
  std::vector<float> output(voice_isolation->FrameSize(), 0.0f);

  voice_isolation->ProcessAudio(input, output);

  for (auto x : output) {
    EXPECT_NE(x, 0.0f);
  }
}

// ProcessAudio() returns 0.9 times the dereverberated output plus 0.1 times
// the denoising-only output. After the bias subtraction, the denoising-only
// output of the stateful test model is twice its dereverberated output, so the
// closed form expects 1.1 times the dereverberated output: returning the
// dereverberated output alone, or unpacking the wrong channel, gives 1.0 and
// swapped weights give 1.9. DC and Nyquist bypass the model.
TEST(TfLiteVoiceIsolation, ProcessAudioMatchesClosedForm) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  // A freshly created object must start from the initial model state.
  StatefulTestModelReference reference;
  std::vector<float> output(voice_isolation->FrameSize(), 0.0f);
  constexpr int kNumFrames = 4;
  for (int frame = 0; frame < kNumFrames; ++frame) {
    const std::vector<float> input =
        MakeTestInput(voice_isolation->FrameSize(), frame);
    const std::vector<float> expected = reference.ProcessAudio(input);

    voice_isolation->ProcessAudio(input, output);

    // The test model output is exact in float32, and the mix runs the same
    // float operations as the reference.
    EXPECT_EQ(output, expected) << "frame " << frame;
  }
}

TEST(TfLiteVoiceIsolation, ClearBuffersRestoresInitialState) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  const std::vector<float> input =
      MakeTestInput(voice_isolation->FrameSize(), /*frame_index=*/0);
  std::vector<float> output1(voice_isolation->FrameSize(), 0.0f);
  std::vector<float> output2(voice_isolation->FrameSize(), 0.0f);
  std::vector<float> output3(voice_isolation->FrameSize(), 0.0f);

  // Process the same input twice. The model state carries over, so the second
  // output must differ from the first one. Otherwise this test cannot detect
  // whether ClearBuffers() resets anything.
  voice_isolation->ProcessAudio(input, output1);
  voice_isolation->ProcessAudio(input, output2);
  ASSERT_NE(output1, output2);

  // After clearing, the model must behave as a freshly created one.
  voice_isolation->ClearBuffers();
  voice_isolation->ProcessAudio(input, output3);
  EXPECT_EQ(output1, output3);
}

TEST(TfLiteVoiceIsolation, SpeechMaskStartsAtOne) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  EXPECT_EQ(voice_isolation->SpeechMask().size(), kSpeechMaskSize);
  EXPECT_THAT(base::ToVector(voice_isolation->SpeechMask()), Each(1.0f));
}

// After bias subtraction, the denoising-only output of the stateless test model
// scales the interior bins (50 Hz to 7950 Hz) by
// `kStatelessModelDenoisingOnlyGain`, while DC and Nyquist bypass the model.
// Away from the bypassed DC and Nyquist bins, the speech mask matches
// `kStatelessModelDenoisingOnlyGain`.
TEST(TfLiteVoiceIsolation, SpeechMaskMatchesStatelessModelDenoisingOnlyGain) {
  constexpr float kTolerance = 1e-3f;
  // DC (bin 0) and Nyquist (bin 160) bypass the model with gain 1, and the
  // Hann window of MakeNoiseDfts() spreads the DC offset of the [0, 1) noise
  // into bin 1. From 10 bins away from DC and Nyquist on, leakage from the
  // bypassed bins changes the mask by less than `kTolerance`.
  constexpr size_t kMinBinsFromDcAndNyquist = 10;
  FakeModel fake_model = BuildStatelessTestModel();
  ASSERT_NE(fake_model.model, nullptr);

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
      TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get()));

  const std::vector<std::vector<float>> masks = ProcessAndGetSpeechMasks(
      *voice_isolation, MakeNoiseDfts(kFirstNoiseSeed, /*dft_size=*/kDftSize,
                                      /*num_calls=*/kNumSpeechMaskCalls));

  for (size_t call = kFirstCheckedSpeechMaskCall; call < kNumSpeechMaskCalls;
       ++call) {
    const base::span<const float> interior_mask =
        base::span(masks[call])
            .subspan(kMinBinsFromDcAndNyquist,
                     kSpeechMaskSize - 2 * kMinBinsFromDcAndNyquist);
    EXPECT_THAT(interior_mask,
                Each(FloatNear(kStatelessModelDenoisingOnlyGain, kTolerance)))
        << "call " << call;
  }
}

// The speech mask comes from the denoising-only output alone, not from the
// output mix: it matches a SpeechMaskEstimator fed the closed-form
// denoising-only output of the stateful test model, whose mask differs from
// that of the mix ProcessAudio() returns.
TEST(TfLiteVoiceIsolation, SpeechMaskIgnoresDereverberatedOutput) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  // Reference estimators fed the closed-form denoising-only output, which the
  // mask must come from, and the closed-form mix, which it must not.
  StatefulTestModelReference reference;
  SpeechMaskEstimator denoising_only_estimator(/*dft_size=*/kDftSize);
  SpeechMaskEstimator mix_estimator(/*dft_size=*/kDftSize);
  base::AlignedHeapArray<float> reference_dfts = base::AlignedUninit<float>(
      kNumDfts * kDftSize, SpeechMaskEstimator::kDftAlignment);
  std::vector<float> output(voice_isolation->FrameSize());

  // Feeds one frame to TfLiteVoiceIsolation and to both reference estimators.
  const auto process_frame = [&](int frame) {
    const std::vector<float> input =
        MakeTestInput(voice_isolation->FrameSize(), frame);
    const std::vector<float> mix = reference.ProcessAudio(input);

    base::span(reference_dfts).copy_from(reference.denoising_only());
    denoising_only_estimator.Estimate(input, reference_dfts);
    base::span(reference_dfts).copy_from(mix);
    mix_estimator.Estimate(input, reference_dfts);

    voice_isolation->ProcessAudio(input, output);
  };

  // The first call primes the overlap-add history of the speech mask
  // estimators.
  process_frame(/*frame=*/0);

  constexpr int kNumCheckedFrames = 3;
  for (int frame = 1; frame <= kNumCheckedFrames; ++frame) {
    process_frame(frame);
    const std::vector<float> expected_mask =
        base::ToVector(denoising_only_estimator.mask());

    // The test input must tell the two sources of the mask apart.
    ASSERT_NE(expected_mask, base::ToVector(mix_estimator.mask()))
        << "frame " << frame;
    EXPECT_EQ(base::ToVector(voice_isolation->SpeechMask()), expected_mask)
        << "frame " << frame;
  }
}

TEST(TfLiteVoiceIsolation, ProcessAudioSupportsInPlaceAliasing) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TfLiteVoiceIsolation> out_of_place_voice_isolation,
      TfLiteVoiceIsolation::MaybeCreate(model.get()));
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TfLiteVoiceIsolation> in_place_voice_isolation,
      TfLiteVoiceIsolation::MaybeCreate(model.get()));

  const std::vector<std::vector<float>> input_dfts =
      MakeNoiseDfts(kFirstNoiseSeed, /*dft_size=*/kDftSize,
                    /*num_calls=*/kNumSpeechMaskCalls);
  std::vector<float> out_of_place_output(
      out_of_place_voice_isolation->FrameSize());

  for (size_t call = 0; call < kNumSpeechMaskCalls; ++call) {
    out_of_place_voice_isolation->ProcessAudio(input_dfts[call],
                                               out_of_place_output);

    std::vector<float> in_place_buffer = input_dfts[call];
    in_place_voice_isolation->ProcessAudio(in_place_buffer, in_place_buffer);

    EXPECT_EQ(in_place_buffer, out_of_place_output) << "call " << call;
    EXPECT_EQ(base::ToVector(in_place_voice_isolation->SpeechMask()),
              base::ToVector(out_of_place_voice_isolation->SpeechMask()))
        << "call " << call;
  }
}

TEST(TfLiteVoiceIsolation, ClearBuffersResetsSpeechMask) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  // Both signals have the same level: a much louder first signal would mostly
  // raise the magnitudes of the second one, which the cap of the mask at 1
  // hides.
  const std::vector<std::vector<float>> first_signal =
      MakeNoiseDfts(kFirstNoiseSeed, /*dft_size=*/kDftSize,
                    /*num_calls=*/kNumSpeechMaskCalls);
  const std::vector<std::vector<float>> second_signal =
      MakeNoiseDfts(kSecondNoiseSeed, /*dft_size=*/kDftSize,
                    /*num_calls=*/kNumSpeechMaskCalls);

  // Reference: a fresh instance fed the second signal.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TfLiteVoiceIsolation> fresh_voice_isolation,
      TfLiteVoiceIsolation::MaybeCreate(model.get()));
  const std::vector<std::vector<float>> expected_masks =
      ProcessAndGetSpeechMasks(*fresh_voice_isolation, second_signal);

  // Without ClearBuffers(), the first signal changes the mask of the second
  // one. Otherwise this test could not detect whether ClearBuffers() resets the
  // mask estimation.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TfLiteVoiceIsolation> uncleared_voice_isolation,
      TfLiteVoiceIsolation::MaybeCreate(model.get()));
  ProcessAndGetSpeechMasks(*uncleared_voice_isolation, first_signal);
  ASSERT_NE(ProcessAndGetSpeechMasks(*uncleared_voice_isolation, second_signal),
            expected_masks);

  // ClearBuffers() resets the mask to all ones, and the mask estimation then
  // behaves as in a fresh instance.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));
  ProcessAndGetSpeechMasks(*voice_isolation, first_signal);
  voice_isolation->ClearBuffers();
  EXPECT_THAT(base::ToVector(voice_isolation->SpeechMask()), Each(1.0f));
  EXPECT_EQ(ProcessAndGetSpeechMasks(*voice_isolation, second_signal),
            expected_masks);
}

// Verifies that attempting to initialize TfLiteVoiceIsolation with a FlatBuffer
// model that passes buffer verification but lacks valid execution subgraphs or
// operators fails safely and returns kInterpreterCreationFailed without
// crashing.
TEST(TfLiteVoiceIsolation, FailsOnEmptyModel) {
  FakeModel bogus = BuildBogusModel();
  ASSERT_NE(bogus.model, nullptr);

  auto result = TfLiteVoiceIsolation::MaybeCreate(bogus.model.get());
  EXPECT_THAT(result,
              base::test::ErrorIs(
                  VoiceIsolationCreationResult::kInterpreterCreationFailed));
}

// Verifies that a FlatBuffer model with incompatible tensor configuration (such
// as an unexpected frame size or identical input and output tensors) is safely
// rejected with kIncompatibleModel.
TEST(TfLiteVoiceIsolation, FailsOnIncompatibleModel) {
  FakeModel fake_model =
      BuildModelWithSameInputOutputTensor(/*tensor_size=*/320);
  ASSERT_NE(fake_model.model, nullptr);

  // TODO(crbug.com/568298417): Remove the UBSan skip once TFLite no longer
  // calls memcpy() with a null pointer for models without operators.
#if defined(UNDEFINED_SANITIZER)
  GTEST_SKIP() << "TFLite has undefined behavior for models without operators.";
#else
  auto result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
#endif
}

TEST(TfLiteVoiceIsolation, RejectsModelWithConstantInputTensor) {
  FakeModel fake_model = BuildModelWithConstantInputBuffer();
  ASSERT_NE(fake_model.model, nullptr);

  base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
                 VoiceIsolationCreationResult>
      result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
}

TEST(TfLiteVoiceIsolation, RejectsModelWithVariableTensorsThatCannotBeReset) {
  FakeModel fake_model = BuildModelWithUnallocatedSecondarySubgraphVariable();
  ASSERT_NE(fake_model.model, nullptr);

  base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
                 VoiceIsolationCreationResult>
      result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
}

TEST(TfLiteVoiceIsolation, RejectsModelWhoseInitValueSizeDiffersFromStateSize) {
  FakeModel fake_model =
      BuildModelWithResourceVariable(/*primary_elements=*/kModelOutputElements,
                                     /*init_byte_value=*/0);
  ASSERT_NE(fake_model.model, nullptr);

  base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
                 VoiceIsolationCreationResult>
      result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
}

TEST(TfLiteVoiceIsolation, RejectsModelWithNonZeroResourceVariableInit) {
  FakeModel fake_model =
      BuildModelWithResourceVariable(/*primary_elements=*/kModelInputElements,
                                     /*init_byte_value=*/1);
  ASSERT_NE(fake_model.model, nullptr);

  base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
                 VoiceIsolationCreationResult>
      result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
}

TEST(TfLiteVoiceIsolation,
     RejectsModelWithResourceVariableWithoutInitSubgraph) {
  FakeModel fake_model = BuildModelWithResourceVariableWithoutInitSubgraph();
  ASSERT_NE(fake_model.model, nullptr);

  base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
                 VoiceIsolationCreationResult>
      result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
}

TEST(TfLiteVoiceIsolation, RejectsModelWithUnassignedResourceVariable) {
  FakeModel fake_model = BuildModelWithResourceVariable(
      /*primary_elements=*/kModelInputElements, /*init_byte_value=*/0,
      /*extra_variable=*/
      ExtraPrimaryResourceVariable::kUnassignedByInitSubgraph);
  ASSERT_NE(fake_model.model, nullptr);

  base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
                 VoiceIsolationCreationResult>
      result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
}

TEST(TfLiteVoiceIsolation, RejectsModelWithDynamicTensors) {
  FakeModel fake_model = BuildModelWithDynamicTensor();
  ASSERT_NE(fake_model.model, nullptr);

  base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
                 VoiceIsolationCreationResult>
      result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
}

TEST(TfLiteVoiceIsolation, RejectsModelWithControlFlowOp) {
  FakeModel fake_model = BuildModelWithIfOp();
  ASSERT_NE(fake_model.model, nullptr);

  base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
                 VoiceIsolationCreationResult>
      result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
}

TEST(TfLiteVoiceIsolation, RecordsModelInferenceExecutionTimeEvery10Seconds) {
  constexpr size_t kMetricsReportingIntervalSeconds = 10;
  constexpr std::string_view kAverageInferenceTimeHistogram =
      "Media.Audio.Capture.VoiceIsolation.ModelInferenceExecutionTime.Average";
  constexpr std::string_view kMaxInferenceTimeHistogram =
      "Media.Audio.Capture.VoiceIsolation.ModelInferenceExecutionTime.Max";
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers;
  base::HistogramTester histogram_tester;

  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  const std::vector<float> input(voice_isolation->FrameSize(), 0.0f);
  std::vector<float> output(voice_isolation->FrameSize(), 0.0f);

  // 10 seconds of active processing at 50 Hz = 500 calls. The warmup Invoke()
  // inside MaybeCreate() must not count toward the 500-call period.
  const size_t calls_per_10_seconds =
      kMetricsReportingIntervalSeconds * voice_isolation->FramesPerSecond();
  for (size_t i = 0; i < calls_per_10_seconds - 1; ++i) {
    voice_isolation->ProcessAudio(input, output);
  }

  histogram_tester.ExpectTotalCount(kAverageInferenceTimeHistogram,
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(kMaxInferenceTimeHistogram,
                                    /*expected_count=*/0);

  // The 500th call completes the 10-second window.
  voice_isolation->ProcessAudio(input, output);

  const int expected_high_res_samples =
      base::TimeTicks::IsHighResolution() ? 1 : 0;
  histogram_tester.ExpectUniqueSample(
      kAverageInferenceTimeHistogram,
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMicroseconds(),
      /*expected_bucket_count=*/expected_high_res_samples);
  histogram_tester.ExpectUniqueSample(
      kMaxInferenceTimeHistogram,
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMicroseconds(),
      /*expected_bucket_count=*/expected_high_res_samples);

  // Verify that the per-period counter resets and only logs again after a
  // second 500-call window completes.
  for (size_t i = 0; i < calls_per_10_seconds - 1; ++i) {
    voice_isolation->ProcessAudio(input, output);
  }
  histogram_tester.ExpectTotalCount(
      kAverageInferenceTimeHistogram,
      /*expected_count=*/expected_high_res_samples);

  voice_isolation->ProcessAudio(input, output);
  histogram_tester.ExpectTotalCount(
      kAverageInferenceTimeHistogram,
      /*expected_count=*/2 * expected_high_res_samples);
  histogram_tester.ExpectTotalCount(
      kMaxInferenceTimeHistogram,
      /*expected_count=*/2 * expected_high_res_samples);
}

}  // namespace media
