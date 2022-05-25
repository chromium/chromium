// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/mediastream/media_error_state.h"
#include "third_party/blink/renderer/modules/mediastream/transferred_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_controller.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"

namespace blink {

namespace {

class GetOpenDeviceRequestCallbacks final : public UserMediaRequest::Callbacks {
 public:
  ~GetOpenDeviceRequestCallbacks() override = default;

  void OnSuccess(const MediaStreamVector& streams) override {}
  void OnError(ScriptWrappable* callback_this_value,
               const V8MediaStreamError* error) override {}
};

}  // namespace

String ContentHintToString(
    const WebMediaStreamTrack::ContentHintType& content_hint) {
  switch (content_hint) {
    case WebMediaStreamTrack::ContentHintType::kNone:
      return kContentHintStringNone;
    case WebMediaStreamTrack::ContentHintType::kAudioSpeech:
      return kContentHintStringAudioSpeech;
    case WebMediaStreamTrack::ContentHintType::kAudioMusic:
      return kContentHintStringAudioMusic;
    case WebMediaStreamTrack::ContentHintType::kVideoMotion:
      return kContentHintStringVideoMotion;
    case WebMediaStreamTrack::ContentHintType::kVideoDetail:
      return kContentHintStringVideoDetail;
    case WebMediaStreamTrack::ContentHintType::kVideoText:
      return kContentHintStringVideoText;
  }
  NOTREACHED();
  return kContentHintStringNone;
}

String ReadyStateToString(const MediaStreamSource::ReadyState& ready_state) {
  // Although muted is tracked as a ReadyState, only "live" and "ended" are
  // visible externally.
  switch (ready_state) {
    case MediaStreamSource::kReadyStateLive:
    case MediaStreamSource::kReadyStateMuted:
      return "live";
    case MediaStreamSource::kReadyStateEnded:
      return "ended";
  }
  NOTREACHED();
  return String();
}

// static
MediaStreamTrack* MediaStreamTrack::FromTransferredState(
    ScriptState* script_state,
    const TransferredValues& data) {
  auto* window =
      DynamicTo<LocalDOMWindow>(ExecutionContext::From(script_state));
  if (!window)
    return nullptr;

  UserMediaController* user_media = UserMediaController::From(window);
  MediaErrorState error_state;
  // TODO(1288839): Set media_type, options, callbacks, surface appropriately
  UserMediaRequest* request = UserMediaRequest::Create(
      window, user_media, UserMediaRequest::MediaType::kDisplayMedia,
      MediaStreamConstraints::Create(),
      MakeGarbageCollected<GetOpenDeviceRequestCallbacks>(), error_state,
      IdentifiableSurface());

  // TODO(1288839): Create a TransferredMediaStreamTrack implementing interfaces
  // supporting BrowserCaptureMediaStreamTrack or FocusableMediaStreamTrack
  // operations when needed (or support these behaviors in some other way).
  TransferredMediaStreamTrack* transferred_media_stream_track =
      MakeGarbageCollected<TransferredMediaStreamTrack>(
          ExecutionContext::From(script_state), data);

  request->SetTransferData(data.session_id, transferred_media_stream_track);
  request->Start();
  return transferred_media_stream_track;
}

}  // namespace blink
