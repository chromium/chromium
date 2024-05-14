// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/serialization/serialized_track_params.h"
#include "third_party/blink/renderer/modules/breakout_box/media_stream_track_generator.h"
#include "third_party/blink/renderer/modules/mediacapturefromelement/canvas_capture_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"

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
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
  return SerializedReadyState::kReadyStateEnded;
}

SerializedTrackImplSubtype SerializeTrackImplSubtype(
    ScriptWrappable::TypeDispatcher& dispatcher) {
  if (dispatcher.ToMostDerived<MediaStreamTrack>()) {
    return SerializedTrackImplSubtype::kTrackImplSubtypeBase;
  } else if (dispatcher.ToMostDerived<CanvasCaptureMediaStreamTrack>()) {
    return SerializedTrackImplSubtype::kTrackImplSubtypeCanvasCapture;
  } else if (dispatcher.ToMostDerived<MediaStreamTrackGenerator>()) {
    return SerializedTrackImplSubtype::kTrackImplSubtypeGenerator;
  } else if (dispatcher.ToMostDerived<BrowserCaptureMediaStreamTrack>()) {
    return SerializedTrackImplSubtype::kTrackImplSubtypeBrowserCapture;
  }
  auto* wrapper_type_info =
      dispatcher.DowncastTo<MediaStreamTrack>()->GetWrapperTypeInfo();
  LOG(FATAL) << "SerializeTrackImplSubtype is missing a case for "
             << wrapper_type_info->interface_name;
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

const WrapperTypeInfo* DeserializeTrackImplSubtype(
    SerializedTrackImplSubtype type) {
  switch (type) {
    case SerializedTrackImplSubtype::kTrackImplSubtypeBase:
      return MediaStreamTrack::GetStaticWrapperTypeInfo();
    case SerializedTrackImplSubtype::kTrackImplSubtypeCanvasCapture:
      return CanvasCaptureMediaStreamTrack::GetStaticWrapperTypeInfo();
    case SerializedTrackImplSubtype::kTrackImplSubtypeGenerator:
      return MediaStreamTrackGenerator::GetStaticWrapperTypeInfo();
    case SerializedTrackImplSubtype::kTrackImplSubtypeBrowserCapture:
      return BrowserCaptureMediaStreamTrack::GetStaticWrapperTypeInfo();
  }
}

}  // namespace blink
