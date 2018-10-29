// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "services/media_session/media_session_service.h"
#include "services/media_session/mock_media_session.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

class MediaControllerTest : public testing::Test {
 public:
  MediaControllerTest() = default;

  void SetUp() override {
    // Create an instance of the MediaSessionService.
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            MediaSessionService::Create());
    connector_ = connector_factory_->CreateConnector();

    // Bind |audio_focus_ptr_| to AudioFocusManager.
    connector_->BindInterface("test", mojo::MakeRequest(&audio_focus_ptr_));

    // Bind |media_controller_ptr_| to MediaController.
    connector_->BindInterface("test",
                              mojo::MakeRequest(&media_controller_ptr_));
  }

  void TearDown() override {
    // Run pending tasks.
    base::RunLoop().RunUntilIdle();
  }

  void RequestAudioFocus(test::MockMediaSession& session) {
    session.RequestAudioFocusFromService(audio_focus_ptr_,
                                         mojom::AudioFocusType::kGainTransient);
  }

  mojom::MediaControllerPtr& controller() { return media_controller_ptr_; }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  mojom::AudioFocusManagerPtr audio_focus_ptr_;
  mojom::MediaControllerPtr media_controller_ptr_;

  DISALLOW_COPY_AND_ASSIGN(MediaControllerTest);
};

TEST_F(MediaControllerTest, ActiveController_Suspend) {
  test::MockMediaSession media_session;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->Suspend();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }
}

TEST_F(MediaControllerTest, ActiveController_Suspend_Multiple) {
  test::MockMediaSession media_session_1;

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  test::MockMediaSession media_session_2;

  {
    test::MockMediaSessionMojoObserver observer_1(media_session_1);
    test::MockMediaSessionMojoObserver observer_2(media_session_2);

    RequestAudioFocus(media_session_2);

    observer_1.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
    observer_2.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_2);
    controller()->Suspend();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    media_session_2.AbandonAudioFocusFromClient();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    controller()->Suspend();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }
}

TEST_F(MediaControllerTest, ActiveController_Suspend_Noop) {
  controller()->Suspend();
}

TEST_F(MediaControllerTest, ActiveController_Suspend_Noop_Abandoned) {
  test::MockMediaSession media_session;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  media_session.AbandonAudioFocusFromClient();

  controller()->Suspend();

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }
}

TEST_F(MediaControllerTest, ActiveController_SuspendResume) {
  test::MockMediaSession media_session;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->Suspend();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->Resume();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }
}

TEST_F(MediaControllerTest, ActiveController_ToggleSuspendResume_Playing) {
  test::MockMediaSession media_session;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->ToggleSuspendResume();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }
}

TEST_F(MediaControllerTest, ActiveController_ToggleSuspendResume_Ducked) {
  test::MockMediaSession media_session;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    media_session.StartDucking();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kDucking);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->ToggleSuspendResume();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }
}

TEST_F(MediaControllerTest, ActiveController_ToggleSuspendResume_Inactive) {
  test::MockMediaSession media_session;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    media_session.Stop();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kInactive);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->ToggleSuspendResume();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }
}

TEST_F(MediaControllerTest, ActiveController_ToggleSuspendResume_Paused) {
  test::MockMediaSession media_session;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->Suspend();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->ToggleSuspendResume();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }
}

TEST_F(MediaControllerTest, ActiveController_Observer_StateTransition) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::MockMediaSessionMojoObserver observer(controller());
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::MockMediaSessionMojoObserver observer(controller());
    controller()->Suspend();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kSuspended);
  }

  {
    test::MockMediaSessionMojoObserver observer(controller());
    RequestAudioFocus(media_session_2);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    media_session_1.StartDucking();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kDucking);
  }

  {
    test::MockMediaSessionMojoObserver observer(controller());
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }
}

TEST_F(MediaControllerTest, ActiveController_PreviousTrack) {
  test::MockMediaSession media_session;
  EXPECT_EQ(0, media_session.prev_track_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(0, media_session.prev_track_count());
  }

  controller()->PreviousTrack();
  controller().FlushForTesting();

  EXPECT_EQ(1, media_session.prev_track_count());
}

TEST_F(MediaControllerTest, ActiveController_NextTrack) {
  test::MockMediaSession media_session;
  EXPECT_EQ(0, media_session.next_track_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(0, media_session.next_track_count());
  }

  controller()->NextTrack();
  controller().FlushForTesting();

  EXPECT_EQ(1, media_session.next_track_count());
}

}  // namespace media_session
