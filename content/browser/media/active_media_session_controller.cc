// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/active_media_session_controller.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/unguessable_token.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_keys_listener_manager_impl.h"
#include "content/public/browser/media_keys_listener_manager.h"
#include "content/public/browser/media_session_service.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/media_keys_util.h"

namespace content {

using media_session::mojom::MediaSessionAction;

ActiveMediaSessionController::ActiveMediaSessionController(
    base::UnguessableToken request_id)
    : request_id_(request_id) {
  // Connect to the MediaControllerManager and create a MediaController that
  // controls the session given by `request_id`.
  GetMediaSessionService().BindMediaControllerManager(
      controller_manager_remote_.BindNewPipeAndPassReceiver());

  if (request_id == base::UnguessableToken::Null()) {
    // ID is null for all scenarios where kWebAppSystemMediaControls is not
    // supported. ie. Linux always, mac/Windows with the feature flag off.
    // Create a media controller that follows the active session for this case.
    controller_manager_remote_->CreateActiveMediaController(
        media_controller_remote_.BindNewPipeAndPassReceiver());
  } else {
    // Create a media controller tied to |request_id| when
    // kWebAppSystemMediaControls is enabled (on Windows/macOS).
    controller_manager_remote_->CreateMediaControllerForSession(
        media_controller_remote_.BindNewPipeAndPassReceiver(), request_id);
  }

  // Observe the active media controller for changes to playback state and
  // supported actions.
  media_controller_remote_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());
}

ActiveMediaSessionController::~ActiveMediaSessionController() = default;

void ActiveMediaSessionController::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();
  DCHECK(media_keys_listener_manager_impl);

  session_info_ = std::move(session_info);
  media_keys_listener_manager_impl->SetIsMediaPlaying(
      session_info_ && session_info_->playback_state ==
                           media_session::mojom::MediaPlaybackState::kPlaying);
}

void ActiveMediaSessionController::MediaSessionActionsChanged(
    const std::vector<MediaSessionAction>& actions) {
  MediaKeysListenerManager* media_keys_listener_manager =
      MediaKeysListenerManager::GetInstance();
  DCHECK(media_keys_listener_manager);

  // Stop listening to any keys that are currently being watched, but aren't in
  // |actions|.
  // This loop is what tells the media keys listener manager to stop watching
  // next/previous when a new tab is active because next/previous are in
  // actions_ but NOT in |actions|.
  for (const MediaSessionAction& action : actions_) {
    std::optional<ui::KeyboardCode> action_key_code =
        MediaSessionActionToKeyCode(action);
    if (!action_key_code.has_value())
      continue;
    if (!base::Contains(actions, action))
      media_keys_listener_manager->StopWatchingMediaKey(*action_key_code, this,
                                                        request_id_);
  }

  // Populate |actions_| with the new MediaSessionActions and start listening
  // to necessary media keys.
  actions_.clear();
  for (const MediaSessionAction& action : actions) {
    std::optional<ui::KeyboardCode> action_key_code =
        MediaSessionActionToKeyCode(action);
    if (action_key_code.has_value()) {
      // It's okay to call this even on keys we're already listening to, since
      // it's a no-op in that case.
      if (media_keys_listener_manager->StartWatchingMediaKey(
              *action_key_code, this, request_id_)) {
        actions_.insert(action);
      }
    } else {
      // If there is no media key associated with this action, then just add it
      // to the list of actions we listen to (since we can receive certain
      // non-key actions like SeekTo).
      actions_.insert(action);
    }
  }
}

void ActiveMediaSessionController::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {
  position_ = position;
}

void ActiveMediaSessionController::FlushForTesting() {
  media_controller_remote_.FlushForTesting();  // IN-TEST
}

void ActiveMediaSessionController::OnMediaKeysAccelerator(
    const ui::Accelerator& accelerator) {
  // Ignore key released events.
  if (accelerator.key_state() == ui::Accelerator::KeyState::RELEASED)
    return;

  MaybePerformAction(KeyCodeToMediaSessionAction(accelerator.key_code()));
}

void ActiveMediaSessionController::OnNext() {
  MaybePerformAction(MediaSessionAction::kNextTrack);
}

void ActiveMediaSessionController::OnPrevious() {
  MaybePerformAction(MediaSessionAction::kPreviousTrack);
}

void ActiveMediaSessionController::OnPlay() {
  MaybePerformAction(MediaSessionAction::kPlay);
}

void ActiveMediaSessionController::OnPause() {
  MaybePerformAction(MediaSessionAction::kPause);
}

void ActiveMediaSessionController::OnPlayPause() {
  if (session_info_ && session_info_->playback_state ==
                           media_session::mojom::MediaPlaybackState::kPlaying) {
    MaybePerformAction(MediaSessionAction::kPause);
    return;
  }
  MaybePerformAction(MediaSessionAction::kPlay);
}

void ActiveMediaSessionController::OnStop() {
  MaybePerformAction(MediaSessionAction::kStop);
}

void ActiveMediaSessionController::OnSeek(const base::TimeDelta& time) {
  media_controller_remote_->Seek(time);
}

void ActiveMediaSessionController::OnSeekTo(const base::TimeDelta& time) {
  if (base::Contains(actions_,
                     media_session::mojom::MediaSessionAction::kSeekTo)) {
    media_controller_remote_->SeekTo(time);
  } else if (position_) {
    auto time_diff =
        time - position_->GetPositionAtTime(base::TimeTicks::Now());
    media_controller_remote_->Seek(time_diff);
  }
}

void ActiveMediaSessionController::MaybePerformAction(
    MediaSessionAction action) {
  // Ignore if we don't support the action.
  if (!SupportsAction(action))
    return;

  PerformAction(action);
}

bool ActiveMediaSessionController::SupportsAction(
    MediaSessionAction action) const {
  return actions_.contains(action);
}

void ActiveMediaSessionController::PerformAction(MediaSessionAction action) {
  DCHECK(SupportsAction(action));
  switch (action) {
    case MediaSessionAction::kPreviousTrack:
      media_controller_remote_->PreviousTrack();
      ui::RecordMediaHardwareKeyAction(
          ui::MediaHardwareKeyAction::kPreviousTrack);
      return;
    case MediaSessionAction::kPlay:
      media_controller_remote_->Resume();
      ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kPlay);
      return;
    case MediaSessionAction::kPause:
      media_controller_remote_->Suspend();
      ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kPause);
      return;
    case MediaSessionAction::kNextTrack:
      media_controller_remote_->NextTrack();
      ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kNextTrack);
      return;
    case MediaSessionAction::kStop:
      media_controller_remote_->Stop();
      ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kStop);
      return;
    case MediaSessionAction::kSeekBackward:
    case MediaSessionAction::kSeekForward:
    case MediaSessionAction::kSkipAd:
    case MediaSessionAction::kSeekTo:
    case MediaSessionAction::kScrubTo:
    case MediaSessionAction::kEnterPictureInPicture:
    case MediaSessionAction::kExitPictureInPicture:
    case MediaSessionAction::kSwitchAudioDevice:
    case MediaSessionAction::kToggleMicrophone:
    case MediaSessionAction::kToggleCamera:
    case MediaSessionAction::kHangUp:
    case MediaSessionAction::kRaise:
    case MediaSessionAction::kSetMute:
    case MediaSessionAction::kPreviousSlide:
    case MediaSessionAction::kNextSlide:
    case MediaSessionAction::kEnterAutoPictureInPicture:
      NOTREACHED();
  }
}

MediaSessionAction ActiveMediaSessionController::KeyCodeToMediaSessionAction(
    ui::KeyboardCode key_code) const {
  switch (key_code) {
    case ui::KeyboardCode::VKEY_MEDIA_PLAY_PAUSE:
      if (session_info_ &&
          session_info_->playback_state ==
              media_session::mojom::MediaPlaybackState::kPlaying) {
        return MediaSessionAction::kPause;
      }
      return MediaSessionAction::kPlay;
    case ui::KeyboardCode::VKEY_MEDIA_STOP:
      return MediaSessionAction::kStop;
    case ui::KeyboardCode::VKEY_MEDIA_NEXT_TRACK:
      return MediaSessionAction::kNextTrack;
    case ui::KeyboardCode::VKEY_MEDIA_PREV_TRACK:
      return MediaSessionAction::kPreviousTrack;
    default:
      NOTREACHED();
  }
}

std::optional<ui::KeyboardCode>
ActiveMediaSessionController::MediaSessionActionToKeyCode(
    MediaSessionAction action) const {
  switch (action) {
    case MediaSessionAction::kPlay:
    case MediaSessionAction::kPause:
      return ui::KeyboardCode::VKEY_MEDIA_PLAY_PAUSE;
    case MediaSessionAction::kStop:
      return ui::KeyboardCode::VKEY_MEDIA_STOP;
    case MediaSessionAction::kNextTrack:
      return ui::KeyboardCode::VKEY_MEDIA_NEXT_TRACK;
    case MediaSessionAction::kPreviousTrack:
      return ui::KeyboardCode::VKEY_MEDIA_PREV_TRACK;
    case MediaSessionAction::kSeekBackward:
    case MediaSessionAction::kSeekForward:
    case MediaSessionAction::kSkipAd:
    case MediaSessionAction::kSeekTo:
    case MediaSessionAction::kScrubTo:
    case MediaSessionAction::kEnterPictureInPicture:
    case MediaSessionAction::kExitPictureInPicture:
    case MediaSessionAction::kSwitchAudioDevice:
    case MediaSessionAction::kToggleMicrophone:
    case MediaSessionAction::kToggleCamera:
    case MediaSessionAction::kHangUp:
    case MediaSessionAction::kRaise:
    case MediaSessionAction::kSetMute:
    case MediaSessionAction::kPreviousSlide:
    case MediaSessionAction::kNextSlide:
    case MediaSessionAction::kEnterAutoPictureInPicture:
      return std::nullopt;
  }
}

}  // namespace content
