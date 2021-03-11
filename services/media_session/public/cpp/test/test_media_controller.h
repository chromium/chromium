// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_TEST_MEDIA_CONTROLLER_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_TEST_MEDIA_CONTROLLER_H_

#include <utility>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace media_session {
namespace test {

// A mock MediaControllerImageObserver than can be used for waiting for images.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP)
    TestMediaControllerImageObserver
    : public mojom::MediaControllerImageObserver {
 public:
  TestMediaControllerImageObserver(
      mojo::Remote<mojom::MediaController>& controller,
      int minimum_size_px,
      int desired_size_px);
  ~TestMediaControllerImageObserver() override;

  // mojom::MediaControllerImageObserver overrides.
  void MediaControllerImageChanged(mojom::MediaSessionImageType type,
                                   const SkBitmap& bitmap) override;

  void WaitForExpectedImageOfType(mojom::MediaSessionImageType type,
                                  bool expect_null_value);

 private:
  // The bool is whether the image type should be a null value.
  using ImageTypePair = std::pair<mojom::MediaSessionImageType, bool>;

  std::unique_ptr<base::RunLoop> run_loop_;

  base::Optional<ImageTypePair> expected_;
  base::Optional<ImageTypePair> current_;

  mojo::Receiver<mojom::MediaControllerImageObserver> receiver_{this};
};

// A mock MediaControllerObsever that can be used for waiting for state changes.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP)
    TestMediaControllerObserver : public mojom::MediaControllerObserver {
 public:
  explicit TestMediaControllerObserver(
      mojo::Remote<mojom::MediaController>& media_controller);

  ~TestMediaControllerObserver() override;

  // mojom::MediaControllerObserver overrides.
  void MediaSessionInfoChanged(mojom::MediaSessionInfoPtr session) override;
  void MediaSessionMetadataChanged(
      const base::Optional<MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<mojom::MediaSessionAction>& actions) override;
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override;
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override;

  void WaitForState(mojom::MediaSessionInfo::SessionState wanted_state);
  void WaitForPlaybackState(mojom::MediaPlaybackState wanted_state);
  void WaitForEmptyInfo();

  void WaitForEmptyMetadata();
  void WaitForExpectedMetadata(const MediaMetadata& metadata);

  void WaitForEmptyActions();
  void WaitForExpectedActions(
      const std::set<mojom::MediaSessionAction>& actions);

  void WaitForEmptyPosition();
  void WaitForNonEmptyPosition();

  void WaitForSession(const base::Optional<base::UnguessableToken>& request_id);

  const mojom::MediaSessionInfoPtr& session_info() const {
    return *session_info_;
  }

  const base::Optional<base::Optional<MediaMetadata>>& session_metadata()
      const {
    return session_metadata_;
  }

  const std::set<mojom::MediaSessionAction>& actions() const {
    return *session_actions_;
  }

  const base::Optional<base::Optional<MediaPosition>>& session_position() {
    return session_position_;
  }

 private:
  void StartWaiting();

  base::Optional<mojom::MediaSessionInfoPtr> session_info_;
  base::Optional<base::Optional<MediaMetadata>> session_metadata_;
  base::Optional<std::set<mojom::MediaSessionAction>> session_actions_;
  base::Optional<base::Optional<base::UnguessableToken>> session_request_id_;
  base::Optional<base::Optional<MediaPosition>> session_position_;
  bool waiting_for_empty_position_ = false;
  bool waiting_for_non_empty_position_ = false;

  base::Optional<MediaMetadata> expected_metadata_;
  base::Optional<std::set<mojom::MediaSessionAction>> expected_actions_;
  bool waiting_for_empty_metadata_ = false;

  bool waiting_for_empty_info_ = false;
  base::Optional<mojom::MediaSessionInfo::SessionState> wanted_state_;
  base::Optional<mojom::MediaPlaybackState> wanted_playback_state_;

  base::Optional<base::Optional<base::UnguessableToken>> expected_request_id_;

  std::unique_ptr<base::RunLoop> run_loop_;

  mojo::Receiver<mojom::MediaControllerObserver> receiver_{this};
};

// Implements the MediaController mojo interface for tests.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP) TestMediaController
    : public mojom::MediaController {
 public:
  TestMediaController();
  ~TestMediaController() override;

  mojo::Remote<mojom::MediaController> CreateMediaControllerRemote();

  // mojom::MediaController:
  void Suspend() override;
  void Resume() override;
  void Stop() override;
  void ToggleSuspendResume() override;
  void AddObserver(
      mojo::PendingRemote<mojom::MediaControllerObserver> observer) override;
  void PreviousTrack() override;
  void NextTrack() override;
  void Seek(base::TimeDelta seek_time) override;
  void ObserveImages(mojom::MediaSessionImageType type,
                     int minimum_size_px,
                     int desired_size_px,
                     mojo::PendingRemote<mojom::MediaControllerImageObserver>
                         observer) override {}
  void SeekTo(base::TimeDelta seek_time) override;
  void ScrubTo(base::TimeDelta seek_time) override {}
  void EnterPictureInPicture() override;
  void ExitPictureInPicture() override;
  void SetAudioSinkId(const base::Optional<std::string>& id) override {}

  int toggle_suspend_resume_count() const {
    return toggle_suspend_resume_count_;
  }

  int suspend_count() const { return suspend_count_; }
  int resume_count() const { return resume_count_; }
  int stop_count() const { return stop_count_; }
  int add_observer_count() const { return add_observer_count_; }
  int previous_track_count() const { return previous_track_count_; }
  int next_track_count() const { return next_track_count_; }
  int seek_backward_count() const { return seek_backward_count_; }
  int seek_forward_count() const { return seek_forward_count_; }
  int seek_to_count() const { return seek_to_count_; }

  base::Optional<base::TimeDelta> seek_to_time() { return seek_to_time_; }

  void SimulateMediaSessionInfoChanged(mojom::MediaSessionInfoPtr session_info);
  void SimulateMediaSessionActionsChanged(
      const std::vector<mojom::MediaSessionAction>& actions);
  void Flush();

 private:
  int toggle_suspend_resume_count_ = 0;
  int suspend_count_ = 0;
  int resume_count_ = 0;
  int stop_count_ = 0;
  int add_observer_count_ = 0;
  int previous_track_count_ = 0;
  int next_track_count_ = 0;
  int seek_backward_count_ = 0;
  int seek_forward_count_ = 0;
  int seek_to_count_ = 0;

  base::Optional<base::TimeDelta> seek_to_time_;

  mojo::RemoteSet<mojom::MediaControllerObserver> observers_;

  mojo::Receiver<mojom::MediaController> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(TestMediaController);
};

}  // namespace test
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_TEST_MEDIA_CONTROLLER_H_
