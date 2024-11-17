// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/media_session.h"

#include <memory>
#include <optional>

#include "base/notreached.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_position_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_session_action_details.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_session_action_handler.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_session_playback_state.h"
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

V8MediaSessionAction::Enum MojomActionToActionEnum(MediaSessionAction action) {
  switch (action) {
    case MediaSessionAction::kPlay:
      return V8MediaSessionAction::Enum::kPlay;
    case MediaSessionAction::kPause:
      return V8MediaSessionAction::Enum::kPause;
    case MediaSessionAction::kPreviousTrack:
      return V8MediaSessionAction::Enum::kPrevioustrack;
    case MediaSessionAction::kNextTrack:
      return V8MediaSessionAction::Enum::kNexttrack;
    case MediaSessionAction::kSeekBackward:
      return V8MediaSessionAction::Enum::kSeekbackward;
    case MediaSessionAction::kSeekForward:
      return V8MediaSessionAction::Enum::kSeekforward;
    case MediaSessionAction::kSkipAd:
      return V8MediaSessionAction::Enum::kSkipad;
    case MediaSessionAction::kStop:
      return V8MediaSessionAction::Enum::kStop;
    case MediaSessionAction::kSeekTo:
      return V8MediaSessionAction::Enum::kSeekto;
    case MediaSessionAction::kToggleMicrophone:
      return V8MediaSessionAction::Enum::kTogglemicrophone;
    case MediaSessionAction::kToggleCamera:
      return V8MediaSessionAction::Enum::kTogglecamera;
    case MediaSessionAction::kHangUp:
      return V8MediaSessionAction::Enum::kHangup;
    case MediaSessionAction::kPreviousSlide:
      return V8MediaSessionAction::Enum::kPreviousslide;
    case MediaSessionAction::kNextSlide:
      return V8MediaSessionAction::Enum::kNextslide;
    case MediaSessionAction::kEnterPictureInPicture:
      return V8MediaSessionAction::Enum::kEnterpictureinpicture;
    case MediaSessionAction::kScrubTo:
    case MediaSessionAction::kExitPictureInPicture:
    case MediaSessionAction::kSwitchAudioDevice:
    case MediaSessionAction::kEnterAutoPictureInPicture:
    case MediaSessionAction::kSetMute:
    case MediaSessionAction::kRaise:
      NOTREACHED();
  }
  NOTREACHED();
}

MediaSessionAction ActionEnumToMojomAction(V8MediaSessionAction::Enum action) {
  switch (action) {
    case V8MediaSessionAction::Enum::kPlay:
      return MediaSessionAction::kPlay;
    case V8MediaSessionAction::Enum::kPause:
      return MediaSessionAction::kPause;
    case V8MediaSessionAction::Enum::kPrevioustrack:
      return MediaSessionAction::kPreviousTrack;
    case V8MediaSessionAction::Enum::kNexttrack:
      return MediaSessionAction::kNextTrack;
    case V8MediaSessionAction::Enum::kSeekbackward:
      return MediaSessionAction::kSeekBackward;
    case V8MediaSessionAction::Enum::kSeekforward:
      return MediaSessionAction::kSeekForward;
    case V8MediaSessionAction::Enum::kSkipad:
      return MediaSessionAction::kSkipAd;
    case V8MediaSessionAction::Enum::kStop:
      return MediaSessionAction::kStop;
    case V8MediaSessionAction::Enum::kSeekto:
      return MediaSessionAction::kSeekTo;
    case V8MediaSessionAction::Enum::kTogglemicrophone:
      return MediaSessionAction::kToggleMicrophone;
    case V8MediaSessionAction::Enum::kTogglecamera:
      return MediaSessionAction::kToggleCamera;
    case V8MediaSessionAction::Enum::kHangup:
      return MediaSessionAction::kHangUp;
    case V8MediaSessionAction::Enum::kPreviousslide:
      return MediaSessionAction::kPreviousSlide;
    case V8MediaSessionAction::Enum::kNextslide:
      return MediaSessionAction::kNextSlide;
    case V8MediaSessionAction::Enum::kEnterpictureinpicture:
      return MediaSessionAction::kEnterPictureInPicture;
  }
  NOTREACHED();
}

V8MediaSessionPlaybackState::Enum MediaSessionPlaybackStateToEnum(
    mojom::blink::MediaSessionPlaybackState state) {
  switch (state) {
    case mojom::blink::MediaSessionPlaybackState::NONE:
      return V8MediaSessionPlaybackState::Enum::kNone;
    case mojom::blink::MediaSessionPlaybackState::PAUSED:
      return V8MediaSessionPlaybackState::Enum::kPaused;
    case mojom::blink::MediaSessionPlaybackState::PLAYING:
      return V8MediaSessionPlaybackState::Enum::kPlaying;
  }
  NOTREACHED();
}

mojom::blink::MediaSessionPlaybackState EnumToMediaSessionPlaybackState(
    const V8MediaSessionPlaybackState::Enum& state) {
  switch (state) {
    case V8MediaSessionPlaybackState::Enum::kNone:
      return mojom::blink::MediaSessionPlaybackState::NONE;
    case V8MediaSessionPlaybackState::Enum::kPaused:
      return mojom::blink::MediaSessionPlaybackState::PAUSED;
    case V8MediaSessionPlaybackState::Enum::kPlaying:
      return mojom::blink::MediaSessionPlaybackState::PLAYING;
  }
  NOTREACHED();
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

void MediaSession::setPlaybackState(
    const V8MediaSessionPlaybackState& playback_state) {
  playback_state_ = EnumToMediaSessionPlaybackState(playback_state.AsEnum());

  RecalculatePositionState(/*was_set=*/false);

  mojom::blink::MediaSessionService* service = GetService();
  if (service)
    service->SetPlaybackState(playback_state_);
}

V8MediaSessionPlaybackState MediaSession::playbackState() {
  return V8MediaSessionPlaybackState(
      MediaSessionPlaybackStateToEnum(playback_state_));
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

void MediaSession::setActionHandler(const V8MediaSessionAction& action,
                                    V8MediaSessionActionHandler* handler,
                                    ExceptionState& exception_state) {
  auto action_value = action.AsEnum();
  if (action_value == V8MediaSessionAction::Enum::kSkipad) {
    LocalDOMWindow* window = GetSupplementable()->DomWindow();
    if (!RuntimeEnabledFeatures::SkipAdEnabled(window)) {
      exception_state.ThrowTypeError(
          "The provided value 'skipad' is not a valid enum "
          "value of type MediaSessionAction.");
      return;
    }

    UseCounter::Count(window, WebFeature::kMediaSessionSkipAd);
  }

  if (!RuntimeEnabledFeatures::MediaSessionEnterPictureInPictureEnabled()) {
    if (action_value == V8MediaSessionAction::Enum::kEnterpictureinpicture) {
      exception_state.ThrowTypeError(
          "The provided value 'enterpictureinpicture'"
          " is not a valid enum "
          "value of type MediaSessionAction.");
      return;
    }
  }

  if (handler) {
    auto add_result = action_handlers_.Set(action_value, handler);

    if (!add_result.is_new_entry)
      return;

    NotifyActionChange(action_value, ActionChangeType::kActionEnabled);
  } else {
    if (action_handlers_.find(action_value) == action_handlers_.end()) {
      return;
    }

    action_handlers_.erase(action_value);

    NotifyActionChange(action_value, ActionChangeType::kActionDisabled);
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

  // The duration cannot be NaN.
  if (std::isnan(position_state->duration())) {
    exception_state.ThrowTypeError("The provided duration cannot be NaN.");
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

void MediaSession::NotifyActionChange(V8MediaSessionAction::Enum action,
                                      ActionChangeType type) {
  mojom::blink::MediaSessionService* service = GetService();
  if (!service)
    return;

  auto mojom_action = ActionEnumToMojomAction(action);
  switch (type) {
    case ActionChangeType::kActionEnabled:
      service->EnableAction(mojom_action);
      break;
    case ActionChangeType::kActionDisabled:
      service->DisableAction(mojom_action);
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

  auto v8_action = MojomActionToActionEnum(action);

  auto iter = action_handlers_.find(v8_action);
  if (iter == action_handlers_.end())
    return;

  const auto* blink_details =
      mojo::TypeConverter<const blink::MediaSessionActionDetails*,
                          blink::mojom::blink::MediaSessionActionDetailsPtr>::
          ConvertWithV8Action(details, v8_action);

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
