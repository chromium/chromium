// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/mock_media_session.h"

#include <utility>

#include "services/media_session/public/cpp/switches.h"

namespace media_session {
namespace test {

MockMediaSessionMojoObserver::MockMediaSessionMojoObserver(
    mojom::MediaSession& media_session)
    : binding_(this) {
  mojom::MediaSessionObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  media_session.AddObserver(std::move(observer));
}

MockMediaSessionMojoObserver::MockMediaSessionMojoObserver(
    mojom::MediaControllerPtr& controller)
    : binding_(this) {
  mojom::MediaSessionObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  controller->AddObserver(std::move(observer));
}

MockMediaSessionMojoObserver::~MockMediaSessionMojoObserver() = default;

void MockMediaSessionMojoObserver::MediaSessionInfoChanged(
    mojom::MediaSessionInfoPtr session) {
  session_info_ = std::move(session);

  if (wanted_state_ == session_info_->state ||
      session_info_->playback_state == wanted_playback_state_) {
    run_loop_.Quit();
  }
}

void MockMediaSessionMojoObserver::WaitForState(
    mojom::MediaSessionInfo::SessionState wanted_state) {
  if (session_info_ && session_info_->state == wanted_state)
    return;

  wanted_state_ = wanted_state;
  run_loop_.Run();
}

void MockMediaSessionMojoObserver::WaitForPlaybackState(
    mojom::MediaPlaybackState wanted_state) {
  if (session_info_ && session_info_->playback_state == wanted_state)
    return;

  wanted_playback_state_ = wanted_state;
  run_loop_.Run();
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

void MockMediaSession::AddObserver(mojom::MediaSessionObserverPtr observer) {
  observer->MediaSessionInfoChanged(GetMediaSessionInfoSync());
  observers_.AddPtr(std::move(observer));
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

void MockMediaSession::Stop() {
  SetState(mojom::MediaSessionInfo::SessionState::kInactive);
}

void MockMediaSession::AbandonAudioFocusFromClient() {
  DCHECK(afr_client_.is_bound());
  afr_client_->AbandonAudioFocus();
  afr_client_.FlushForTesting();
  afr_client_.reset();
}

base::UnguessableToken MockMediaSession::GetRequestIdFromClient() {
  DCHECK(afr_client_.is_bound());
  base::UnguessableToken id = base::UnguessableToken::Null();

  afr_client_->GetRequestId(base::BindOnce(
      [](base::UnguessableToken* id,
         const base::UnguessableToken& received_id) { *id = received_id; },
      &id));

  afr_client_.FlushForTesting();
  DCHECK_NE(base::UnguessableToken::Null(), id);
  return id;
}

base::UnguessableToken MockMediaSession::RequestAudioFocusFromService(
    mojom::AudioFocusManagerPtr& service,
    mojom::AudioFocusType audio_focus_type) {
  bool result;
  base::OnceClosure callback =
      base::BindOnce([](bool* out_result) { *out_result = true; }, &result);

  if (afr_client_.is_bound()) {
    // Request audio focus through the existing request.
    afr_client_->RequestAudioFocus(GetMediaSessionInfoSync(), audio_focus_type,
                                   std::move(callback));

    afr_client_.FlushForTesting();
  } else {
    // Build a new audio focus request.
    mojom::MediaSessionPtr media_session;
    bindings_.AddBinding(this, mojo::MakeRequest(&media_session));

    service->RequestAudioFocus(
        mojo::MakeRequest(&afr_client_), std::move(media_session),
        GetMediaSessionInfoSync(), audio_focus_type, std::move(callback));

    service.FlushForTesting();
  }

  // If the audio focus was granted then we should set the session state to
  // active.
  if (result)
    SetState(mojom::MediaSessionInfo::SessionState::kActive);

  return GetRequestIdFromClient();
}

mojom::MediaSessionInfo::SessionState MockMediaSession::GetState() const {
  return GetMediaSessionInfoSync()->state;
}

void MockMediaSession::FlushForTesting() {
  afr_client_.FlushForTesting();
}

void MockMediaSession::SetState(mojom::MediaSessionInfo::SessionState state) {
  state_ = state;
  NotifyObservers();
}

void MockMediaSession::NotifyObservers() {
  mojom::MediaSessionInfoPtr session_info = GetMediaSessionInfoSync();

  if (afr_client_.is_bound())
    afr_client_->MediaSessionInfoChanged(session_info.Clone());

  observers_.ForAllPtrs([&session_info](mojom::MediaSessionObserver* observer) {
    observer->MediaSessionInfoChanged(session_info.Clone());
  });
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

  return info;
}

}  // namespace test
}  // namespace media_session
