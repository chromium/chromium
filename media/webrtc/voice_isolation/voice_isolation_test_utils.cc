// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
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

constexpr char kTestModelFileName[] = "test_model_1_2_160_2.tflite";
constexpr char kStatefulTestModelFileName[] =
    "test_model_stateful_1_2_160_2.tflite";

// WindowedFft::ForwardTransform() returns two DFTs per call.
constexpr size_t kNumDftsPerCall = 2;

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

std::unique_ptr<tflite::FlatBufferModel> LoadStatefulVoiceIsolationTestModel() {
  return LoadTestModel(kStatefulTestModelFileName);
}

FakeModel BuildBogusModel() {
  flatbuffers::FlatBufferBuilder builder(1024);
  auto description_offset = builder.CreateString("bogus");
  tflite::ModelBuilder model_builder(builder);
  model_builder.add_description(description_offset);
  tflite::FinishModelBuffer(builder, model_builder.Finish());

  FakeModel fake_model;
  fake_model.buffer = builder.Release();
  fake_model.model = tflite::FlatBufferModel::VerifyAndBuildFromBuffer(
      reinterpret_cast<const char*>(fake_model.buffer.data()),
      fake_model.buffer.size());
  CHECK(fake_model.model);
  return fake_model;
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

  auto model_offset =
      tflite::CreateModelDirect(builder,
                                /*version=*/TFLITE_SCHEMA_VERSION,
                                /*operator_codes=*/nullptr, &subgraphs,
                                "invalid_tensor_metadata", &buffers);
  tflite::FinishModelBuffer(builder, model_offset);

  FakeModel fake_model;
  fake_model.buffer = builder.Release();
  fake_model.model = tflite::FlatBufferModel::VerifyAndBuildFromBuffer(
      reinterpret_cast<const char*>(fake_model.buffer.data()),
      fake_model.buffer.size());
  CHECK(fake_model.model);
  return fake_model;
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
