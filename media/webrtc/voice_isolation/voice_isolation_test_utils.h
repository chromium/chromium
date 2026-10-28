// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_TEST_UTILS_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_TEST_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {

// Holds a FlatBuffer DetachedBuffer and the FlatBufferModel built from it.
// The DetachedBuffer owns the underlying memory and must outlive `model`.
struct FakeModel {
  flatbuffers::DetachedBuffer buffer;
  std::unique_ptr<tflite::FlatBufferModel> model;
};

// Gains of the two outputs of the stateless test model built by
// BuildStatelessTestModel() after bias subtraction: the first (dereverberated)
// output is `kStatelessModelDereverbGain * input` and the second
// (denoising-only) output is `kStatelessModelDenoisingOnlyGain * input`.
inline constexpr float kStatelessModelDereverbGain = 0.5f;
inline constexpr float kStatelessModelDenoisingOnlyGain = 0.75f;

// Loads the stateful Voice Isolation test model shipped with the tests, whose
// recurrent state lives in resource variables. Its two outputs differ: after
// the bias subtraction, the second (denoising-only) output is twice the first
// (dereverberated) one. Returns nullptr if the model cannot be loaded.
std::unique_ptr<tflite::FlatBufferModel> LoadVoiceIsolationTestModel();

// Builds an in-memory stateless Voice Isolation test model whose two outputs
// are affine functions of its input with different gains and non-zero constant
// biases:
//   dereverberated = kStatelessModelDereverbGain * input + bias_0
//   denoising_only = kStatelessModelDenoisingOnlyGain * input + bias_1
FakeModel BuildStatelessTestModel();

// Builds a minimal FlatBuffer model containing a description string but no
// subgraphs or operators. This passes FlatBuffer schema verification but fails
// interpreter initialization with kInterpreterCreationFailed.
FakeModel BuildBogusModel();

// Builds a FlatBuffer model with one subgraph where the same float tensor of
// `tensor_size` elements is used as both the single input and single output,
// and no operators.
FakeModel BuildModelWithSameInputOutputTensor(int tensor_size);

// Returns the input DFTs of `num_calls` consecutive calls of a component that
// processes two 50%-overlapping DFTs of `dft_size` floats per call, like
// TfLiteVoiceIsolation, for uniform white noise generated from `seed`. A
// WindowedFft analysis, like the one in front of TfLiteVoiceIsolation, makes
// the DFTs consistent: consecutive DFTs share half of their window.
std::vector<std::vector<float>> MakeNoiseDfts(uint64_t seed,
                                              size_t dft_size,
                                              size_t num_calls);

}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_TEST_UTILS_H_
