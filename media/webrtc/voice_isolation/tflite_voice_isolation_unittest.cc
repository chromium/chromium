// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/tflite_voice_isolation.h"

#include <cmath>
#include <complex>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {
namespace {

std::unique_ptr<tflite::FlatBufferModel> GetTestModelBuffer() {
  base::FilePath source_root;

  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));

  source_root = source_root.AppendASCII("media")
                    .AppendASCII("webrtc")
                    .AppendASCII("voice_isolation")
                    .AppendASCII("test_model_1_2_160_2.tflite");

  return tflite::FlatBufferModel::BuildFromFile(
      source_root.AsUTF8Unsafe().c_str());
}
}  // namespace

TEST(TfLiteVoiceIsolation, CreateWorks) {
  auto model = GetTestModelBuffer();
  ASSERT_NE(model, nullptr);

  auto voice_isolation = TfLiteVoiceIsolation::MaybeCreate(model.get());
  ASSERT_NE(voice_isolation, nullptr);
  EXPECT_EQ(voice_isolation->FrameSize(), 640u);
}

TEST(TfLiteVoiceIsolation, ProcessAudioWorks) {
  auto model = GetTestModelBuffer();
  ASSERT_NE(model, nullptr);

  auto voice_isolation = TfLiteVoiceIsolation::MaybeCreate(model.get());
  ASSERT_NE(voice_isolation, nullptr);

  std::vector<float> input(voice_isolation->FrameSize(), 1.0f);
  std::vector<float> output(voice_isolation->FrameSize(), 0.0f);

  voice_isolation->ProcessAudio(input, output);

  for (auto x : output) {
    EXPECT_NE(x, 0.0f);
  }
}

TEST(TfLiteVoiceIsolation, ClearBuffersWorks) {
  std::unique_ptr<tflite::FlatBufferModel> model = GetTestModelBuffer();
  ASSERT_NE(model, nullptr);

  std::unique_ptr<TfLiteVoiceIsolation> voice_isolation =
      TfLiteVoiceIsolation::MaybeCreate(model.get());
  ASSERT_NE(voice_isolation, nullptr);

  std::vector<float> input(voice_isolation->FrameSize(), 1.0f);
  std::vector<float> output1(voice_isolation->FrameSize(), 0.0f);
  std::vector<float> output2(voice_isolation->FrameSize(), 0.0f);
  std::vector<float> output3(voice_isolation->FrameSize(), 0.0f);

  // Execute model the first time.
  voice_isolation->ProcessAudio(input, output1);

  // Execute model a second time.
  voice_isolation->ProcessAudio(input, output2);

  // Call ClearBuffers to reset internal state.
  voice_isolation->ClearBuffers();

  // Execute model again after clearing.
  // TODO(barrerap): Current test model doest no have internal state. We need to
  // include a new test model with internal state to properly validate
  // TfLiteVoiceIsolation clears the internal state when `ClearBuffers` is used.
  voice_isolation->ProcessAudio(input, output3);

  // Output after clearing should match the first execution.
  for (size_t i = 0; i < voice_isolation->FrameSize(); ++i) {
    EXPECT_FLOAT_EQ(output1[i], output3[i]);
  }
}
}  // namespace media
