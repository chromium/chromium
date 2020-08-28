// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/media_player_impl.h"

#include <fuchsia/media/cpp/fidl.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/media_session.h"
#include "services/media_session/public/mojom/constants.mojom.h"

namespace {

fuchsia::media::sessions2::PlayerCapabilityFlags ActionToCapabilityFlag(
    media_session::mojom::MediaSessionAction action) {
  using MediaSessionAction = media_session::mojom::MediaSessionAction;
  using PlayerCapabilityFlags =
      fuchsia::media::sessions2::PlayerCapabilityFlags;
  switch (action) {
    case MediaSessionAction::kEnterPictureInPicture:
    case MediaSessionAction::kExitPictureInPicture:
      return {};  // PlayerControl does not support picture-in-picture.
    case MediaSessionAction::kPlay:
      return PlayerCapabilityFlags::PLAY;
    case MediaSessionAction::kPause:
      return PlayerCapabilityFlags::PAUSE;
    case MediaSessionAction::kPreviousTrack:
      return PlayerCapabilityFlags::CHANGE_TO_PREV_ITEM;
    case MediaSessionAction::kNextTrack:
      return PlayerCapabilityFlags::CHANGE_TO_NEXT_ITEM;
    case MediaSessionAction::kSeekBackward:
      return PlayerCapabilityFlags::SKIP_REVERSE;
    case MediaSessionAction::kSeekForward:
      return PlayerCapabilityFlags::SKIP_FORWARD;
    case MediaSessionAction::kSeekTo:
      return PlayerCapabilityFlags::SEEK;
    case MediaSessionAction::kScrubTo:
      return {};  // PlayerControl does not support scrub-to.
    case MediaSessionAction::kSkipAd:
      return {};  // PlayerControl does not support skipping ads.
    case MediaSessionAction::kStop:
      return {};  // PlayerControl assumes that stop is always supported.
  }
}

void AddMetadata(base::StringPiece label,
                 base::StringPiece16 value,
                 fuchsia::media::Metadata* metadata) {
  fuchsia::media::Property property;
  property.label = label.as_string();
  property.value = base::UTF16ToUTF8(value);
  metadata->properties.emplace_back(std::move(property));
}

fuchsia::media::sessions2::PlayerState SessionStateToPlayerState(
    media_session::mojom::MediaSessionInfo::SessionState state) {
  switch (state) {
    case media_session::mojom::MediaSessionInfo::SessionState::kActive:
    case media_session::mojom::MediaSessionInfo::SessionState::kDucking:
      return fuchsia::media::sessions2::PlayerState::PLAYING;
    case media_session::mojom::MediaSessionInfo::SessionState::kInactive:
      return fuchsia::media::sessions2::PlayerState::IDLE;
    case media_session::mojom::MediaSessionInfo::SessionState::kSuspended:
      return fuchsia::media::sessions2::PlayerState::PAUSED;
  };
}

}  // namespace

MediaPlayerImpl::MediaPlayerImpl(
    content::MediaSession* media_session,
    fidl::InterfaceRequest<fuchsia::media::sessions2::Player> request,
    base::OnceClosure on_disconnect)
    : media_session_(media_session),
      on_disconnect_(std::move(on_disconnect)),
      binding_(this, std::move(request)),
      observer_receiver_(this) {
  binding_.set_error_handler([this](zx_status_t status) mutable {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << "Player disconnected.";
    std::move(on_disconnect_).Run();
  });

  // Set default values for some fields in |pending_info_delta_|, which some
  // clients may otherwise use incorrect defaults for.
  pending_info_delta_.set_local(true);
}

MediaPlayerImpl::~MediaPlayerImpl() = default;

void MediaPlayerImpl::WatchInfoChange(
    WatchInfoChangeCallback info_change_callback) {
  if (pending_info_change_callback_) {
    DLOG(ERROR) << "Unexpected WatchInfoChange().";
    ReportErrorAndDisconnect(ZX_ERR_BAD_STATE);
    return;
  }

  pending_info_change_callback_ = std::move(info_change_callback);

  if (!observer_receiver_.is_bound()) {
    // |media_session| will notify us via our MediaSessionObserver interface
    // of the current state of the session (metadata, actions, etc) in response
    // to AddObserver().
    media_session_->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
  }

  MaybeSendPlayerInfoDelta();
}

void MediaPlayerImpl::Play() {
  media_session_->Resume(content::MediaSession::SuspendType::kUI);
}

void MediaPlayerImpl::Pause() {
  media_session_->Suspend(content::MediaSession::SuspendType::kUI);
}

void MediaPlayerImpl::Stop() {
  media_session_->Suspend(content::MediaSession::SuspendType::kUI);
}

void MediaPlayerImpl::Seek(zx_duration_t position) {
  media_session_->SeekTo(base::TimeDelta::FromZxDuration(position));
}

void MediaPlayerImpl::SkipForward() {
  media_session_->Seek(base::TimeDelta::FromSeconds(
      media_session::mojom::kDefaultSeekTimeSeconds));
}

void MediaPlayerImpl::SkipReverse() {
  media_session_->Seek(-base::TimeDelta::FromSeconds(
      media_session::mojom::kDefaultSeekTimeSeconds));
}

void MediaPlayerImpl::NextItem() {
  media_session_->NextTrack();
}

void MediaPlayerImpl::PrevItem() {
  media_session_->PreviousTrack();
}

void MediaPlayerImpl::SetPlaybackRate(float playback_rate) {
  // content::MediaSession does not support changes to playback rate.
}

void MediaPlayerImpl::SetRepeatMode(
    fuchsia::media::sessions2::RepeatMode repeat_mode) {
  // content::MediaSession does not provide control over repeat playback.
}

void MediaPlayerImpl::SetShuffleMode(bool shuffle_on) {
  // content::MediaSession does not provide control over item playback order.
}

void MediaPlayerImpl::BindVolumeControl(
    fidl::InterfaceRequest<fuchsia::media::audio::VolumeControl>
        volume_control_request) {
  // content::MediaSession does not provide control over audio gain.
  volume_control_request.Close(ZX_ERR_NOT_SUPPORTED);
}

void MediaPlayerImpl::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr info) {
  fuchsia::media::sessions2::PlayerStatus status;
  status.set_player_state(SessionStateToPlayerState(info->state));
  pending_info_delta_.set_player_status(std::move(status));
  MaybeSendPlayerInfoDelta();
}

void MediaPlayerImpl::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata_mojo) {
  fuchsia::media::Metadata metadata;
  if (metadata_mojo) {
    AddMetadata(fuchsia::media::METADATA_LABEL_TITLE, metadata_mojo->title,
                &metadata);
    AddMetadata(fuchsia::media::METADATA_LABEL_ARTIST, metadata_mojo->artist,
                &metadata);
    AddMetadata(fuchsia::media::METADATA_LABEL_ALBUM, metadata_mojo->album,
                &metadata);
    AddMetadata(fuchsia::media::METADATA_SOURCE_TITLE,
                metadata_mojo->source_title, &metadata);
  }
  pending_info_delta_.set_metadata(std::move(metadata));
  MaybeSendPlayerInfoDelta();
}

void MediaPlayerImpl::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  // TODO(https://crbug.com/879317): Implement PROVIDE_BITMAPS.
  fuchsia::media::sessions2::PlayerCapabilityFlags capability_flags{};
  for (auto action : actions)
    capability_flags |= ActionToCapabilityFlag(action);
  pending_info_delta_.mutable_player_capabilities()->set_flags(
      std::move(capability_flags));
  MaybeSendPlayerInfoDelta();
}

void MediaPlayerImpl::MediaSessionImagesChanged(
    const base::flat_map<media_session::mojom::MediaSessionImageType,
                         std::vector<media_session::MediaImage>>& images) {
  // TODO(https://crbug.com/879317): Implement image-changed.
  NOTIMPLEMENTED_LOG_ONCE();
}

void MediaPlayerImpl::MediaSessionPositionChanged(
    const base::Optional<media_session::MediaPosition>& position) {
  // TODO(https://crbug.com/879317): Implement media position changes.
  NOTIMPLEMENTED_LOG_ONCE();
}

void MediaPlayerImpl::MaybeSendPlayerInfoDelta() {
  if (!pending_info_change_callback_)
    return;
  if (pending_info_delta_.IsEmpty())
    return;
  // std::exchange(foo, {}) returns the contents of |foo|, while ensuring that
  // |foo| is reset to the initial/empty state.
  std::exchange(pending_info_change_callback_,
                {})(std::exchange(pending_info_delta_, {}));
}

void MediaPlayerImpl::ReportErrorAndDisconnect(zx_status_t status) {
  binding_.Close(status);
  std::move(on_disconnect_).Run();
}
