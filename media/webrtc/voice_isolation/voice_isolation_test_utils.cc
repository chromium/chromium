// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/test/insecure_random_generator.h"
#include "media/webrtc/voice_isolation/stft_voice_isolation.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"
#include "third_party/tflite/src/tensorflow/lite/version.h"

namespace media {

namespace {

constexpr char kTestModelFileName[] = "test_model_stateful_1_2_160_2.tflite";

// WindowedFft::ForwardTransform() returns two DFTs per call.
constexpr size_t kNumDftsPerCall = 2;

// Shape constants and non-zero output biases of the stateless test model built
// by BuildStatelessTestModel(). TfLiteVoiceIsolation subtracts the zero-input
// warmup output from every call, so non-zero biases verify that bias
// subtraction is applied to both output channels.
constexpr int32_t kStatelessModelNumComplexBins = 320;
constexpr int32_t kStatelessModelInputChannels = 2;
constexpr int32_t kStatelessModelOutputChannels = 4;
constexpr float kStatelessModelDereverbBias = 1.0f;
constexpr float kStatelessModelDenoisingOnlyBias = 42.0f;

// Serializes `values` into a constant FlatBuffer buffer.
template <size_t N>
flatbuffers::Offset<tflite::Buffer> CreateFloatBuffer(
    flatbuffers::FlatBufferBuilder& builder,
    const std::array<float, N>& values) {
  const base::span<const uint8_t> bytes =
      base::as_byte_span(base::allow_nonunique_obj, values);
  return tflite::CreateBuffer(builder,
                              builder.CreateVector(bytes.data(), bytes.size()));
}

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

// Loads the test model `file_name` from the Voice Isolation source directory.
// Returns nullptr if the model cannot be loaded.
std::unique_ptr<tflite::FlatBufferModel> LoadTestModel(
    std::string_view file_name) {
  base::FilePath source_root;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));

  const base::FilePath model_path = source_root.AppendASCII("media")
                                        .AppendASCII("webrtc")
                                        .AppendASCII("voice_isolation")
                                        .AppendASCII(file_name);
  return tflite::FlatBufferModel::BuildFromFile(
      model_path.AsUTF8Unsafe().c_str());
}

}  // namespace

std::unique_ptr<tflite::FlatBufferModel> LoadVoiceIsolationTestModel() {
  return LoadTestModel(kTestModelFileName);
}

FakeModel BuildStatelessTestModel() {
  flatbuffers::FlatBufferBuilder builder(4096);

  // Per-bin (re, im) scale and bias for the interleaved dereverberated and
  // denoising-only outputs.
  constexpr std::array<float, kStatelessModelOutputChannels> kScaleValues = {
      kStatelessModelDereverbGain,
      kStatelessModelDereverbGain,
      kStatelessModelDenoisingOnlyGain,
      kStatelessModelDenoisingOnlyGain,
  };
  constexpr std::array<float, kStatelessModelOutputChannels> kBiasValues = {
      kStatelessModelDereverbBias,
      kStatelessModelDereverbBias,
      kStatelessModelDenoisingOnlyBias,
      kStatelessModelDenoisingOnlyBias,
  };

  std::vector<flatbuffers::Offset<tflite::Buffer>> buffers = {
      tflite::CreateBuffer(builder),
      CreateFloatBuffer(builder, kScaleValues),
      CreateFloatBuffer(builder, kBiasValues),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
      tflite::CreateBuffer(builder),
  };

  const std::vector<int32_t> input_shape = {1, kStatelessModelNumComplexBins,
                                            kStatelessModelInputChannels};
  const std::vector<int32_t> parameter_shape = {1, 1,
                                                kStatelessModelOutputChannels};
  const std::vector<int32_t> output_shape = {1, kStatelessModelNumComplexBins,
                                             kStatelessModelOutputChannels};
  std::vector<flatbuffers::Offset<tflite::Tensor>> tensors = {
      tflite::CreateTensorDirect(builder, &input_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/3,
                                 "input"),
      tflite::CreateTensorDirect(builder, &parameter_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/1,
                                 "scale"),
      tflite::CreateTensorDirect(builder, &parameter_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/2,
                                 "bias"),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/4,
                                 "concat_out"),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/5,
                                 "mul_out"),
      tflite::CreateTensorDirect(builder, &output_shape,
                                 tflite::TensorType_FLOAT32, /*buffer=*/6,
                                 "output"),
  };

  std::vector<flatbuffers::Offset<tflite::OperatorCode>> opcodes = {
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_CONCATENATION),
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_MUL),
      tflite::CreateOperatorCode(builder, /*deprecated_builtin_code=*/0,
                                 /*custom_code=*/0, /*version=*/1,
                                 tflite::BuiltinOperator_ADD),
  };

  const std::vector<int32_t> concat_inputs = {0, 0};
  const std::vector<int32_t> concat_outputs = {3};
  const std::vector<int32_t> mul_inputs = {3, 1};
  const std::vector<int32_t> mul_outputs = {4};
  const std::vector<int32_t> add_inputs = {4, 2};
  const std::vector<int32_t> add_outputs = {5};
  std::vector<flatbuffers::Offset<tflite::Operator>> operators = {
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/0, &concat_inputs, &concat_outputs,
          tflite::BuiltinOptions_ConcatenationOptions,
          tflite::CreateConcatenationOptions(builder, /*axis=*/2).Union()),
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/1, &mul_inputs, &mul_outputs,
          tflite::BuiltinOptions_MulOptions,
          tflite::CreateMulOptions(builder, tflite::ActivationFunctionType_NONE)
              .Union()),
      tflite::CreateOperatorDirect(
          builder, /*opcode_index=*/2, &add_inputs, &add_outputs,
          tflite::BuiltinOptions_AddOptions,
          tflite::CreateAddOptions(builder, tflite::ActivationFunctionType_NONE)
              .Union()),
  };

  const std::vector<int32_t> subgraph_inputs = {0};
  const std::vector<int32_t> subgraph_outputs = {5};
  std::vector<flatbuffers::Offset<tflite::SubGraph>> subgraphs = {
      tflite::CreateSubGraphDirect(builder, &tensors, &subgraph_inputs,
                                   &subgraph_outputs, &operators, "main"),
  };

  return FinishFakeModel(
      builder, tflite::CreateModelDirect(
                   builder, /*version=*/TFLITE_SCHEMA_VERSION, &opcodes,
                   &subgraphs, "stateless_affine_test_model", &buffers));
}

FakeModel BuildBogusModel() {
  flatbuffers::FlatBufferBuilder builder(1024);
  auto description_offset = builder.CreateString("bogus");
  tflite::ModelBuilder model_builder(builder);
  model_builder.add_description(description_offset);
  return FinishFakeModel(builder, model_builder.Finish());
}

FakeModel BuildModelWithSameInputOutputTensor(int tensor_size) {
  flatbuffers::FlatBufferBuilder builder(1024);

  auto buffer = tflite::CreateBuffer(builder);
  std::vector<flatbuffers::Offset<tflite::Buffer>> buffers = {buffer};

  std::vector<int32_t> shape = {1, tensor_size};
  auto tensor = tflite::CreateTensorDirect(
      builder, &shape, tflite::TensorType_FLOAT32, /*buffer=*/0, "tensor");
  std::vector<flatbuffers::Offset<tflite::Tensor>> tensors = {tensor};

  std::vector<int32_t> inputs = {0};
  std::vector<int32_t> outputs = {0};
  auto subgraph = tflite::CreateSubGraphDirect(
      builder, &tensors, &inputs, &outputs, /*operators=*/nullptr, "subgraph");
  std::vector<flatbuffers::Offset<tflite::SubGraph>> subgraphs = {subgraph};

  return FinishFakeModel(
      builder,
      tflite::CreateModelDirect(builder, /*version=*/TFLITE_SCHEMA_VERSION,
                                /*operator_codes=*/nullptr, &subgraphs,
                                "invalid_tensor_metadata", &buffers));
}

std::vector<std::vector<float>> MakeNoiseDfts(uint64_t seed,
                                              size_t dft_size,
                                              size_t num_calls) {
  base::test::InsecureRandomGenerator generator;
  generator.ReseedForTesting(seed);
  WindowedFft windowed_fft(base::checked_cast<int>(dft_size));
  std::vector<float> audio(dft_size);
  std::vector<std::vector<float>> dfts(
      num_calls, std::vector<float>(kNumDftsPerCall * dft_size));
  for (std::vector<float>& call_dfts : dfts) {
    for (float& sample : audio) {
      sample = static_cast<float>(generator.RandDouble());
    }
    windowed_fft.ForwardTransform(audio, call_dfts);
  }
  return dfts;
}

}  // namespace media
