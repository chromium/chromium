// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_TEST_UTILS_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_TEST_UTILS_H_

#include <memory>

#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {

// Holds a FlatBuffer DetachedBuffer and the FlatBufferModel built from it.
// The DetachedBuffer owns the underlying memory and must outlive `model`.
struct FakeModel {
  flatbuffers::DetachedBuffer buffer;
  std::unique_ptr<tflite::FlatBufferModel> model;
};

// Loads the Voice Isolation test model shipped with the tests. Returns nullptr
// if the model cannot be loaded.
std::unique_ptr<tflite::FlatBufferModel> LoadVoiceIsolationTestModel();

// Builds a minimal FlatBuffer model containing a description string but no
// subgraphs or operators. This passes FlatBuffer schema verification but fails
// interpreter initialization with kInterpreterCreationFailed.
FakeModel BuildBogusModel();

// Builds a FlatBuffer model with one subgraph where the same float tensor of
// `tensor_size` elements is used as both the single input and single output,
// and no operators.
FakeModel BuildModelWithSameInputOutputTensor(int tensor_size);

}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_TEST_UTILS_H_
