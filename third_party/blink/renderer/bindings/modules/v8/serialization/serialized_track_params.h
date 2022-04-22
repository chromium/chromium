// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_SERIALIZATION_SERIALIZED_TRACK_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_SERIALIZATION_SERIALIZED_TRACK_PARAMS_H_

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"

namespace blink {

// Enum values must remain contiguous and starting at zero.
enum class SerializedContentHintType : uint32_t {
  kNone = 0,
  kAudioSpeech = 1,
  kAudioMusic = 2,
  kVideoMotion = 3,
  kVideoDetail = 4,
  kVideoText = 5,
  kLast = kVideoText,
};

// Enum values must remain contiguous and starting at zero.
enum class SerializedReadyState : uint32_t {
  kReadyStateLive = 0,
  kReadyStateMuted = 1,
  kReadyStateEnded = 2,
  kLast = kReadyStateEnded
};

SerializedContentHintType SerializeContentHint(
    WebMediaStreamTrack::ContentHintType type);

SerializedReadyState SerializeReadyState(MediaStreamSource::ReadyState state);

WebMediaStreamTrack::ContentHintType DeserializeContentHint(
    SerializedContentHintType type);

MediaStreamSource::ReadyState DeserializeReadyState(SerializedReadyState state);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_SERIALIZATION_SERIALIZED_TRACK_PARAMS_H_
