// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/media_session.h"

#include <memory>

#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_position_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_session_action_details.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_session_action_handler.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_session_seek_to_action_details.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/mediasession/media_metadata.h"
#include "third_party/blink/renderer/modules/mediasession/media_metadata_sanitizer.h"
#include "third_party/blink/renderer/modules/mediasession/media_session_type_converters.h"
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
  DEFINE_STATIC_LOCAL(const AtomicString, toggle_microphone_action_name,
                      ("togglemicrophone"));
  DEFINE_STATIC_LOCAL(const AtomicString, toggle_camera_action_name,
                      ("togglecamera"));
  DEFINE_STATIC_LOCAL(const AtomicString, hang_up_action_name, ("hangup"));
  DEFINE_STATIC_LOCAL(const AtomicString, previous_slide_action_name,
                      ("previousslide"));
  DEFINE_STATIC_LOCAL(const AtomicString, next_slide_action_name,
                      ("nextslide"));
  DEFINE_STATIC_LOCAL(const AtomicString, enter_picture_in_picture_action_name,
                      ("enterpictureinpicture"));

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
    case MediaSessionAction::kToggleMicrophone:
      return toggle_microphone_action_name;
    case MediaSessionAction::kToggleCamera:
      return toggle_camera_action_name;
    case MediaSessionAction::kHangUp:
      return hang_up_action_name;
    case MediaSessionAction::kPreviousSlide:
      return previous_slide_action_name;
    case MediaSessionAction::kNextSlide:
      return next_slide_action_name;
    case MediaSessionAction::kEnterPictureInPicture:
      return enter_picture_in_picture_action_name;
    default:
      NOTREACHED();
  }
  return WTF::g_empty_atom;
}

absl::optional<MediaSessionAction> ActionNameToMojomAction(
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
  if ("togglemicrophone" == action_name)
    return MediaSessionAction::kToggleMicrophone;
  if ("togglecamera" == action_name)
    return MediaSessionAction::kToggleCamera;
  if ("hangup" == action_name)
    return MediaSessionAction::kHangUp;
  if ("previousslide" == action_name)
    return MediaSessionAction::kPreviousSlide;
  if ("nextslide" == action_name)
    return MediaSessionAction::kNextSlide;
  if ("enterpictureinpicture" == action_name) {
    return MediaSessionAction::kEnterPictureInPicture;
  }

  NOTREACHED();
  return absl::nullopt;
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

const char MediaSession::kSupplementName[] = "MediaSession";

MediaSession* MediaSession::mediaSession(Navigator& navigator) {
  MediaSession* supplement =
      Supplement<Navigator>::From<MediaSession>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<MediaSession>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

MediaSession::MediaSession(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      clock_(base::DefaultTickClock::GetInstance()),
      playback_state_(mojom::blink::MediaSessionPlaybackState::NONE),
      service_(navigator.GetExecutionContext()),
      client_receiver_(this, navigator.DomWindow()) {}

void MediaSession::setPlaybackState(const String& playback_state) {
  playback_state_ = StringToMediaSessionPlaybackState(playback_state);

  RecalculatePositionState(/*was_set=*/false);

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
  return metadata_.Get();
}

void MediaSession::OnMetadataChanged() {
  mojom::blink::MediaSessionService* service = GetService();
  if (!service)
    return;

  // OnMetadataChanged() is called from a timer. The Window/ExecutionContext
  // might detaches in the meantime. See https://crbug.com/1269522
  ExecutionContext* context = GetSupplementable()->DomWindow();
  if (!context)
    return;

  service->SetMetadata(
      MediaMetadataSanitizer::SanitizeAndConvertToMojo(metadata_, context));
}

void MediaSession::setActionHandler(const String& action,
                                    V8MediaSessionActionHandler* handler,
                                    ExceptionState& exception_state) {
  if (action == "skipad") {
    LocalDOMWindow* window = GetSupplementable()->DomWindow();
    if (!RuntimeEnabledFeatures::SkipAdEnabled(window)) {
      exception_state.ThrowTypeError(
          "The provided value 'skipad' is not a valid enum "
          "value of type MediaSessionAction.");
      return;
    }

    UseCounter::Count(window, WebFeature::kMediaSessionSkipAd);
  }

  if (!RuntimeEnabledFeatures::MediaSessionSlidesEnabled()) {
    if ("previousslide" == action || "nextslide" == action) {
      exception_state.ThrowTypeError("The provided value '" + action +
                                     "' is not a valid enum "
                                     "value of type MediaSessionAction.");
      return;
    }
  }

  if (!RuntimeEnabledFeatures::MediaSessionEnterPictureInPictureEnabled()) {
    if ("enterpictureinpicture" == action) {
      exception_state.ThrowTypeError(
          "The provided value 'enterpictureinpicture'"
          " is not a valid enum "
          "value of type MediaSessionAction.");
      return;
    }
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

  // The playback rate cannot be equal to zero.
  if (position_state->hasPlaybackRate() &&
      position_state->playbackRate() == 0) {
    exception_state.ThrowTypeError(
        "The provided playbackRate cannot be equal to zero.");
    return;
  }

  position_state_ =
      mojo::ConvertTo<media_session::mojom::blink::MediaPositionPtr>(
          position_state);

  declared_playback_rate_ = position_state_->playback_rate;

  RecalculatePositionState(/*was_set=*/true);
}

void MediaSession::setMicrophoneActive(bool active) {
  auto* service = GetService();
  if (!service)
    return;

  if (active) {
    service->SetMicrophoneState(
        media_session::mojom::MicrophoneState::kUnmuted);
  } else {
    service->SetMicrophoneState(media_session::mojom::MicrophoneState::kMuted);
  }
}

void MediaSession::setCameraActive(bool active) {
  auto* service = GetService();
  if (!service)
    return;

  if (active) {
    service->SetCameraState(media_session::mojom::CameraState::kTurnedOn);
  } else {
    service->SetCameraState(media_session::mojom::CameraState::kTurnedOff);
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

base::TimeDelta MediaSession::GetPositionNow() const {
  const base::TimeTicks now = clock_->NowTicks();

  const base::TimeDelta elapsed_time =
      position_state_->playback_rate *
      (now - position_state_->last_updated_time);
  const base::TimeDelta updated_position =
      position_state_->position + elapsed_time;
  const base::TimeDelta start = base::Seconds(0);

  if (updated_position <= start)
    return start;
  else if (updated_position >= position_state_->duration)
    return position_state_->duration;
  else
    return updated_position;
}

void MediaSession::RecalculatePositionState(bool was_set) {
  if (!position_state_)
    return;

  double new_playback_rate =
      playback_state_ == mojom::blink::MediaSessionPlaybackState::PAUSED
          ? 0.0
          : declared_playback_rate_;

  if (!was_set && new_playback_rate == position_state_->playback_rate)
    return;

  // If we updated the position state because of the playback rate then we
  // should update the time.
  if (!was_set) {
    position_state_->position = GetPositionNow();
  }

  position_state_->playback_rate = new_playback_rate;
  position_state_->last_updated_time = clock_->NowTicks();

  if (auto* service = GetService())
    service->SetPositionState(position_state_.Clone());
}

mojom::blink::MediaSessionService* MediaSession::GetService() {
  if (service_) {
    return service_.get();
  }
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!window) {
    return nullptr;
  }

  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner = window->GetTaskRunner(TaskType::kMiscPlatformAPI);
  window->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  if (service_.get())
    service_->SetClient(client_receiver_.BindNewPipeAndPassRemote(task_runner));
  return service_.get();
}

void MediaSession::DidReceiveAction(
    media_session::mojom::blink::MediaSessionAction action,
    mojom::blink::MediaSessionActionDetailsPtr details) {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!window)
    return;
  LocalFrame::NotifyUserActivation(
      window->GetFrame(),
      mojom::blink::UserActivationNotificationType::kInteraction);

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

void MediaSession::Trace(Visitor* visitor) const {
  visitor->Trace(client_receiver_);
  visitor->Trace(metadata_);
  visitor->Trace(action_handlers_);
  visitor->Trace(service_);
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
