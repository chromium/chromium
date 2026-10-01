// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"

#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"
#include "third_party/tflite/src/tensorflow/lite/version.h"

namespace media {

namespace {

constexpr char kTestModelFileName[] = "test_model_stateful_1_2_160_2.tflite";

}  // namespace

std::unique_ptr<tflite::FlatBufferModel> LoadVoiceIsolationTestModel() {
  base::FilePath source_root;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));

  const base::FilePath model_path = source_root.AppendASCII("media")
                                        .AppendASCII("webrtc")
                                        .AppendASCII("voice_isolation")
                                        .AppendASCII(kTestModelFileName);
  return tflite::FlatBufferModel::BuildFromFile(
      model_path.AsUTF8Unsafe().c_str());
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

}  // namespace media
