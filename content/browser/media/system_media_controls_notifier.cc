// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/system_media_controls_notifier.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "components/system_media_controls/system_media_controls.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

#if defined(OS_WIN)
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/base/idle/idle.h"
#endif  // defined(OS_WIN)

namespace content {

using PlaybackStatus =
    system_media_controls::SystemMediaControls::PlaybackStatus;

const int kMinImageSize = 71;
const int kDesiredImageSize = 150;

#if defined(OS_WIN)
constexpr base::TimeDelta kScreenLockPollInterval =
    base::TimeDelta::FromSeconds(1);
constexpr int kHideSmtcDelaySeconds = 5;
constexpr base::TimeDelta kHideSmtcDelay =
    base::TimeDelta::FromSeconds(kHideSmtcDelaySeconds);
#endif  // defined(OS_WIN)

SystemMediaControlsNotifier::SystemMediaControlsNotifier(
    service_manager::Connector* connector,
    system_media_controls::SystemMediaControls* system_media_controls)
    : system_media_controls_(system_media_controls) {
  DCHECK(system_media_controls_);

#if defined(OS_WIN)
  lock_polling_timer_.Start(
      FROM_HERE, kScreenLockPollInterval,
      base::BindRepeating(&SystemMediaControlsNotifier::CheckLockState,
                          base::Unretained(this)));
#endif  // defined(OS_WIN)

  // |connector| can be null in tests.
  if (!connector)
    return;

  // Connect to the MediaControllerManager and create a MediaController that
  // controls the active session so we can observe it.
  mojo::Remote<media_session::mojom::MediaControllerManager> controller_manager;
  connector->Connect(media_session::mojom::kServiceName,
                     controller_manager.BindNewPipeAndPassReceiver());
  controller_manager->CreateActiveMediaController(
      media_controller_.BindNewPipeAndPassReceiver());

  // Observe the active media controller for changes to playback state and
  // supported actions.
  media_controller_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());

  // Observe the active media controller for changes to provided artwork.
  media_controller_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kArtwork, kMinImageSize,
      kDesiredImageSize,
      media_controller_image_observer_receiver_.BindNewPipeAndPassRemote());
}

SystemMediaControlsNotifier::~SystemMediaControlsNotifier() = default;

void SystemMediaControlsNotifier::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info_ptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if defined(OS_WIN)
  bool is_playing = false;
#endif  // defined(OS_WIN)

  session_info_ptr_ = std::move(session_info_ptr);
  if (session_info_ptr_) {
    if (session_info_ptr_->playback_state ==
        media_session::mojom::MediaPlaybackState::kPlaying) {
#if defined(OS_WIN)
      is_playing = true;
#endif  // defined(OS_WIN)
      system_media_controls_->SetPlaybackStatus(PlaybackStatus::kPlaying);
    } else {
      system_media_controls_->SetPlaybackStatus(PlaybackStatus::kPaused);
    }
  } else {
    system_media_controls_->SetPlaybackStatus(PlaybackStatus::kStopped);

    // These steps reference the Media Session Standard
    // https://wicg.github.io/mediasession/#metadata
    // 5.3.1 If the active media session is null, unset the media metadata
    // presented to the platform, and terminate these steps.
    system_media_controls_->ClearMetadata();
  }

#if defined(OS_WIN)
  if (screen_locked_) {
    if (is_playing)
      StopHideSmtcTimer();
    else if (!hide_smtc_timer_.IsRunning())
      StartHideSmtcTimer();
  }
#endif  // defined(OS_WIN)
}

void SystemMediaControlsNotifier::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (metadata.has_value()) {
    // 5.3.3 Update the media metadata presented to the platform to match the
    // metadata for the active media session.
    // If no title was provided, the title of the tab will be in the title
    // property.
    system_media_controls_->SetTitle(metadata->title);

    // If no artist was provided, then the source URL will be in the artist
    // property.
    system_media_controls_->SetArtist(metadata->artist);
    system_media_controls_->UpdateDisplay();
  } else {
    // 5.3.2 If the metadata of the active media session is an empty metadata,
    // unset the media metadata presented to the platform.
    system_media_controls_->ClearMetadata();
  }
}

void SystemMediaControlsNotifier::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!bitmap.empty()) {
    // 5.3.4.4.3 If the image format is supported, use the image as the artwork
    // for display in the platform UI. Otherwise the fetch image algorithm fails
    // and terminates.
    system_media_controls_->SetThumbnail(bitmap);
  } else {
    // 5.3.4.2 If metadata's artwork is empty, terminate these steps.
    // If no images are fetched in the fetch image algorithm, the user agent
    // may have fallback behavior such as displaying a default image as artwork.
    // We display the application icon if no artwork is provided.
    base::Optional<gfx::ImageSkia> icon =
        GetContentClient()->browser()->GetProductLogo();
    if (icon.has_value())
      system_media_controls_->SetThumbnail(*icon->bitmap());
    else
      system_media_controls_->ClearThumbnail();
  }
}

#if defined(OS_WIN)
void SystemMediaControlsNotifier::CheckLockState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool new_state = ui::CheckIdleStateIsLocked();
  if (screen_locked_ == new_state)
    return;

  screen_locked_ = new_state;
  if (screen_locked_)
    OnScreenLocked();
  else
    OnScreenUnlocked();
}

void SystemMediaControlsNotifier::OnScreenLocked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If media is currently playing, don't hide the SMTC.
  if (session_info_ptr_ &&
      session_info_ptr_->playback_state ==
          media_session::mojom::MediaPlaybackState::kPlaying) {
    return;
  }

  // Otherwise, hide them.
  system_media_controls_->SetEnabled(false);
}

void SystemMediaControlsNotifier::OnScreenUnlocked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  StopHideSmtcTimer();
  system_media_controls_->SetEnabled(true);
}

void SystemMediaControlsNotifier::StartHideSmtcTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hide_smtc_timer_.Start(
      FROM_HERE, kHideSmtcDelay,
      base::BindOnce(&SystemMediaControlsNotifier::HideSmtcTimerFired,
                     base::Unretained(this)));
}

void SystemMediaControlsNotifier::StopHideSmtcTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hide_smtc_timer_.Stop();
}

void SystemMediaControlsNotifier::HideSmtcTimerFired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  system_media_controls_->SetEnabled(false);
}
#endif  // defined(OS_WIN)

}  // namespace content
