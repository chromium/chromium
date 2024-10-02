// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_video_stats.h"
#include "third_party/blink/renderer/modules/mediastream/transferred_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"

namespace blink {

namespace {

class GetOpenDeviceRequestCallbacks final : public UserMediaRequest::Callbacks {
 public:
  ~GetOpenDeviceRequestCallbacks() override = default;

  void OnSuccess(const MediaStreamVector& streams,
                 CaptureController* capture_controller) override {}
  void OnError(ScriptWrappable* callback_this_value,
               const V8MediaStreamError* error,
               CaptureController* capture_controller,
               UserMediaRequestResult result) override {}
};

}  // namespace

MediaStreamTrack::MediaStreamTrack()
    : ActiveScriptWrappable<MediaStreamTrack>({}) {}

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
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
  return String();
}

// static
MediaStreamTrack* MediaStreamTrack::FromTransferredState(
    ScriptState* script_state,
    const TransferredValues& data) {
  DCHECK(data.track_impl_subtype);

  // Allow injecting a mock.
  if (GetFromTransferredStateImplForTesting()) {
    return GetFromTransferredStateImplForTesting().Run(data);
  }

  auto* window =
      DynamicTo<LocalDOMWindow>(ExecutionContext::From(script_state));
  if (!window) {
    return nullptr;
  }

  UserMediaClient* user_media_client = UserMediaClient::From(window);
  if (!user_media_client) {
    return nullptr;
  }

  // TODO(1288839): Set media_type, options, callbacks, surface appropriately
  MediaConstraints audio = (data.kind == "audio")
                               ? media_constraints_impl::Create()
                               : MediaConstraints();
  MediaConstraints video = (data.kind == "video")
                               ? media_constraints_impl::Create()
                               : MediaConstraints();
  UserMediaRequest* const request = MakeGarbageCollected<UserMediaRequest>(
      window, user_media_client, UserMediaRequestType::kDisplayMedia, audio,
      video, /*should_prefer_current_tab=*/false,
      /*capture_controller=*/nullptr,
      MakeGarbageCollected<GetOpenDeviceRequestCallbacks>(),
      IdentifiableSurface());
  if (!request) {
    return nullptr;
  }

  // TODO(1288839): Create a TransferredMediaStreamTrack implementing interfaces
  // supporting BrowserCaptureMediaStreamTrack operations when needed (or
  // support these behaviors in some other way).
  TransferredMediaStreamTrack* transferred_media_stream_track =
      MakeGarbageCollected<TransferredMediaStreamTrack>(
          ExecutionContext::From(script_state), data);

  request->SetTransferData(data.session_id, data.transfer_id,
                           transferred_media_stream_track);
  request->Start();

  // TODO(1288839): get rid of TransferredMediaStreamTrack, since it's just a
  // container for the impl track
  auto* track = transferred_media_stream_track->track();
  // TODO(1288839): What happens if GetOpenDevice fails?
  DCHECK(track);
  if (track->GetWrapperTypeInfo() != data.track_impl_subtype) {
    NOTREACHED_IN_MIGRATION()
        << "transferred track should be "
        << data.track_impl_subtype->interface_name << " but instead it is "
        << track->GetWrapperTypeInfo()->interface_name;
    return nullptr;
  }
  return track;
}

// static
MediaStreamTrack::FromTransferredStateImplForTesting&
MediaStreamTrack::GetFromTransferredStateImplForTesting() {
  static base::NoDestructor<
      MediaStreamTrack::FromTransferredStateImplForTesting>
      impl;
  return *impl;
}

}  // namespace blink
