// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/media_session.h"

#include <memory>
#include "base/optional.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_session_action_handler.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/mediasession/media_metadata.h"
#include "third_party/blink/renderer/modules/mediasession/media_metadata_sanitizer.h"
#include "third_party/blink/renderer/modules/mediasession/media_position_state.h"
#include "third_party/blink/renderer/modules/mediasession/media_session_action_details.h"
#include "third_party/blink/renderer/modules/mediasession/media_session_seek_to_action_details.h"
#include "third_party/blink/renderer/modules/mediasession/type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

using ::media_session::mojom::blink::MediaSessionAction;

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
  DEFINE_STATIC_LOCAL(const AtomicString, skip_ad_action_name, ("skipad"));
  DEFINE_STATIC_LOCAL(const AtomicString, stop_action_name, ("stop"));
  DEFINE_STATIC_LOCAL(const AtomicString, seek_to_action_name, ("seekto"));

  switch (action) {
    case MediaSessionAction::kPlay:
      return play_action_name;
    case MediaSessionAction::kPause:
      return pause_action_name;
    case MediaSessionAction::kPreviousTrack:
      return previous_track_action_name;
    case MediaSessionAction::kNextTrack:
      return next_track_action_name;
    case MediaSessionAction::kSeekBackward:
      return seek_backward_action_name;
    case MediaSessionAction::kSeekForward:
      return seek_forward_action_name;
    case MediaSessionAction::kSkipAd:
      return skip_ad_action_name;
    case MediaSessionAction::kStop:
      return stop_action_name;
    case MediaSessionAction::kSeekTo:
      return seek_to_action_name;
    default:
      NOTREACHED();
  }
  return WTF::g_empty_atom;
}

base::Optional<MediaSessionAction> ActionNameToMojomAction(
    const String& action_name) {
  if ("play" == action_name)
    return MediaSessionAction::kPlay;
  if ("pause" == action_name)
    return MediaSessionAction::kPause;
  if ("previoustrack" == action_name)
    return MediaSessionAction::kPreviousTrack;
  if ("nexttrack" == action_name)
    return MediaSessionAction::kNextTrack;
  if ("seekbackward" == action_name)
    return MediaSessionAction::kSeekBackward;
  if ("seekforward" == action_name)
    return MediaSessionAction::kSeekForward;
  if ("skipad" == action_name)
    return MediaSessionAction::kSkipAd;
  if ("stop" == action_name)
    return MediaSessionAction::kStop;
  if ("seekto" == action_name)
    return MediaSessionAction::kSeekTo;

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
      playback_state_(mojom::blink::MediaSessionPlaybackState::NONE) {}

void MediaSession::Dispose() {
  client_receiver_.reset();
}

void MediaSession::setPlaybackState(const String& playback_state) {
  playback_state_ = StringToMediaSessionPlaybackState(playback_state);

  RecalculatePositionState(false /* notify */);

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
                                    V8MediaSessionActionHandler* handler,
                                    ExceptionState& exception_state) {
  if (action == "skipad") {
    if (!RuntimeEnabledFeatures::SkipAdEnabled(GetExecutionContext())) {
      exception_state.ThrowTypeError(
          "The provided value 'skipad' is not a valid enum "
          "value of type MediaSessionAction.");
      return;
    }

    UseCounter::Count(GetExecutionContext(), WebFeature::kMediaSessionSkipAd);
  } else if (action == "seekto" &&
             !RuntimeEnabledFeatures::MediaSessionSeekingEnabled(
                 GetExecutionContext())) {
    exception_state.ThrowTypeError(
        "The provided value 'seekto' is not a valid enum "
        "value of type MediaSessionAction.");
    return;
  }

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

void MediaSession::setPositionState(MediaPositionState* position_state,
                                    ExceptionState& exception_state) {
  // If the dictionary is empty / null then we should reset the position state.
  if (!position_state->hasDuration() && !position_state->hasPlaybackRate() &&
      !position_state->hasPosition()) {
    position_state_ = nullptr;
    declared_playback_rate_ = 0.0;

    if (auto* service = GetService())
      service->SetPositionState(nullptr);

    return;
  }

  // The duration cannot be missing.
  if (!position_state->hasDuration()) {
    exception_state.ThrowTypeError("The duration must be provided.");
    return;
  }

  // The duration cannot be negative.
  if (position_state->duration() < 0) {
    exception_state.ThrowTypeError(
        "The provided duration cannot be less than zero.");
    return;
  }

  // The position cannot be negative.
  if (position_state->hasPosition() && position_state->position() < 0) {
    exception_state.ThrowTypeError(
        "The provided position cannot be less than zero.");
    return;
  }

  // The position cannot be greater than the duration.
  if (position_state->hasPosition() &&
      position_state->position() > position_state->duration()) {
    exception_state.ThrowTypeError(
        "The provided position cannot be greater than the duration.");
    return;
  }

  // The playback rate cannot be less than or equal to zero.
  if (position_state->hasPlaybackRate() &&
      position_state->playbackRate() <= 0) {
    exception_state.ThrowTypeError(
        "The provided playbackRate cannot be less than or equal to zero.");
    return;
  }

  position_state_ =
      mojo::ConvertTo<media_session::mojom::blink::MediaPositionPtr>(
          position_state);

  declared_playback_rate_ = position_state_->playback_rate;

  RecalculatePositionState(true /* notify */);
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

void MediaSession::RecalculatePositionState(bool notify) {
  if (!position_state_)
    return;

  double new_playback_rate =
      playback_state_ == mojom::blink::MediaSessionPlaybackState::PAUSED
          ? 0.0
          : declared_playback_rate_;

  notify = notify || new_playback_rate != position_state_->playback_rate;
  position_state_->playback_rate = new_playback_rate;

  if (!notify)
    return;

  if (auto* service = GetService())
    service->SetPositionState(position_state_.Clone());
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

  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  frame->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver());
  if (service_.get())
    service_->SetClient(client_receiver_.BindNewPipeAndPassRemote(task_runner));

  return service_.get();
}

void MediaSession::DidReceiveAction(
    media_session::mojom::blink::MediaSessionAction action,
    mojom::blink::MediaSessionActionDetailsPtr details) {
  Document* document = To<Document>(GetExecutionContext());
  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(document ? document->GetFrame()
                                                : nullptr);

  auto& name = MojomActionToActionName(action);

  auto iter = action_handlers_.find(name);
  if (iter == action_handlers_.end())
    return;

  const auto* blink_details =
      mojo::TypeConverter<const blink::MediaSessionActionDetails*,
                          blink::mojom::blink::MediaSessionActionDetailsPtr>::
          ConvertWithActionName(details, name);

  iter->value->InvokeAndReportException(this, blink_details);
}

void MediaSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(metadata_);
  visitor->Trace(action_handlers_);
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
