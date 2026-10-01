// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_TEST_UTILS_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_TEST_UTILS_H_

#include <memory>

namespace tflite {
namespace impl {
class FlatBufferModel;
}  // namespace impl
using FlatBufferModel = impl::FlatBufferModel;
}  // namespace tflite

namespace media {

// Loads the stateful Voice Isolation test model shipped with the tests. Returns
// nullptr if the model cannot be loaded.
std::unique_ptr<tflite::FlatBufferModel> LoadVoiceIsolationTestModel();

}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_TEST_UTILS_H_
