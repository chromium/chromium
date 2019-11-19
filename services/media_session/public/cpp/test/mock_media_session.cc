// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/test/mock_media_session.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"

namespace media_session {
namespace test {

namespace {

bool IsPositionEqual(const MediaPosition& p1, const MediaPosition& p2) {
  if (p1.duration() != p2.duration() ||
      p1.playback_rate() != p2.playback_rate()) {
    return false;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  if (p1.GetPositionAtTime(now) == p2.GetPositionAtTime(now))
    return true;

  // To make testing easier we allow position at creation time to be equal
  // to one another. If we did not do this then the position may advance
  // if the playback rate is not zero.
  return p1.GetPositionAtTime(p1.last_updated_time()) ==
         p2.GetPositionAtTime(p2.last_updated_time());
}

}  // namespace

MockMediaSessionMojoObserver::MockMediaSessionMojoObserver(
    mojom::MediaSession& media_session) {
  media_session.AddObserver(receiver_.BindNewPipeAndPassRemote());
}

MockMediaSessionMojoObserver::~MockMediaSessionMojoObserver() = default;

void MockMediaSessionMojoObserver::MediaSessionInfoChanged(
    mojom::MediaSessionInfoPtr session) {
  session_info_ = std::move(session);

  if (expected_controllable_.has_value() &&
      expected_controllable_ == session_info_->is_controllable) {
    run_loop_->Quit();
    expected_controllable_.reset();
  } else if (wanted_state_ == session_info_->state ||
             session_info_->playback_state == wanted_playback_state_) {
    run_loop_->Quit();
  }
}

void MockMediaSessionMojoObserver::MediaSessionMetadataChanged(
    const base::Optional<MediaMetadata>& metadata) {
  session_metadata_ = metadata;

  if (expected_metadata_.has_value() && expected_metadata_ == metadata) {
    run_loop_->Quit();
    expected_metadata_.reset();
  } else if (waiting_for_empty_metadata_ &&
             (!metadata.has_value() || metadata->IsEmpty())) {
    run_loop_->Quit();
    waiting_for_empty_metadata_ = false;
  }
}

void MockMediaSessionMojoObserver::MediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  session_actions_ =
      std::set<mojom::MediaSessionAction>(actions.begin(), actions.end());

  if (expected_actions_.has_value() && expected_actions_ == session_actions_) {
    run_loop_->Quit();
    expected_actions_.reset();
  }
}

void MockMediaSessionMojoObserver::MediaSessionImagesChanged(
    const base::flat_map<mojom::MediaSessionImageType, std::vector<MediaImage>>&
        images) {
  session_images_ = images;

  if (expected_images_of_type_.has_value()) {
    auto type = expected_images_of_type_->first;
    auto images = expected_images_of_type_->second;
    auto it = session_images_->find(type);

    if (it != session_images_->end() && it->second == images) {
      run_loop_->Quit();
      expected_images_of_type_.reset();
    }
  }
}

void MockMediaSessionMojoObserver::MediaSessionPositionChanged(
    const base::Optional<media_session::MediaPosition>& position) {
  session_position_ = position;

  if (position.has_value() && expected_position_.has_value() &&
      IsPositionEqual(*position, *expected_position_)) {
    run_loop_->Quit();
    expected_position_.reset();
  } else if (waiting_for_empty_position_ && !position.has_value()) {
    run_loop_->Quit();
    waiting_for_empty_position_ = false;
  }
}

void MockMediaSessionMojoObserver::WaitForState(
    mojom::MediaSessionInfo::SessionState wanted_state) {
  if (session_info_ && session_info_->state == wanted_state)
    return;

  wanted_state_ = wanted_state;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForPlaybackState(
    mojom::MediaPlaybackState wanted_state) {
  if (session_info_ && session_info_->playback_state == wanted_state)
    return;

  wanted_playback_state_ = wanted_state;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForControllable(bool is_controllable) {
  if (session_info_ && session_info_->is_controllable == is_controllable)
    return;

  expected_controllable_ = is_controllable;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForEmptyMetadata() {
  if (session_metadata_.has_value() || !session_metadata_->has_value())
    return;

  waiting_for_empty_metadata_ = true;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForExpectedMetadata(
    const MediaMetadata& metadata) {
  if (session_metadata_.has_value() && session_metadata_ == metadata)
    return;

  expected_metadata_ = metadata;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForEmptyActions() {
  WaitForExpectedActions(std::set<mojom::MediaSessionAction>());
}

void MockMediaSessionMojoObserver::WaitForExpectedActions(
    const std::set<mojom::MediaSessionAction>& actions) {
  if (session_actions_.has_value() && session_actions_ == actions)
    return;

  expected_actions_ = actions;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForExpectedImagesOfType(
    mojom::MediaSessionImageType type,
    const std::vector<MediaImage>& images) {
  if (session_images_.has_value()) {
    auto it = session_images_->find(type);
    if (it != session_images_->end() && it->second == images)
      return;
  }

  expected_images_of_type_ = std::make_pair(type, images);
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForEmptyPosition() {
  // |session_position_| is doubly wrapped in base::Optional so we must check
  // both values.
  if (session_position_.has_value() && !session_position_->has_value())
    return;

  waiting_for_empty_position_ = true;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForExpectedPosition(
    const MediaPosition& position) {
  if (session_position_.has_value() && session_position_->has_value()) {
    if (IsPositionEqual(*session_position_.value(), position))
      return;
  }

  expected_position_ = position;
  StartWaiting();
}

void MockMediaSessionMojoObserver::StartWaiting() {
  DCHECK(!run_loop_);

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

MockMediaSession::MockMediaSession() = default;

MockMediaSession::MockMediaSession(bool force_duck) : force_duck_(force_duck) {}

MockMediaSession::~MockMediaSession() {}

void MockMediaSession::Suspend(SuspendType suspend_type) {
  SetState(mojom::MediaSessionInfo::SessionState::kSuspended);
}

void MockMediaSession::Resume(SuspendType suspend_type) {
  SetState(mojom::MediaSessionInfo::SessionState::kActive);
}

void MockMediaSession::StartDucking() {
  is_ducking_ = true;
  NotifyObservers();
}

void MockMediaSession::StopDucking() {
  is_ducking_ = false;
  NotifyObservers();
}

void MockMediaSession::GetMediaSessionInfo(
    GetMediaSessionInfoCallback callback) {
  std::move(callback).Run(GetMediaSessionInfoSync());
}

void MockMediaSession::AddObserver(
    mojo::PendingRemote<mojom::MediaSessionObserver> observer) {
  ++add_observer_count_;
  mojo::Remote<mojom::MediaSessionObserver> media_session_observer(
      std::move(observer));

  media_session_observer->MediaSessionInfoChanged(GetMediaSessionInfoSync());

  std::vector<mojom::MediaSessionAction> actions(actions_.begin(),
                                                 actions_.end());
  media_session_observer->MediaSessionActionsChanged(actions);
  media_session_observer->MediaSessionImagesChanged(images_);

  observers_.Add(std::move(media_session_observer));
}

void MockMediaSession::GetDebugInfo(GetDebugInfoCallback callback) {
  mojom::MediaSessionDebugInfoPtr debug_info(
      mojom::MediaSessionDebugInfo::New());

  debug_info->name = "name";
  debug_info->owner = "owner";
  debug_info->state = "state";

  std::move(callback).Run(std::move(debug_info));
}

void MockMediaSession::PreviousTrack() {
  prev_track_count_++;
}

void MockMediaSession::NextTrack() {
  next_track_count_++;
}

void MockMediaSession::Seek(base::TimeDelta seek_time) {
  seek_count_++;
}

void MockMediaSession::Stop(SuspendType type) {
  SetState(mojom::MediaSessionInfo::SessionState::kInactive);
}

void MockMediaSession::GetMediaImageBitmap(
    const MediaImage& image,
    int minimum_size_px,
    int desired_size_px,
    GetMediaImageBitmapCallback callback) {
  last_image_src_ = image.src;

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::Make(10, 10, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));

  std::move(callback).Run(bitmap);
}

void MockMediaSession::SeekTo(base::TimeDelta seek_time) {
  seek_to_count_++;
  is_scrubbing_ = false;
}

void MockMediaSession::ScrubTo(base::TimeDelta seek_time) {
  is_scrubbing_ = true;
}

void MockMediaSession::SetIsControllable(bool value) {
  is_controllable_ = value;
  NotifyObservers();
}

void MockMediaSession::AbandonAudioFocusFromClient() {
  DCHECK(afr_client_.is_bound());
  afr_client_->AbandonAudioFocus();
  afr_client_.FlushForTesting();
  afr_client_.reset();
  request_id_ = base::UnguessableToken::Null();
}

base::UnguessableToken MockMediaSession::RequestAudioFocusFromService(
    mojo::Remote<mojom::AudioFocusManager>& service,
    mojom::AudioFocusType audio_focus_type) {
  if (afr_client_.is_bound()) {
    RequestAudioFocusFromClient(audio_focus_type);
  } else {
    DCHECK(request_id_.is_empty());

    // Build a new audio focus request.
    mojo::PendingRemote<mojom::MediaSession> media_session;
    receivers_.Add(this, media_session.InitWithNewPipeAndPassReceiver());

    service->RequestAudioFocus(
        afr_client_.BindNewPipeAndPassReceiver(), std::move(media_session),
        GetMediaSessionInfoSync(), audio_focus_type,
        base::BindOnce(
            [](base::UnguessableToken* id,
               const base::UnguessableToken& received_id) {
              *id = received_id;
            },
            &request_id_));

    service.FlushForTesting();
    afr_client_.FlushForTesting();
  }

  DCHECK(!request_id_.is_empty());
  SetState(mojom::MediaSessionInfo::SessionState::kActive);

  return request_id_;
}

bool MockMediaSession::RequestGroupedAudioFocusFromService(
    const base::UnguessableToken& request_id,
    mojo::Remote<mojom::AudioFocusManager>& service,
    mojom::AudioFocusType audio_focus_type,
    const base::UnguessableToken& group_id) {
  if (afr_client_.is_bound()) {
    RequestAudioFocusFromClient(audio_focus_type);
    SetState(mojom::MediaSessionInfo::SessionState::kActive);
    return true;
  }

  DCHECK(request_id_.is_empty());

  // Build a new audio focus request.
  mojo::PendingRemote<mojom::MediaSession> media_session;
  receivers_.Add(this, media_session.InitWithNewPipeAndPassReceiver());
  bool success;

  service->RequestGroupedAudioFocus(
      request_id, afr_client_.BindNewPipeAndPassReceiver(),
      std::move(media_session), GetMediaSessionInfoSync(), audio_focus_type,
      group_id,
      base::BindOnce([](bool* success, bool result) { *success = result; },
                     &success));

  service.FlushForTesting();
  afr_client_.FlushForTesting();

  if (success) {
    request_id_ = request_id;
    SetState(mojom::MediaSessionInfo::SessionState::kActive);
  } else {
    afr_client_.reset();
  }

  return success;
}

mojom::MediaSessionInfo::SessionState MockMediaSession::GetState() const {
  return GetMediaSessionInfoSync()->state;
}

void MockMediaSession::FlushForTesting() {
  afr_client_.FlushForTesting();
}

void MockMediaSession::SimulateMetadataChanged(
    const base::Optional<MediaMetadata>& metadata) {
  for (auto& observer : observers_) {
    observer->MediaSessionMetadataChanged(metadata);
  }
}

void MockMediaSession::SimulatePositionChanged(
    const base::Optional<MediaPosition>& position) {
  for (auto& observer : observers_) {
    observer->MediaSessionPositionChanged(position);
  }
}

void MockMediaSession::ClearAllImages() {
  images_.clear();

  for (auto& observer : observers_) {
    observer->MediaSessionImagesChanged(this->images_);
  }
}

void MockMediaSession::SetImagesOfType(mojom::MediaSessionImageType type,
                                       const std::vector<MediaImage>& images) {
  images_.insert_or_assign(type, images);

  for (auto& observer : observers_) {
    observer->MediaSessionImagesChanged(this->images_);
  }
}

void MockMediaSession::EnableAction(mojom::MediaSessionAction action) {
  if (base::Contains(actions_, action))
    return;

  actions_.insert(action);
  NotifyActionObservers();
}

void MockMediaSession::DisableAction(mojom::MediaSessionAction action) {
  if (!base::Contains(actions_, action))
    return;

  actions_.erase(action);
  NotifyActionObservers();
}

void MockMediaSession::SetState(mojom::MediaSessionInfo::SessionState state) {
  state_ = state;
  NotifyObservers();
}

void MockMediaSession::NotifyObservers() {
  mojom::MediaSessionInfoPtr session_info = GetMediaSessionInfoSync();

  if (afr_client_.is_bound())
    afr_client_->MediaSessionInfoChanged(session_info.Clone());

  for (auto& observer : observers_) {
    observer->MediaSessionInfoChanged(session_info.Clone());
  }
}

mojom::MediaSessionInfoPtr MockMediaSession::GetMediaSessionInfoSync() const {
  mojom::MediaSessionInfoPtr info(mojom::MediaSessionInfo::New());
  info->force_duck = force_duck_;
  info->state = state_;
  if (is_ducking_)
    info->state = mojom::MediaSessionInfo::SessionState::kDucking;

  info->playback_state = mojom::MediaPlaybackState::kPaused;
  if (state_ == mojom::MediaSessionInfo::SessionState::kActive)
    info->playback_state = mojom::MediaPlaybackState::kPlaying;

  info->is_controllable = is_controllable_;
  info->prefer_stop_for_gain_focus_loss = prefer_stop_;

  return info;
}

void MockMediaSession::NotifyActionObservers() {
  std::vector<mojom::MediaSessionAction> actions(actions_.begin(),
                                                 actions_.end());

  for (auto& observer : observers_) {
    observer->MediaSessionActionsChanged(actions);
  }
}

void MockMediaSession::RequestAudioFocusFromClient(
    mojom::AudioFocusType audio_focus_type) {
  DCHECK(afr_client_.is_bound());
  DCHECK(!request_id_.is_empty());

  bool result = false;
  afr_client_->RequestAudioFocus(
      GetMediaSessionInfoSync(), audio_focus_type,
      base::BindOnce([](bool* out_result) { *out_result = true; }, &result));

  afr_client_.FlushForTesting();
  DCHECK(result);
}

}  // namespace test
}  // namespace media_session
