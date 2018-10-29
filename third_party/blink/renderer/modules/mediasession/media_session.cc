// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/media_session.h"

#include <memory>
#include "base/optional.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_session_action_handler.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediasession/media_metadata.h"
#include "third_party/blink/renderer/modules/mediasession/media_metadata_sanitizer.h"

namespace blink {

namespace {

using ::blink::mojom::blink::MediaSessionAction;

const AtomicString& MojomActionToActionName(MediaSessionAction action) {
  DEFINE_STATIC_LOCAL(const AtomicString, play_action_name, ("play"));
  DEFINE_STATIC_LOCAL(const AtomicString, pause_action_name, ("pause"));
  DEFINE_STATIC_LOCAL(const AtomicString, previous_track_action_name,
                      ("previoustrack"));
  DEFINE_STATIC_LOCAL(const AtomicString, next_track_action_name,
                      ("nexttrack"));
  DEFINE_STATIC_LOCAL(const AtomicString, seek_backward_action_name,
                      ("seekbackward"));
  DEFINE_STATIC_LOCAL(const AtomicString, seek_forward_action_name,
                      ("seekforward"));

  switch (action) {
    case MediaSessionAction::PLAY:
      return play_action_name;
    case MediaSessionAction::PAUSE:
      return pause_action_name;
    case MediaSessionAction::PREVIOUS_TRACK:
      return previous_track_action_name;
    case MediaSessionAction::NEXT_TRACK:
      return next_track_action_name;
    case MediaSessionAction::SEEK_BACKWARD:
      return seek_backward_action_name;
    case MediaSessionAction::SEEK_FORWARD:
      return seek_forward_action_name;
    default:
      NOTREACHED();
  }
  return WTF::g_empty_atom;
}

base::Optional<MediaSessionAction> ActionNameToMojomAction(
    const String& action_name) {
  if ("play" == action_name)
    return MediaSessionAction::PLAY;
  if ("pause" == action_name)
    return MediaSessionAction::PAUSE;
  if ("previoustrack" == action_name)
    return MediaSessionAction::PREVIOUS_TRACK;
  if ("nexttrack" == action_name)
    return MediaSessionAction::NEXT_TRACK;
  if ("seekbackward" == action_name)
    return MediaSessionAction::SEEK_BACKWARD;
  if ("seekforward" == action_name)
    return MediaSessionAction::SEEK_FORWARD;

  NOTREACHED();
  return base::nullopt;
}

const AtomicString& MediaSessionPlaybackStateToString(
    mojom::blink::MediaSessionPlaybackState state) {
  DEFINE_STATIC_LOCAL(const AtomicString, none_value, ("none"));
  DEFINE_STATIC_LOCAL(const AtomicString, paused_value, ("paused"));
  DEFINE_STATIC_LOCAL(const AtomicString, playing_value, ("playing"));

  switch (state) {
    case mojom::blink::MediaSessionPlaybackState::NONE:
      return none_value;
    case mojom::blink::MediaSessionPlaybackState::PAUSED:
      return paused_value;
    case mojom::blink::MediaSessionPlaybackState::PLAYING:
      return playing_value;
  }
  NOTREACHED();
  return WTF::g_empty_atom;
}

mojom::blink::MediaSessionPlaybackState StringToMediaSessionPlaybackState(
    const String& state_name) {
  if (state_name == "none")
    return mojom::blink::MediaSessionPlaybackState::NONE;
  if (state_name == "paused")
    return mojom::blink::MediaSessionPlaybackState::PAUSED;
  DCHECK_EQ(state_name, "playing");
  return mojom::blink::MediaSessionPlaybackState::PLAYING;
}

}  // anonymous namespace

MediaSession::MediaSession(ExecutionContext* execution_context)
    : ContextClient(execution_context),
      playback_state_(mojom::blink::MediaSessionPlaybackState::NONE),
      client_binding_(this) {}

MediaSession* MediaSession::Create(ExecutionContext* execution_context) {
  return new MediaSession(execution_context);
}

void MediaSession::Dispose() {
  client_binding_.Close();
}

void MediaSession::setPlaybackState(const String& playback_state) {
  playback_state_ = StringToMediaSessionPlaybackState(playback_state);
  mojom::blink::MediaSessionService* service = GetService();
  if (service)
    service->SetPlaybackState(playback_state_);
}

String MediaSession::playbackState() {
  return MediaSessionPlaybackStateToString(playback_state_);
}

void MediaSession::setMetadata(MediaMetadata* metadata) {
  if (metadata)
    metadata->SetSession(this);

  if (metadata_)
    metadata_->SetSession(nullptr);

  metadata_ = metadata;
  OnMetadataChanged();
}

MediaMetadata* MediaSession::metadata() const {
  return metadata_;
}

void MediaSession::OnMetadataChanged() {
  mojom::blink::MediaSessionService* service = GetService();
  if (!service)
    return;

  service->SetMetadata(MediaMetadataSanitizer::SanitizeAndConvertToMojo(
      metadata_, GetExecutionContext()));
}

void MediaSession::setActionHandler(const String& action,
                                    V8MediaSessionActionHandler* handler) {
  if (handler) {
    auto add_result = action_handlers_.Set(action, handler);

    if (!add_result.is_new_entry)
      return;

    NotifyActionChange(action, ActionChangeType::kActionEnabled);
  } else {
    if (action_handlers_.find(action) == action_handlers_.end())
      return;

    action_handlers_.erase(action);

    NotifyActionChange(action, ActionChangeType::kActionDisabled);
  }
}

void MediaSession::NotifyActionChange(const String& action,
                                      ActionChangeType type) {
  mojom::blink::MediaSessionService* service = GetService();
  if (!service)
    return;

  auto mojom_action = ActionNameToMojomAction(action);
  DCHECK(mojom_action.has_value());

  switch (type) {
    case ActionChangeType::kActionEnabled:
      service->EnableAction(mojom_action.value());
      break;
    case ActionChangeType::kActionDisabled:
      service->DisableAction(mojom_action.value());
      break;
  }
}

mojom::blink::MediaSessionService* MediaSession::GetService() {
  if (service_)
    return service_.get();
  if (!GetExecutionContext())
    return nullptr;

  Document* document = To<Document>(GetExecutionContext());
  LocalFrame* frame = document->GetFrame();
  if (!frame)
    return nullptr;

  frame->GetInterfaceProvider().GetInterface(mojo::MakeRequest(&service_));
  if (service_.get()) {
    // Record the eTLD+1 of the frame using the API.
    Platform::Current()->RecordRapporURL("Media.Session.APIUsage.Origin",
                                         document->Url());
    blink::mojom::blink::MediaSessionClientPtr client;
    client_binding_.Bind(mojo::MakeRequest(&client));
    service_->SetClient(std::move(client));
  }

  return service_.get();
}

void MediaSession::DidReceiveAction(
    blink::mojom::blink::MediaSessionAction action) {
  Document* document = To<Document>(GetExecutionContext());
  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(document ? document->GetFrame()
                                                : nullptr);

  auto iter = action_handlers_.find(MojomActionToActionName(action));
  if (iter == action_handlers_.end())
    return;

  iter->value->InvokeAndReportException(this);
}

void MediaSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(metadata_);
  visitor->Trace(action_handlers_);
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
