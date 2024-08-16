// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/system_media_controls_notifier.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/system_media_controls/system_media_controls.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_session_client.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/image/image_skia.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/idle/idle.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

using PlaybackStatus =
    system_media_controls::SystemMediaControls::PlaybackStatus;

const int kMinImageSize = 71;
const int kDesiredImageSize = 150;

#if BUILDFLAG(IS_WIN)
constexpr base::TimeDelta kScreenLockPollInterval = base::Seconds(1);
constexpr int kHideSmtcDelaySeconds = 5;
constexpr base::TimeDelta kHideSmtcDelay = base::Seconds(kHideSmtcDelaySeconds);
#endif  // BUILDFLAG(IS_WIN)

constexpr base::TimeDelta kDebounceDelay = base::Milliseconds(10);

SystemMediaControlsNotifier::SystemMediaControlsNotifier(
    system_media_controls::SystemMediaControls* system_media_controls,
    base::UnguessableToken request_id)
    : system_media_controls_(system_media_controls) {
  DCHECK(system_media_controls_);

#if BUILDFLAG(IS_WIN)
  lock_polling_timer_.Start(
      FROM_HERE, kScreenLockPollInterval,
      base::BindRepeating(&SystemMediaControlsNotifier::CheckLockState,
                          base::Unretained(this)));
#endif  // BUILDFLAG(IS_WIN)

  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  GetMediaSessionService().BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());

  if (request_id == base::UnguessableToken::Null()) {
    // Null ID for all scenarios where kWebAppSystemMediaControls is not
    // supported. ie. Linux always, and Mac/Windows when the feature flag off.
    // Create a media controller that follows the active session for this case.
    controller_manager_remote->CreateActiveMediaController(
        media_controller_remote_.BindNewPipeAndPassReceiver());
  } else {
    // Create a media controller tied to |request_id| when
    // kWebAppSystemMediaControls is enabled (on Windows or macOS).
    controller_manager_remote->CreateMediaControllerForSession(
        media_controller_remote_.BindNewPipeAndPassReceiver(), request_id);
  }

  // Observe the active media controller for changes to playback state and
  // supported actions.
  media_controller_remote_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());

  // Observe the active media controller for changes to provided artwork.
  media_controller_remote_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kArtwork, kMinImageSize,
      kDesiredImageSize,
      media_controller_image_observer_receiver_.BindNewPipeAndPassRemote());
}

SystemMediaControlsNotifier::~SystemMediaControlsNotifier() = default;

void SystemMediaControlsNotifier::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info_ptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool is_playing = false;

  session_info_ptr_ = std::move(session_info_ptr);
  if (session_info_ptr_) {
    is_playing = session_info_ptr_->playback_state ==
                 media_session::mojom::MediaPlaybackState::kPlaying;

    DebouncePlaybackStatusUpdate(is_playing ? PlaybackStatus::kPlaying
                                            : PlaybackStatus::kPaused);
  } else {
    system_media_controls_->SetPlaybackStatus(PlaybackStatus::kStopped);

    // These steps reference the Media Session Standard
    // https://wicg.github.io/mediasession/#metadata
    // 5.3.1 If the active media session is null, unset the media metadata
    // presented to the platform, and terminate these steps.
    ClearAllMetadata();
  }

#if BUILDFLAG(IS_WIN)
  if (screen_locked_) {
    if (is_playing) {
      StopHideSmtcTimer();
    } else if (!hide_smtc_timer_.IsRunning()) {
      StartHideSmtcTimer();
    }
  }
#endif  // BUILDFLAG(IS_WIN)
}

void SystemMediaControlsNotifier::MediaSessionMetadataChanged(
    const std::optional<media_session::MediaMetadata>& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (metadata.has_value()) {
    // 5.3.3 Update the media metadata presented to the platform to match the
    // metadata for the active media session.
    DebounceMetadataUpdate(*metadata);
  } else {
    // 5.3.2 If the metadata of the active media session is an empty metadata,
    // unset the media metadata presented to the platform.
    ClearAllMetadata();
  }
}

void SystemMediaControlsNotifier::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  // SeekTo is not often supported so we will emulate "seekto" using
  // "seekforward" and "seekbackward" if they exist.
  bool seek_available = false;
  for (const media_session::mojom::MediaSessionAction& action : actions) {
    if (action == media_session::mojom::MediaSessionAction::kSeekTo ||
        action == media_session::mojom::MediaSessionAction::kSeekBackward ||
        action == media_session::mojom::MediaSessionAction::kSeekForward) {
      seek_available = true;
      break;
    }
  }
  DebounceSetIsSeekToEnabled(seek_available);
}

void SystemMediaControlsNotifier::MediaSessionChanged(
    const std::optional<base::UnguessableToken>& request_id) {
  if (!request_id.has_value()) {
    system_media_controls_->SetID(nullptr);
    return;
  }
  auto string_id = request_id->ToString();
  system_media_controls_->SetID(&string_id);
}

void SystemMediaControlsNotifier::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DebounceIconUpdate(bitmap);
}

void SystemMediaControlsNotifier::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (position) {
    DebouncePositionUpdate(*position);
  } else {
    ClearAllMetadata();
  }
}

void SystemMediaControlsNotifier::DebouncePositionUpdate(
    media_session::MediaPosition position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delayed_position_update_ = position;

  MaybeScheduleMetadataUpdate();
}

void SystemMediaControlsNotifier::DebounceMetadataUpdate(
    media_session::MediaMetadata metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delayed_metadata_update_ = metadata;

  MaybeScheduleMetadataUpdate();
}

void SystemMediaControlsNotifier::DebouncePlaybackStatusUpdate(
    system_media_controls::SystemMediaControls::PlaybackStatus
        playback_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  delayed_playback_status_ = playback_status;

  MaybeScheduleMetadataUpdate();
}

void SystemMediaControlsNotifier::DebounceIconUpdate(const SkBitmap& bitmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  delayed_icon_update_ = bitmap;

  // Only update `delayed_icon_update_` once every kDebounceDelay.
  if (!icon_update_timer_.IsRunning()) {
    icon_update_timer_.Start(
        FROM_HERE, kDebounceDelay,
        base::BindOnce(&SystemMediaControlsNotifier::UpdateIcon,
                       base::Unretained(this)));
  }
}

void SystemMediaControlsNotifier::DebounceSetIsSeekToEnabled(
    bool is_seek_to_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  delayed_is_seek_to_enabled_ = is_seek_to_enabled;

  // Only update `delayed_is_seek_to_enabled_` once every kDebounceDelay.
  if (!actions_update_timer_.IsRunning()) {
    auto update_seek_to_is_enabled = [](SystemMediaControlsNotifier* self) {
      CHECK(self->delayed_is_seek_to_enabled_);

      self->system_media_controls_->SetIsSeekToEnabled(
          *self->delayed_is_seek_to_enabled_);

      self->delayed_is_seek_to_enabled_ = std::nullopt;
    };

    actions_update_timer_.Start(
        FROM_HERE, kDebounceDelay,
        base::BindOnce(std::move(update_seek_to_is_enabled),
                       base::Unretained(this)));
  }
}

void SystemMediaControlsNotifier::MaybeScheduleMetadataUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (metadata_update_timer_.IsRunning()) {
    return;
  }

  metadata_update_timer_.Start(
      FROM_HERE, kDebounceDelay,
      base::BindOnce(&SystemMediaControlsNotifier::UpdateMetadata,
                     base::Unretained(this)));
}

void SystemMediaControlsNotifier::UpdateMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (delayed_position_update_) {
    system_media_controls_->SetPosition(*delayed_position_update_);
    delayed_position_update_ = std::nullopt;
  }

  if (delayed_metadata_update_) {
    // If we need to hide the playing media's metadata we replace it with
    // placeholder metadata.
    if (session_info_ptr_ && session_info_ptr_->hide_metadata) {
      MediaSessionClient* media_session_client = MediaSessionClient::Get();
      CHECK(media_session_client);

      system_media_controls_->SetTitle(
          media_session_client->GetTitlePlaceholder());
      system_media_controls_->SetArtist(
          media_session_client->GetArtistPlaceholder());
      system_media_controls_->SetAlbum(
          media_session_client->GetAlbumPlaceholder());

      // Always make sure the metadata replacement is accompanied by the
      // thumbnail replacement.
      system_media_controls_->SetThumbnail(
          media_session_client->GetThumbnailPlaceholder());
    } else {
      // If no title was provided, the title of the tab will be in the title
      // property.
      system_media_controls_->SetTitle(delayed_metadata_update_->title);

      // If no artist was provided, then the source URL will be in the artist
      // property.
      system_media_controls_->SetArtist(delayed_metadata_update_->artist);

      system_media_controls_->SetAlbum(delayed_metadata_update_->album);
    }

    system_media_controls_->UpdateDisplay();
    delayed_metadata_update_ = std::nullopt;
  }

  if (delayed_playback_status_) {
    system_media_controls_->SetPlaybackStatus(*delayed_playback_status_);
    delayed_playback_status_ = std::nullopt;
  }
}

void SystemMediaControlsNotifier::UpdateIcon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(delayed_icon_update_);

  // If we need to hide the media metadata we replace the playing media's image
  // with a placeholder.
  if (session_info_ptr_ && session_info_ptr_->hide_metadata) {
    MediaSessionClient* media_session_client = MediaSessionClient::Get();
    CHECK(media_session_client);

    system_media_controls_->SetThumbnail(
        media_session_client->GetThumbnailPlaceholder());
  } else if (!delayed_icon_update_->empty()) {
    // 5.3.4.4.3 If the image format is supported, use the image as the
    // artwork for display in the platform UI. Otherwise the fetch image
    // algorithm fails and terminates.
    system_media_controls_->SetThumbnail(*delayed_icon_update_);
  } else {
    // 5.3.4.2 If metadata's artwork is empty, terminate these steps.
    // If no images are fetched in the fetch image algorithm, the user agent
    // may have fallback behavior such as displaying a default image as
    // artwork. We display the application icon if no artwork is provided.
    std::optional<gfx::ImageSkia> icon =
        GetContentClient()->browser()->GetProductLogo();
    if (icon.has_value()) {
      system_media_controls_->SetThumbnail(*icon->bitmap());
    } else {
      system_media_controls_->ClearThumbnail();
    }
  }

  delayed_icon_update_ = std::nullopt;
}

void SystemMediaControlsNotifier::ClearAllMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metadata_update_timer_.Stop();

  delayed_position_update_ = std::nullopt;
  delayed_metadata_update_ = std::nullopt;
  delayed_playback_status_ = std::nullopt;

  system_media_controls_->ClearMetadata();
}

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
