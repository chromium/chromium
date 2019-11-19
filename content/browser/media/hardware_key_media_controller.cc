// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/hardware_key_media_controller.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_keys_listener_manager_impl.h"
#include "content/public/browser/media_keys_listener_manager.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/media_keys_util.h"

namespace content {

using media_session::mojom::MediaSessionAction;

HardwareKeyMediaController::HardwareKeyMediaController(
    service_manager::Connector* connector) {
  // |connector| can be null in tests.
  if (!connector)
    return;

  // Connect to the MediaControllerManager and create a MediaController that
  // controls the active session.
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  connector->Connect(media_session::mojom::kServiceName,
                     controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_controller_remote_.BindNewPipeAndPassReceiver());

  // Observe the active media controller for changes to playback state and
  // supported actions.
  media_controller_remote_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());
}

HardwareKeyMediaController::~HardwareKeyMediaController() = default;

void HardwareKeyMediaController::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();
  DCHECK(media_keys_listener_manager_impl);

  session_info_ = std::move(session_info);
  media_keys_listener_manager_impl->SetIsMediaPlaying(
      session_info_ && session_info_->playback_state ==
                           media_session::mojom::MediaPlaybackState::kPlaying);
}

void HardwareKeyMediaController::MediaSessionActionsChanged(
    const std::vector<MediaSessionAction>& actions) {
  MediaKeysListenerManager* media_keys_listener_manager =
      MediaKeysListenerManager::GetInstance();
  DCHECK(media_keys_listener_manager);

  // Stop listening to any keys that are currently being watched, but aren't in
  // |actions|.
  for (const MediaSessionAction& action : actions_) {
    base::Optional<ui::KeyboardCode> action_key_code =
        MediaSessionActionToKeyCode(action);

    // We only store supported actions in |actions_|, so we should always get a
    // value from |MediaSessionActionToKeyCode()| here.
    DCHECK(action_key_code.has_value());
    if (std::find(actions.begin(), actions.end(), action) == actions.end())
      media_keys_listener_manager->StopWatchingMediaKey(*action_key_code, this);
  }

  // Populate |actions_| with the new MediaSessionActions and start listening
  // to necessary media keys.
  actions_.clear();
  for (const MediaSessionAction& action : actions) {
    base::Optional<ui::KeyboardCode> action_key_code =
        MediaSessionActionToKeyCode(action);
    if (action_key_code.has_value()) {
      // It's okay to call this even on keys we're already listening to, since
      // it's a no-op in that case.
      if (media_keys_listener_manager->StartWatchingMediaKey(*action_key_code,
                                                             this)) {
        actions_.insert(action);
      }
    }
  }
}

void HardwareKeyMediaController::FlushForTesting() {
  media_controller_remote_.FlushForTesting();
}

void HardwareKeyMediaController::OnMediaKeysAccelerator(
    const ui::Accelerator& accelerator) {
  // Ignore key released events.
  if (accelerator.key_state() == ui::Accelerator::KeyState::RELEASED)
    return;

  MediaSessionAction action =
      KeyCodeToMediaSessionAction(accelerator.key_code());

  // Ignore if we don't support the action.
  if (!SupportsAction(action))
    return;

  PerformAction(action);
}

bool HardwareKeyMediaController::SupportsAction(
    MediaSessionAction action) const {
  return actions_.contains(action);
}

void HardwareKeyMediaController::PerformAction(MediaSessionAction action) {
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
      ui::RecordMediaHardwareKeyAction(
          ui::MediaHardwareKeyAction::kPause);
      return;
    case MediaSessionAction::kNextTrack:
      media_controller_remote_->NextTrack();
      ui::RecordMediaHardwareKeyAction(
          ui::MediaHardwareKeyAction::kNextTrack);
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
      NOTREACHED();
      return;
  }
}

MediaSessionAction HardwareKeyMediaController::KeyCodeToMediaSessionAction(
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
      return MediaSessionAction::kPlay;
  }
}

base::Optional<ui::KeyboardCode>
HardwareKeyMediaController::MediaSessionActionToKeyCode(
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
      return base::nullopt;
  }
}

}  // namespace content
