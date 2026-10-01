// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

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

}  // namespace media
