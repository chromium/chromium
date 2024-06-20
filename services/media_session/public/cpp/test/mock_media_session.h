// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_MEDIA_SESSION_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_MEDIA_SESSION_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace media_session {
namespace test {

// A mock MediaSessionObsever that can be used for waiting for state changes.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP)
    MockMediaSessionMojoObserver : public mojom::MediaSessionObserver {
 public:
  explicit MockMediaSessionMojoObserver(mojom::MediaSession& media_session);

  ~MockMediaSessionMojoObserver() override;

  // mojom::MediaSessionObserver overrides.
  void MediaSessionInfoChanged(mojom::MediaSessionInfoPtr session) override;
  void MediaSessionMetadataChanged(
      const std::optional<MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<mojom::MediaSessionAction>& actions) override;
  void MediaSessionImagesChanged(
      const base::flat_map<mojom::MediaSessionImageType,
                           std::vector<MediaImage>>& images) override;
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override;

  void WaitForState(mojom::MediaSessionInfo::SessionState wanted_state);
  void WaitForPlaybackState(mojom::MediaPlaybackState wanted_state);
  void WaitForMicrophoneState(mojom::MicrophoneState wanted_state);
  void WaitForCameraState(mojom::CameraState wanted_state);

  // Blocks until the set of audio/video states for the players in the media
  // session matches |wanted_states|. The order is not important.
  void WaitForAudioVideoStates(
      const std::vector<mojom::MediaAudioVideoState>& wanted_states);

  void WaitForControllable(bool is_controllable);
  void WaitForExpectedHideMetadata(bool hide_metadata);

  void WaitForEmptyMetadata();
  void WaitForExpectedMetadata(const MediaMetadata& metadata);

  void WaitForEmptyActions();
  void WaitForExpectedActions(
      const std::set<mojom::MediaSessionAction>& actions);

  void WaitForExpectedImagesOfType(mojom::MediaSessionImageType type,
                                   const std::vector<MediaImage>& images);

  void WaitForEmptyPosition();

  // Blocks until notified about MediaPosition changing to one matching
  // |position|.
  void WaitForExpectedPosition(const MediaPosition& position);

  // Blocks until notified about MediaPosition changing to one matching
  // |position|, where media time is required to be equal to or greater than
  // the media time of |position|. Returns the media time actually reported
  // with MediaPosition.
  base::TimeDelta WaitForExpectedPositionAtLeast(const MediaPosition& position);

  // Blocks until notified about MediaSessionInfo meets_visibility_threshold
  // changing to the given value. Returns the value reported.
  bool WaitForMeetsVisibilityThreshold(bool meets_visibility_threshold);

  const mojom::MediaSessionInfoPtr& session_info() const {
    return session_info_;
  }

  const std::optional<std::optional<MediaMetadata>>& session_metadata() const {
    return session_metadata_;
  }

  const std::set<mojom::MediaSessionAction>& actions() const {
    return *session_actions_;
  }

  const std::optional<std::optional<MediaPosition>>& session_position() {
    return session_position_;
  }

 private:
  void StartWaiting();
  void QuitWaitingIfNeeded();

  mojom::MediaSessionInfoPtr session_info_;
  std::optional<std::optional<MediaMetadata>> session_metadata_;
  std::optional<std::set<mojom::MediaSessionAction>> session_actions_;
  std::optional<
      base::flat_map<mojom::MediaSessionImageType, std::vector<MediaImage>>>
      session_images_;
  std::optional<std::optional<MediaPosition>> session_position_;
  bool waiting_for_empty_position_ = false;

  std::optional<MediaMetadata> expected_metadata_;
  std::optional<std::set<mojom::MediaSessionAction>> expected_actions_;
  std::optional<bool> expected_controllable_;
  std::optional<bool> expected_hide_metadata_;
  std::optional<bool> expected_meets_visibility_threshold_;
  std::optional<
      std::pair<mojom::MediaSessionImageType, std::vector<MediaImage>>>
      expected_images_of_type_;
  std::optional<MediaPosition> expected_position_;
  std::optional<MediaPosition> minimum_expected_position_;
  bool waiting_for_empty_metadata_ = false;

  std::optional<mojom::MediaSessionInfo::SessionState> wanted_state_;
  std::optional<mojom::MediaPlaybackState> wanted_playback_state_;
  std::optional<mojom::MicrophoneState> wanted_microphone_state_;
  std::optional<mojom::CameraState> wanted_camera_state_;
  std::optional<std::vector<mojom::MediaAudioVideoState>>
      wanted_audio_video_states_;
  std::unique_ptr<base::RunLoop> run_loop_;

  mojo::Receiver<mojom::MediaSessionObserver> receiver_{this};
};

// A mock MediaSession that can be used for interacting with the Media Session
// service during tests.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP) MockMediaSession
    : public mojom::MediaSession {
 public:
  MockMediaSession();
  explicit MockMediaSession(bool force_duck);

  MockMediaSession(const MockMediaSession&) = delete;
  MockMediaSession& operator=(const MockMediaSession&) = delete;

  ~MockMediaSession() override;

  // mojom::MediaSession overrides.
  void Suspend(SuspendType type) override;
  void Resume(SuspendType type) override;
  void StartDucking() override;
  void StopDucking() override;
  void GetMediaSessionInfo(GetMediaSessionInfoCallback callback) override;
  void AddObserver(
      mojo::PendingRemote<mojom::MediaSessionObserver> observer) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;
  void PreviousTrack() override;
  void NextTrack() override;
  void SkipAd() override;
  void PreviousSlide() override {}
  void NextSlide() override {}
  void Seek(base::TimeDelta seek_time) override;
  void Stop(SuspendType type) override;
  void GetMediaImageBitmap(const MediaImage& image,
                           int minimum_size_px,
                           int desired_size_px,
                           GetMediaImageBitmapCallback callback) override;
  void SeekTo(base::TimeDelta seek_time) override;
  void ScrubTo(base::TimeDelta scrub_to) override;
  void EnterPictureInPicture() override;
  void ExitPictureInPicture() override;
  void GetVisibility(GetVisibilityCallback callback) override;
  void SetAudioSinkId(const std::optional<std::string>& id) override {}
  void ToggleMicrophone() override {}
  void ToggleCamera() override {}
  void HangUp() override {}
  void Raise() override {}
  void SetMute(bool mute) override {}
  void RequestMediaRemoting() override {}
  void EnterAutoPictureInPicture() override {}

  void SetIsControllable(bool value);
  void SetPreferStop(bool value) { prefer_stop_ = value; }

  void AbandonAudioFocusFromClient();

  base::UnguessableToken RequestAudioFocusFromService(
      mojo::Remote<mojom::AudioFocusManager>& service,
      mojom::AudioFocusType audio_foucs_type);

  bool RequestGroupedAudioFocusFromService(
      const base::UnguessableToken& request_id,
      mojo::Remote<mojom::AudioFocusManager>& service,
      mojom::AudioFocusType audio_focus_type,
      const base::UnguessableToken& group_id);

  mojom::MediaSessionInfo::SessionState GetState() const;

  mojom::AudioFocusRequestClient* audio_focus_request() const {
    return afr_client_.get();
  }
  void FlushForTesting();

  void SimulateMetadataChanged(const std::optional<MediaMetadata>& metadata);
  void SimulatePositionChanged(const std::optional<MediaPosition>& position);

  void ClearAllImages();
  void SetImagesOfType(mojom::MediaSessionImageType type,
                       const std::vector<MediaImage>& images);

  void EnableAction(mojom::MediaSessionAction action);
  void DisableAction(mojom::MediaSessionAction action);

  int prev_track_count() const { return prev_track_count_; }
  int next_track_count() const { return next_track_count_; }
  int add_observer_count() const { return add_observer_count_; }
  int seek_count() const { return seek_count_; }
  int skip_ad_count() const { return skip_ad_count_; }
  int seek_to_count() const { return seek_to_count_; }

  bool is_scrubbing() const { return is_scrubbing_; }
  const GURL& last_image_src() const { return last_image_src_; }

  const base::UnguessableToken& request_id() const { return request_id_; }

 private:
  void SetState(mojom::MediaSessionInfo::SessionState);
  void NotifyObservers();
  mojom::MediaSessionInfoPtr GetMediaSessionInfoSync() const;
  void NotifyActionObservers();

  void RequestAudioFocusFromClient(mojom::AudioFocusType audio_focus_type);

  mojo::Remote<mojom::AudioFocusRequestClient> afr_client_;

  base::UnguessableToken request_id_;

  const bool force_duck_ = false;
  bool is_ducking_ = false;
  bool is_controllable_ = false;
  bool is_scrubbing_ = false;
  bool prefer_stop_ = false;

  int prev_track_count_ = 0;
  int next_track_count_ = 0;
  int add_observer_count_ = 0;
  int seek_count_ = 0;
  int skip_ad_count_ = 0;
  int seek_to_count_ = 0;

  std::set<mojom::MediaSessionAction> actions_;

  mojom::MediaSessionInfo::SessionState state_ =
      mojom::MediaSessionInfo::SessionState::kInactive;

  base::flat_map<mojom::MediaSessionImageType, std::vector<MediaImage>> images_;
  GURL last_image_src_;

  mojo::ReceiverSet<mojom::MediaSession> receivers_;

  mojo::RemoteSet<mojom::MediaSessionObserver> observers_;
};

}  // namespace test
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_MEDIA_SESSION_H_
