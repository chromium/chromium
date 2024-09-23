// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/media_player_impl.h"

#include <lib/async/default.h>

#include <string_view>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/media_session.h"
#include "services/media_session/public/mojom/constants.mojom.h"

namespace {

fuchsia_media_sessions2::PlayerCapabilityFlags ActionToCapabilityFlag(
    media_session::mojom::MediaSessionAction action) {
  using MediaSessionAction = media_session::mojom::MediaSessionAction;
  using PlayerCapabilityFlags = fuchsia_media_sessions2::PlayerCapabilityFlags;
  switch (action) {
    case MediaSessionAction::kEnterPictureInPicture:
    case MediaSessionAction::kExitPictureInPicture:
      return {};  // PlayerControl does not support picture-in-picture.
    case MediaSessionAction::kPlay:
      return PlayerCapabilityFlags::kPlay;
    case MediaSessionAction::kPause:
      return PlayerCapabilityFlags::kPause;
    case MediaSessionAction::kPreviousTrack:
      return PlayerCapabilityFlags::kChangeToPrevItem;
    case MediaSessionAction::kNextTrack:
      return PlayerCapabilityFlags::kChangeToNextItem;
    case MediaSessionAction::kSeekBackward:
      return PlayerCapabilityFlags::kSkipReverse;
    case MediaSessionAction::kSeekForward:
      return PlayerCapabilityFlags::kSkipForward;
    case MediaSessionAction::kSeekTo:
      return PlayerCapabilityFlags::kSeek;
    case MediaSessionAction::kScrubTo:
      return {};  // PlayerControl does not support scrub-to.
    case MediaSessionAction::kSkipAd:
      return {};  // PlayerControl does not support skipping ads.
    case MediaSessionAction::kStop:
      return {};  // PlayerControl assumes that stop is always supported.
    case MediaSessionAction::kSwitchAudioDevice:
      return {};  // PlayerControl does not support switching audio device.
    case MediaSessionAction::kToggleMicrophone:
      return {};  // PlayerControl does not support toggling microphone.
    case MediaSessionAction::kToggleCamera:
      return {};  // PlayerControl does not support toggling camera.
    case MediaSessionAction::kHangUp:
      return {};  // PlayerControl does not support hanging up.
    case MediaSessionAction::kRaise:
      return {};  // PlayerControl does not support raising.
    case MediaSessionAction::kSetMute:
      return {};  // TODO(crbug.com/40194407): implement set mute.
    case MediaSessionAction::kPreviousSlide:
      return {};  // PlayerControl does not support going back to previous
                  // slide.
    case MediaSessionAction::kNextSlide:
      return {};  // PlayerControl does not support going to next slide.
    case MediaSessionAction::kEnterAutoPictureInPicture:
      return {};  // PlayerControl does not support picture-in-picture.
  }
}

void AddMetadata(std::string_view label,
                 std::u16string_view value,
                 fuchsia_media::Metadata* metadata) {
  fuchsia_media::Property property{
      {.label{label}, .value{base::UTF16ToUTF8(value)}}};
  metadata->properties().emplace_back(std::move(property));
}

fuchsia_media_sessions2::PlayerState SessionStateToPlayerState(
    media_session::mojom::MediaSessionInfo::SessionState state) {
  switch (state) {
    case media_session::mojom::MediaSessionInfo::SessionState::kActive:
    case media_session::mojom::MediaSessionInfo::SessionState::kDucking:
      return fuchsia_media_sessions2::PlayerState::kPlaying;
    case media_session::mojom::MediaSessionInfo::SessionState::kInactive:
      return fuchsia_media_sessions2::PlayerState::kIdle;
    case media_session::mojom::MediaSessionInfo::SessionState::kSuspended:
      return fuchsia_media_sessions2::PlayerState::kPaused;
  };
}

}  // namespace

MediaPlayerImpl::MediaPlayerImpl(
    content::MediaSession* media_session,
    fidl::ServerEnd<fuchsia_media_sessions2::Player> server_end,
    base::OnceClosure on_disconnect)
    : media_session_(media_session),
      on_disconnect_(std::move(on_disconnect)),
      binding_(async_get_default_dispatcher(),
               std::move(server_end),
               this,
               fit::bind_member(this, &MediaPlayerImpl::OnBindingClosure)),
      observer_receiver_(this) {
  // Set default values for some fields in |pending_info_delta_|, which some
  // clients may otherwise use incorrect defaults for.
  pending_info_delta_.local(true);
}

MediaPlayerImpl::~MediaPlayerImpl() {
  if (pending_info_change_callback_) {
    pending_info_change_callback_->Close(ZX_ERR_PEER_CLOSED);
  }
}

void MediaPlayerImpl::WatchInfoChange(
    MediaPlayerImpl::WatchInfoChangeCompleter::Sync& completer) {
  if (pending_info_change_callback_) {
    ReportErrorAndDisconnect(ZX_ERR_BAD_STATE);
    return;
  }

  pending_info_change_callback_ = completer.ToAsync();

  if (!observer_receiver_.is_bound()) {
    // |media_session| will notify us via our MediaSessionObserver interface
    // of the current state of the session (metadata, actions, etc) in response
    // to AddObserver().
    media_session_->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
  }

  MaybeSendPlayerInfoDelta();
}

void MediaPlayerImpl::Play(
    MediaPlayerImpl::PlayCompleter::Sync& ignored_completer) {
  media_session_->Resume(content::MediaSession::SuspendType::kUI);
}

void MediaPlayerImpl::Pause(
    MediaPlayerImpl::PauseCompleter::Sync& ignored_completer) {
  media_session_->Suspend(content::MediaSession::SuspendType::kUI);
}

void MediaPlayerImpl::Stop(
    MediaPlayerImpl::StopCompleter::Sync& ignored_completer) {
  media_session_->Suspend(content::MediaSession::SuspendType::kUI);
}

void MediaPlayerImpl::Seek(
    MediaPlayerImpl::SeekRequest& request,
    MediaPlayerImpl::SeekCompleter::Sync& ignored_completer) {
  media_session_->SeekTo(base::TimeDelta::FromZxDuration(request.position()));
}

void MediaPlayerImpl::SkipForward(
    MediaPlayerImpl::SkipForwardCompleter::Sync& ignored_completer) {
  media_session_->Seek(
      base::Seconds(media_session::mojom::kDefaultSeekTimeSeconds));
}

void MediaPlayerImpl::SkipReverse(
    MediaPlayerImpl::SkipReverseCompleter::Sync& ignored_completer) {
  media_session_->Seek(
      -base::Seconds(media_session::mojom::kDefaultSeekTimeSeconds));
}

void MediaPlayerImpl::NextItem(
    MediaPlayerImpl::NextItemCompleter::Sync& ignored_completer) {
  media_session_->NextTrack();
}

void MediaPlayerImpl::PrevItem(
    MediaPlayerImpl::PrevItemCompleter::Sync& ignored_completer) {
  media_session_->PreviousTrack();
}

void MediaPlayerImpl::SetPlaybackRate(
    MediaPlayerImpl::SetPlaybackRateRequest& request,
    MediaPlayerImpl::SetPlaybackRateCompleter::Sync& ignored_completer) {
  // content::MediaSession does not support changes to playback rate.
  NOTIMPLEMENTED_LOG_ONCE();
}

void MediaPlayerImpl::SetRepeatMode(
    MediaPlayerImpl::SetRepeatModeRequest& request,
    MediaPlayerImpl::SetRepeatModeCompleter::Sync& ignored_completer) {
  // content::MediaSession does not provide control over repeat playback.
  NOTIMPLEMENTED_LOG_ONCE();
}

void MediaPlayerImpl::SetShuffleMode(
    MediaPlayerImpl::SetShuffleModeRequest& request,
    MediaPlayerImpl::SetShuffleModeCompleter::Sync& ignored_completer) {
  // content::MediaSession does not provide control over item playback order.
  NOTIMPLEMENTED_LOG_ONCE();
}

void MediaPlayerImpl::BindVolumeControl(
    MediaPlayerImpl::BindVolumeControlRequest& request,
    MediaPlayerImpl::BindVolumeControlCompleter::Sync& ignored_completer) {
  // content::MediaSession does not provide control over audio gain.
  request.volume_control_request().Close(ZX_ERR_NOT_SUPPORTED);
}

void MediaPlayerImpl::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr info) {
  fuchsia_media_sessions2::PlayerStatus status{{
      .player_state = SessionStateToPlayerState(info->state),
  }};
  pending_info_delta_.player_status(std::move(status));
  MaybeSendPlayerInfoDelta();
}

void MediaPlayerImpl::MediaSessionMetadataChanged(
    const std::optional<media_session::MediaMetadata>& metadata_mojo) {
  fuchsia_media::Metadata metadata;
  if (metadata_mojo) {
    AddMetadata(fuchsia_media::kMetadataLabelTitle, metadata_mojo->title,
                &metadata);
    AddMetadata(fuchsia_media::kMetadataLabelArtist, metadata_mojo->artist,
                &metadata);
    AddMetadata(fuchsia_media::kMetadataLabelAlbum, metadata_mojo->album,
                &metadata);
    AddMetadata(fuchsia_media::kMetadataSourceTitle,
                metadata_mojo->source_title, &metadata);
  }
  pending_info_delta_.metadata(std::move(metadata));
  MaybeSendPlayerInfoDelta();
}

void MediaPlayerImpl::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  // TODO(crbug.com/40591625): Implement PROVIDE_BITMAPS.
  fuchsia_media_sessions2::PlayerCapabilityFlags capability_flags{};
  for (auto action : actions)
    capability_flags |= ActionToCapabilityFlag(action);
  pending_info_delta_.player_capabilities(
      fuchsia_media_sessions2::PlayerCapabilities{
          {.flags = std::move(capability_flags)}});
  MaybeSendPlayerInfoDelta();
}

void MediaPlayerImpl::MediaSessionImagesChanged(
    const base::flat_map<media_session::mojom::MediaSessionImageType,
                         std::vector<media_session::MediaImage>>& images) {
  // TODO(crbug.com/40591625): Implement image-changed.
  NOTIMPLEMENTED_LOG_ONCE();
}

void MediaPlayerImpl::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {
  // TODO(crbug.com/40591625): Implement media position changes.
  NOTIMPLEMENTED_LOG_ONCE();
}

void MediaPlayerImpl::MaybeSendPlayerInfoDelta() {
  if (!pending_info_change_callback_)
    return;
  if (pending_info_delta_.IsEmpty())
    return;
  // std::exchange(foo, {}) returns the contents of |foo|, while ensuring that
  // |foo| is reset to the initial/empty state.
  std::exchange(pending_info_change_callback_, std::nullopt)
      ->Reply(std::exchange(pending_info_delta_, {}));
}

void MediaPlayerImpl::OnBindingClosure(fidl::UnbindInfo info) {
  ZX_LOG_IF(ERROR, info.status() != ZX_ERR_PEER_CLOSED, info.status())
      << "Player disconnected.";
  if (on_disconnect_) {
    std::move(on_disconnect_).Run();
  }
}

void MediaPlayerImpl::ReportErrorAndDisconnect(zx_status_t status) {
  binding_.Close(status);
}
