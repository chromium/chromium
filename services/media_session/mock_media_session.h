// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_MOCK_MEDIA_SESSION_H_
#define SERVICES_MEDIA_SESSION_MOCK_MEDIA_SESSION_H_

#include "base/optional.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace media_session {
namespace test {

// A mock MediaSessionObsever that can be used for waiting for state changes.
class MockMediaSessionMojoObserver : public mojom::MediaSessionObserver {
 public:
  // A MediaSessionObserver can observe a MediaSession directly or through a
  // MediaController.
  explicit MockMediaSessionMojoObserver(mojom::MediaSession& media_session);
  explicit MockMediaSessionMojoObserver(mojom::MediaControllerPtr& controller);

  ~MockMediaSessionMojoObserver() override;

  // mojom::MediaSessionObserver overrides.
  void MediaSessionInfoChanged(mojom::MediaSessionInfoPtr session) override;

  void WaitForState(mojom::MediaSessionInfo::SessionState wanted_state);
  void WaitForPlaybackState(mojom::MediaPlaybackState wanted_state);

 private:
  mojom::MediaSessionInfoPtr session_info_;
  base::Optional<mojom::MediaSessionInfo::SessionState> wanted_state_;
  base::Optional<mojom::MediaPlaybackState> wanted_playback_state_;
  base::RunLoop run_loop_;

  mojo::Binding<mojom::MediaSessionObserver> binding_;
};

// A mock MediaSession that can be used for interacting with the Media Session
// service during tests.
class MockMediaSession : public mojom::MediaSession {
 public:
  MockMediaSession();
  explicit MockMediaSession(bool force_duck);

  ~MockMediaSession() override;

  // mojom::MediaSession overrides.
  void Suspend(SuspendType) override;
  void Resume(SuspendType) override;
  void StartDucking() override;
  void StopDucking() override;
  void GetMediaSessionInfo(GetMediaSessionInfoCallback) override;
  void AddObserver(mojom::MediaSessionObserverPtr) override;
  void GetDebugInfo(GetDebugInfoCallback) override;
  void PreviousTrack() override;
  void NextTrack() override;

  void Stop();

  void AbandonAudioFocusFromClient();
  base::UnguessableToken GetRequestIdFromClient();

  base::UnguessableToken RequestAudioFocusFromService(
      mojom::AudioFocusManagerPtr&,
      mojom::AudioFocusType);

  mojom::MediaSessionInfo::SessionState GetState() const;

  mojom::AudioFocusRequestClient* audio_focus_request() const {
    return afr_client_.get();
  }
  void FlushForTesting();

  int prev_track_count() const { return prev_track_count_; }
  int next_track_count() const { return next_track_count_; }

 private:
  void SetState(mojom::MediaSessionInfo::SessionState);
  void NotifyObservers();
  mojom::MediaSessionInfoPtr GetMediaSessionInfoSync() const;

  mojom::AudioFocusRequestClientPtr afr_client_;

  const bool force_duck_ = false;
  bool is_ducking_ = false;

  int prev_track_count_ = 0;
  int next_track_count_ = 0;

  mojom::MediaSessionInfo::SessionState state_ =
      mojom::MediaSessionInfo::SessionState::kInactive;

  mojo::BindingSet<mojom::MediaSession> bindings_;

  mojo::InterfacePtrSet<mojom::MediaSessionObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaSession);
};

}  // namespace test
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_MOCK_MEDIA_SESSION_H_
