// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_encoder.h"

namespace blink {

AudioTrackEncoder::AudioTrackEncoder(
    OnEncodedAudioCB on_encoded_audio_cb,
    OnEncodedAudioErrorCB on_encoded_audio_error_cb)
    : paused_(false),
      on_encoded_audio_cb_(std::move(on_encoded_audio_cb)),
      on_encoded_audio_error_cb_(std::move(on_encoded_audio_error_cb)) {}

}  // namespace blink
