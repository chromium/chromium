// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/serialization/serialized_track_params.h"

namespace blink {

SerializedContentHintType SerializeContentHint(
    WebMediaStreamTrack::ContentHintType type) {
  switch (type) {
    case WebMediaStreamTrack::ContentHintType::kNone:
      return SerializedContentHintType::kNone;
    case WebMediaStreamTrack::ContentHintType::kAudioSpeech:
      return SerializedContentHintType::kAudioSpeech;
    case WebMediaStreamTrack::ContentHintType::kAudioMusic:
      return SerializedContentHintType::kAudioMusic;
    case WebMediaStreamTrack::ContentHintType::kVideoMotion:
      return SerializedContentHintType::kVideoMotion;
    case WebMediaStreamTrack::ContentHintType::kVideoDetail:
      return SerializedContentHintType::kVideoDetail;
    case WebMediaStreamTrack::ContentHintType::kVideoText:
      return SerializedContentHintType::kVideoText;
  }
  // Exhaustive list of enum values of WebMediaStreamTrack::ContentHintType. If
  // new values are added in enum WebMediaStreamTrack::ContentHintType, then add
  // them here as well. Do not use default.
  NOTREACHED();
  return SerializedContentHintType::kNone;
}

SerializedReadyState SerializeReadyState(MediaStreamSource::ReadyState state) {
  switch (state) {
    case MediaStreamSource::kReadyStateLive:
      return SerializedReadyState::kReadyStateLive;
    case MediaStreamSource::kReadyStateMuted:
      return SerializedReadyState::kReadyStateMuted;
    case MediaStreamSource::kReadyStateEnded:
      return SerializedReadyState::kReadyStateEnded;
  }
  // Exhaustive list of enum values of MediaStreamSource::ReadyState. If new
  // values are added in enum MediaStreamSource::ReadyState, then add them here
  // as well. Do not use default.
  NOTREACHED();
  return SerializedReadyState::kReadyStateEnded;
}

WebMediaStreamTrack::ContentHintType DeserializeContentHint(
    SerializedContentHintType type) {
  switch (type) {
    case SerializedContentHintType::kNone:
      return WebMediaStreamTrack::ContentHintType::kNone;
    case SerializedContentHintType::kAudioSpeech:
      return WebMediaStreamTrack::ContentHintType::kAudioSpeech;
    case SerializedContentHintType::kAudioMusic:
      return WebMediaStreamTrack::ContentHintType::kAudioMusic;
    case SerializedContentHintType::kVideoMotion:
      return WebMediaStreamTrack::ContentHintType::kVideoMotion;
    case SerializedContentHintType::kVideoDetail:
      return WebMediaStreamTrack::ContentHintType::kVideoDetail;
    case SerializedContentHintType::kVideoText:
      return WebMediaStreamTrack::ContentHintType::kVideoText;
  }
}

MediaStreamSource::ReadyState DeserializeReadyState(
    SerializedReadyState state) {
  switch (state) {
    case SerializedReadyState::kReadyStateLive:
      return MediaStreamSource::kReadyStateLive;
    case SerializedReadyState::kReadyStateMuted:
      return MediaStreamSource::kReadyStateMuted;
    case SerializedReadyState::kReadyStateEnded:
      return MediaStreamSource::kReadyStateEnded;
  }
}

}  // namespace blink
