// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_controller.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/media_session_service_impl.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

class MediaControllerTest : public testing::Test {
 public:
  MediaControllerTest() = default;

  MediaControllerTest(const MediaControllerTest&) = delete;
  MediaControllerTest& operator=(const MediaControllerTest&) = delete;

  void SetUp() override {
    // Create an instance of the MediaSessionService and bind some interfaces.
    service_ = std::make_unique<MediaSessionServiceImpl>();
    service_->BindAudioFocusManager(
        audio_focus_remote_.BindNewPipeAndPassReceiver());
    service_->BindMediaControllerManager(
        controller_manager_remote_.BindNewPipeAndPassReceiver());

    controller_manager_remote_->CreateActiveMediaController(
        media_controller_remote_.BindNewPipeAndPassReceiver());
    controller_manager_remote_.FlushForTesting();

    audio_focus_remote_->SetEnforcementMode(
        mojom::EnforcementMode::kSingleSession);
    audio_focus_remote_.FlushForTesting();
  }

  void TearDown() override {
    // Run pending tasks.
    base::RunLoop().RunUntilIdle();
  }

  void RequestAudioFocus(test::MockMediaSession& session,
                         mojom::AudioFocusType type) {
    session.RequestAudioFocusFromService(audio_focus_remote_, type);
  }

  mojo::Remote<mojom::MediaController>& controller() {
    return media_controller_remote_;
  }

  mojo::Remote<mojom::MediaControllerManager>& manager() {
    return controller_manager_remote_;
  }

  static size_t GetImageObserverCount(const MediaController& controller) {
    return controller.image_observers_.size();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MediaSessionService> service_;
  mojo::Remote<mojom::AudioFocusManager> audio_focus_remote_;
  mojo::Remote<mojom::MediaController> media_controller_remote_;
  mojo::Remote<mojom::MediaControllerManager> controller_manager_remote_;
};

TEST_F(MediaControllerTest, ActiveController_Suspend) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->Suspend();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }
}

TEST_F(MediaControllerTest, ActiveController_Multiple_Abandon_Top) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  media_session_1.SetIsControllable(true);
  media_session_2.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer_1(media_session_1);
    test::MockMediaSessionMojoObserver observer_2(media_session_2);

    RequestAudioFocus(media_session_2, mojom::AudioFocusType::kGain);

    observer_1.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
    observer_2.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session_2.AbandonAudioFocusFromClient();

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    controller()->Resume();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }
}

TEST_F(MediaControllerTest,
       ActiveController_Multiple_Abandon_UnderNonControllable) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;
  test::MockMediaSession media_session_3;

  media_session_1.SetIsControllable(true);
  media_session_2.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer_1(media_session_1);
    test::MockMediaSessionMojoObserver observer_2(media_session_2);

    RequestAudioFocus(media_session_2, mojom::AudioFocusType::kGain);

    observer_1.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
    observer_2.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer_2(media_session_2);
    test::MockMediaSessionMojoObserver observer_3(media_session_3);

    RequestAudioFocus(media_session_3, mojom::AudioFocusType::kGain);

    observer_2.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
    observer_3.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  media_session_2.AbandonAudioFocusFromClient();

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    controller()->Resume();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }
}

TEST_F(MediaControllerTest, ActiveController_Multiple_Controllable) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  media_session_1.SetIsControllable(true);
  media_session_2.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer_1(media_session_1);
    test::MockMediaSessionMojoObserver observer_2(media_session_2);

    RequestAudioFocus(media_session_2, mojom::AudioFocusType::kGain);

    observer_1.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
    observer_2.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_2);
    controller()->Suspend();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }
}

TEST_F(MediaControllerTest, ActiveController_Multiple_NonControllable) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  media_session_1.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  EXPECT_EQ(2, media_session_1.add_observer_count());

  {
    test::MockMediaSessionMojoObserver observer_1(media_session_1);
    test::MockMediaSessionMojoObserver observer_2(media_session_2);

    RequestAudioFocus(media_session_2, mojom::AudioFocusType::kGain);

    observer_1.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
    observer_2.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  // The top session has changed but the controller is still bound to
  // |media_session_1|. We should make sure we do not add an observer if we
  // already have one.
  EXPECT_EQ(3, media_session_1.add_observer_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    controller()->Resume();
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  EXPECT_EQ(4, media_session_1.add_observer_count());
}

TEST_F(MediaControllerTest, ActiveController_Multiple_UpdateControllable) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  media_session_1.SetIsControllable(true);
  media_session_2.SetIsControllable(true);

  EXPECT_EQ(0, media_session_1.add_observer_count());
  EXPECT_EQ(0, media_session_2.add_observer_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  EXPECT_EQ(2, media_session_1.add_observer_count());
  EXPECT_EQ(0, media_session_2.add_observer_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session_2);
    RequestAudioFocus(media_session_2, mojom::AudioFocusType::kGainTransient);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  EXPECT_EQ(2, media_session_1.add_observer_count());
  EXPECT_EQ(2, media_session_2.add_observer_count());

  media_session_2.SetIsControllable(false);
  media_session_2.FlushForTesting();

  EXPECT_EQ(3, media_session_1.add_observer_count());
  EXPECT_EQ(2, media_session_2.add_observer_count());

  media_session_1.SetIsControllable(false);
  media_session_1.FlushForTesting();

  EXPECT_EQ(3, media_session_1.add_observer_count());
  EXPECT_EQ(2, media_session_2.add_observer_count());
}

TEST_F(MediaControllerTest, ActiveController_Suspend_Noop) {
  controller()->Suspend();
}

TEST_F(MediaControllerTest, ActiveController_Suspend_Noop_Abandoned) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  media_session.AbandonAudioFocusFromClient();

  controller()->Suspend();

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }
}

TEST_F(MediaControllerTest, ActiveController_SuspendResume) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
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
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
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
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
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
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    media_session.Stop(mojom::MediaSession::SuspendType::kUI);
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
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
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

  media_session_1.SetIsControllable(true);
  media_session_2.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    controller()->Suspend();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kSuspended);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    RequestAudioFocus(media_session_2, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    media_session_1.StartDucking();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kDucking);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }
}

TEST_F(MediaControllerTest, ActiveController_PreviousTrack) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  EXPECT_EQ(0, media_session.prev_track_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(0, media_session.prev_track_count());
  }

  controller()->PreviousTrack();
  controller().FlushForTesting();

  EXPECT_EQ(1, media_session.prev_track_count());
}

TEST_F(MediaControllerTest, ActiveController_NextTrack) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  EXPECT_EQ(0, media_session.next_track_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(0, media_session.next_track_count());
  }

  controller()->NextTrack();
  controller().FlushForTesting();

  EXPECT_EQ(1, media_session.next_track_count());
}

TEST_F(MediaControllerTest, ActiveController_Seek) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  EXPECT_EQ(0, media_session.seek_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(0, media_session.seek_count());
  }

  controller()->Seek(base::Seconds(mojom::kDefaultSeekTimeSeconds));
  controller().FlushForTesting();

  EXPECT_EQ(1, media_session.seek_count());
}

TEST_F(MediaControllerTest, ActiveController_SkipAd) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  EXPECT_EQ(0, media_session.skip_ad_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(0, media_session.skip_ad_count());
  }

  controller()->SkipAd();
  controller().FlushForTesting();

  EXPECT_EQ(1, media_session.skip_ad_count());
}

TEST_F(MediaControllerTest, ActiveController_SeekTo) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  EXPECT_EQ(0, media_session.seek_to_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);

    EXPECT_EQ(0, media_session.seek_to_count());
  }

  controller()->SeekTo(base::Seconds(mojom::kDefaultSeekTimeSeconds));
  controller().FlushForTesting();

  EXPECT_EQ(1, media_session.seek_to_count());
}

TEST_F(MediaControllerTest, ActiveController_ScrubTo) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  EXPECT_FALSE(media_session.is_scrubbing());
  EXPECT_EQ(0, media_session.seek_to_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);

    EXPECT_FALSE(media_session.is_scrubbing());
    EXPECT_EQ(0, media_session.seek_to_count());
  }

  controller()->ScrubTo(base::Seconds(mojom::kDefaultSeekTimeSeconds));
  controller().FlushForTesting();

  EXPECT_TRUE(media_session.is_scrubbing());
  EXPECT_EQ(0, media_session.seek_to_count());

  controller()->ScrubTo(base::Seconds(mojom::kDefaultSeekTimeSeconds));
  controller().FlushForTesting();

  EXPECT_TRUE(media_session.is_scrubbing());
  EXPECT_EQ(0, media_session.seek_to_count());

  controller()->SeekTo(base::Seconds(mojom::kDefaultSeekTimeSeconds));
  controller().FlushForTesting();

  EXPECT_FALSE(media_session.is_scrubbing());
  EXPECT_EQ(1, media_session.seek_to_count());
}

TEST_F(MediaControllerTest, ActiveController_Metadata_Observer_Abandoned) {
  MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";
  metadata.album = u"album";

  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaMetadata> test_metadata(metadata);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session.SimulateMetadataChanged(test_metadata);
  media_session.AbandonAudioFocusFromClient();

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForEmptyMetadata();
  }
}

TEST_F(MediaControllerTest, ActiveController_Metadata_Observer_Empty) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaMetadata> test_metadata;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    media_session.SimulateMetadataChanged(test_metadata);
    observer.WaitForEmptyMetadata();
  }
}

TEST_F(MediaControllerTest, ActiveController_Metadata_Observer_WithInfo) {
  std::vector<media_session::ChapterInformation> expected_chapters;

  media_session::MediaImage test_image_1;
  test_image_1.src = GURL("https://www.google.com");
  media_session::MediaImage test_image_2;
  test_image_2.src = GURL("https://www.example.org");

  media_session::ChapterInformation test_chapter_1(
      /*title=*/u"chapter1", /*start_time=*/base::Seconds(10),
      /*artwork=*/{test_image_1});

  media_session::ChapterInformation test_chapter_2(
      /*title=*/u"chapter2", /*start_time=*/base::Seconds(20),
      /*artwork=*/{test_image_2});

  expected_chapters.push_back(test_chapter_1);
  expected_chapters.push_back(test_chapter_2);

  MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";
  metadata.album = u"album";
  metadata.chapters = expected_chapters;

  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaMetadata> test_metadata(metadata);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    media_session.SimulateMetadataChanged(test_metadata);
    observer.WaitForExpectedMetadata(metadata);
  }
}

TEST_F(MediaControllerTest, ActiveController_Metadata_AddObserver_Empty) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaMetadata> test_metadata;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session.SimulateMetadataChanged(test_metadata);

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForEmptyMetadata();
  }
}

TEST_F(MediaControllerTest, ActiveController_Metadata_AddObserver_WithInfo) {
  std::vector<media_session::ChapterInformation> expected_chapters;

  media_session::MediaImage test_image_1;
  test_image_1.src = GURL("https://www.google.com");
  media_session::MediaImage test_image_2;
  test_image_2.src = GURL("https://www.example.org");

  media_session::ChapterInformation test_chapter_1(
      /*title=*/u"chapter1", /*start_time=*/base::Seconds(10),
      /*artwork=*/{test_image_1});

  media_session::ChapterInformation test_chapter_2(
      /*title=*/u"chapter2", /*start_time=*/base::Seconds(20),
      /*artwork=*/{test_image_2});

  expected_chapters.push_back(test_chapter_1);
  expected_chapters.push_back(test_chapter_2);

  MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";
  metadata.album = u"album";
  metadata.chapters = expected_chapters;

  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaMetadata> test_metadata(metadata);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session.SimulateMetadataChanged(test_metadata);

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForExpectedMetadata(metadata);
  }
}

TEST_F(MediaControllerTest, ActiveController_Stop) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->Stop();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kInactive);
  }
}

TEST_F(MediaControllerTest, BoundController_Routing) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  media_session_1.SetIsControllable(true);
  media_session_2.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  mojo::Remote<mojom::MediaController> controller;
  manager()->CreateMediaControllerForSession(
      controller.BindNewPipeAndPassReceiver(), media_session_1.request_id());
  manager().FlushForTesting();

  EXPECT_EQ(0, media_session_1.next_track_count());

  controller->NextTrack();
  controller.FlushForTesting();

  EXPECT_EQ(1, media_session_1.next_track_count());

  {
    test::MockMediaSessionMojoObserver observer(media_session_2);
    RequestAudioFocus(media_session_2, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  EXPECT_EQ(1, media_session_1.next_track_count());
  EXPECT_EQ(0, media_session_2.next_track_count());

  controller->NextTrack();
  controller.FlushForTesting();

  EXPECT_EQ(2, media_session_1.next_track_count());
  EXPECT_EQ(0, media_session_2.next_track_count());
}

TEST_F(MediaControllerTest, BoundController_BadRequestId) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  mojo::Remote<mojom::MediaController> controller;
  manager()->CreateMediaControllerForSession(
      controller.BindNewPipeAndPassReceiver(),
      base::UnguessableToken::Create());
  manager().FlushForTesting();

  EXPECT_EQ(0, media_session.next_track_count());

  controller->NextTrack();
  controller.FlushForTesting();

  EXPECT_EQ(0, media_session.next_track_count());
}

TEST_F(MediaControllerTest, BoundController_DropOnAbandon) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  mojo::Remote<mojom::MediaController> controller;
  manager()->CreateMediaControllerForSession(
      controller.BindNewPipeAndPassReceiver(), media_session.request_id());
  manager().FlushForTesting();

  EXPECT_EQ(0, media_session.next_track_count());

  controller->NextTrack();
  controller.FlushForTesting();

  EXPECT_EQ(1, media_session.next_track_count());

  media_session.AbandonAudioFocusFromClient();

  EXPECT_EQ(1, media_session.next_track_count());

  controller->NextTrack();
  controller.FlushForTesting();

  EXPECT_EQ(1, media_session.next_track_count());
}

TEST_F(MediaControllerTest, ActiveController_Actions_AddObserver_Empty) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForEmptyActions();
  }
}

TEST_F(MediaControllerTest, ActiveController_Actions_AddObserver_WithInfo) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session.EnableAction(mojom::MediaSessionAction::kPlay);

  {
    test::TestMediaControllerObserver observer(controller());

    std::set<mojom::MediaSessionAction> expected_actions;
    expected_actions.insert(mojom::MediaSessionAction::kPlay);
    observer.WaitForExpectedActions(expected_actions);
  }
}

TEST_F(MediaControllerTest, ActiveController_Actions_Observer_Empty) {
  test::MockMediaSession media_session;
  media_session.EnableAction(mojom::MediaSessionAction::kPlay);
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    media_session.DisableAction(mojom::MediaSessionAction::kPlay);
    observer.WaitForEmptyActions();
  }
}

TEST_F(MediaControllerTest, ActiveController_Actions_Observer_WithInfo) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    media_session.EnableAction(mojom::MediaSessionAction::kPlay);

    std::set<mojom::MediaSessionAction> expected_actions;
    expected_actions.insert(mojom::MediaSessionAction::kPlay);
    observer.WaitForExpectedActions(expected_actions);
  }
}

TEST_F(MediaControllerTest, ActiveController_Actions_Observer_Abandoned) {
  test::MockMediaSession media_session;
  media_session.EnableAction(mojom::MediaSessionAction::kPlay);
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session.AbandonAudioFocusFromClient();

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForEmptyActions();
  }
}

TEST_F(MediaControllerTest, ActiveController_Position_Observer_Empty) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaPosition> test_position;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    media_session.SimulatePositionChanged(test_position);
    observer.WaitForEmptyPosition();
  }
}

TEST_F(MediaControllerTest, ActiveController_Position_Observer_WithInfo) {
  MediaPosition position(
      /*playback_rate=*/1,
      /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaPosition> test_position(position);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    media_session.SimulatePositionChanged(test_position);
    observer.WaitForNonEmptyPosition();
  }
}

TEST_F(MediaControllerTest, ActiveController_Position_AddObserver_Empty) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaPosition> test_position;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session.SimulatePositionChanged(test_position);

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForEmptyPosition();
  }
}

TEST_F(MediaControllerTest, ActiveController_Position_AddObserver_WithInfo) {
  MediaPosition position(
      /*playback_rate=*/1,
      /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaPosition> test_position(position);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session.SimulatePositionChanged(test_position);

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForNonEmptyPosition();
  }
}

TEST_F(MediaControllerTest, ActiveController_Position_Observer_Abandoned) {
  MediaPosition position(
      /*playback_rate=*/1,
      /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaPosition> test_position(position);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session.SimulatePositionChanged(test_position);
  media_session.AbandonAudioFocusFromClient();

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForEmptyPosition();
  }
}

TEST_F(MediaControllerTest, ActiveController_Observer_Abandoned) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    media_session.AbandonAudioFocusFromClient();

    // We should see empty info, metadata, actions, and position flushed since
    // the active controller is no longer bound to a media session.
    observer.WaitForEmptyInfo();
    observer.WaitForEmptyMetadata();
    observer.WaitForEmptyActions();
    observer.WaitForEmptyPosition();
  }
}

TEST_F(MediaControllerTest, ActiveController_AddObserver_Abandoned) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session.AbandonAudioFocusFromClient();

  {
    test::TestMediaControllerObserver observer(controller());

    // We should see empty info, metadata, actions, and position since the
    // active controller is no longer bound to a media session.
    observer.WaitForEmptyInfo();
    observer.WaitForEmptyMetadata();
    observer.WaitForEmptyActions();
    observer.WaitForEmptyPosition();
  }
}

TEST_F(MediaControllerTest, ClearImageObserverOnError) {
  MediaController controller;

  mojo::Remote<mojom::MediaController> controller_remote;
  controller.BindToInterface(controller_remote.BindNewPipeAndPassReceiver());
  EXPECT_EQ(0u, GetImageObserverCount(controller));

  {
    test::TestMediaControllerImageObserver observer(controller_remote, 0, 0);
    EXPECT_EQ(2u, GetImageObserverCount(controller));
  }

  EXPECT_EQ(2u, GetImageObserverCount(controller));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, GetImageObserverCount(controller));
}

TEST_F(MediaControllerTest, ActiveController_SimulateImagesChanged) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  std::vector<MediaImage> images;
  MediaImage image;
  image.src = GURL("https://www.google.com");
  images.push_back(image);

  {
    test::TestMediaControllerImageObserver observer(controller(), 0, 0);

    // By default, the image is empty but no notification should be received.
    EXPECT_TRUE(media_session.last_image_src().is_empty());

    // Check that we receive the correct image and that it was requested from
    // |media_session| by the controller.
    media_session.SetImagesOfType(mojom::MediaSessionImageType::kArtwork,
                                  images);
    observer.WaitForExpectedImageOfType(mojom::MediaSessionImageType::kArtwork,
                                        false);
    EXPECT_EQ(image.src, media_session.last_image_src());

    // Check that we flush the observer with an empty image. Since the image is
    // empty the last downloaded image by |media_session| should still be the
    // previous image.
    media_session.SetImagesOfType(mojom::MediaSessionImageType::kArtwork,
                                  std::vector<MediaImage>());
    observer.WaitForExpectedImageOfType(mojom::MediaSessionImageType::kArtwork,
                                        true);
    EXPECT_EQ(image.src, media_session.last_image_src());
  }
}

TEST_F(MediaControllerTest,
       ActiveController_SimulateImagesChanged_ToggleControllable) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  std::vector<MediaImage> images;
  MediaImage image;
  image.src = GURL("https://www.google.com");
  images.push_back(image);
  media_session.SetImagesOfType(mojom::MediaSessionImageType::kArtwork, images);

  {
    test::TestMediaControllerImageObserver observer(controller(), 0, 0);
    observer.WaitForExpectedImageOfType(mojom::MediaSessionImageType::kArtwork,
                                        false);
    EXPECT_EQ(image.src, media_session.last_image_src());

    // When the |media_session| becomes uncontrollable it is unbound from the
    // media controller and we should flush the observer with an empty image.
    media_session.SetIsControllable(false);
    observer.WaitForExpectedImageOfType(mojom::MediaSessionImageType::kArtwork,
                                        true);

    // When the |media_session| becomes controllable again it will be bound to
    // the media controller and we should flush the observer with the current
    // images.
    media_session.SetIsControllable(true);
    observer.WaitForExpectedImageOfType(mojom::MediaSessionImageType::kArtwork,
                                        false);
    EXPECT_EQ(image.src, media_session.last_image_src());
  }
}

TEST_F(MediaControllerTest,
       ActiveController_SimulateImagesChanged_TypeChanged) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  std::vector<MediaImage> images;
  MediaImage image;
  image.src = GURL("https://www.google.com");
  images.push_back(image);
  media_session.SetImagesOfType(mojom::MediaSessionImageType::kArtwork, images);

  {
    test::TestMediaControllerImageObserver observer(controller(), 0, 0);
    observer.WaitForExpectedImageOfType(mojom::MediaSessionImageType::kArtwork,
                                        false);
    EXPECT_EQ(image.src, media_session.last_image_src());

    // If we clear all the images associated with the media session we should
    // flush all the observers.
    media_session.ClearAllImages();
    observer.WaitForExpectedImageOfType(mojom::MediaSessionImageType::kArtwork,
                                        true);
    EXPECT_EQ(image.src, media_session.last_image_src());
  }
}

TEST_F(MediaControllerTest,
       ActiveController_SimulateImagesChanged_MinSizeCutoff) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  std::vector<MediaImage> images;
  MediaImage image1;
  image1.src = GURL("https://www.google.com");
  image1.sizes.push_back(gfx::Size(1, 1));

  media_session.SetImagesOfType(mojom::MediaSessionImageType::kArtwork,
                                {image1});

  {
    test::TestMediaControllerImageObserver observer(controller(), 5, 10);

    // The observer requires an image that is at least 5px but the only image
    // we have is 1px so the observer will not be notified.
    EXPECT_TRUE(media_session.last_image_src().is_empty());

    MediaImage image2;
    image2.src = GURL("https://www.example.com");
    image2.sizes.push_back(gfx::Size(10, 10));

    // Update the media session with two images, one that is too small and one
    // that is the right size. We should receive the second image through the
    // observer.
    media_session.SetImagesOfType(mojom::MediaSessionImageType::kArtwork,
                                  {image1, image2});
    observer.WaitForExpectedImageOfType(mojom::MediaSessionImageType::kArtwork,
                                        false);
    EXPECT_EQ(image2.src, media_session.last_image_src());

    // Use the first set of images again.
    media_session.SetImagesOfType(mojom::MediaSessionImageType::kArtwork,
                                  {image1});
    // The observer requires as image that is at least 5px and should now be
    // notified that the image was cleared.
    observer.WaitForExpectedImageOfType(mojom::MediaSessionImageType::kArtwork,
                                        true);
  }
}

TEST_F(MediaControllerTest,
       ActiveController_SimulateImagesChanged_DesiredSize) {
  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  std::vector<MediaImage> images;
  MediaImage image1;
  image1.src = GURL("https://www.google.com");
  image1.sizes.push_back(gfx::Size(10, 10));
  images.push_back(image1);

  MediaImage image2;
  image2.src = GURL("https://www.example.com");
  image2.sizes.push_back(gfx::Size(9, 9));
  images.push_back(image2);

  media_session.SetImagesOfType(mojom::MediaSessionImageType::kArtwork, images);

  {
    test::TestMediaControllerImageObserver observer(controller(), 5, 10);

    // The media session has two images, but the first one is closer to the 10px
    // desired size that the observer has specified. Therefore, the observer
    // should receive that image.
    media_session.SetImagesOfType(mojom::MediaSessionImageType::kArtwork,
                                  images);
    observer.WaitForExpectedImageOfType(mojom::MediaSessionImageType::kArtwork,
                                        false);
    EXPECT_EQ(image1.src, media_session.last_image_src());
  }
}

TEST_F(MediaControllerTest, ActiveController_Observer_SessionChanged) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  media_session_1.SetIsControllable(true);
  media_session_2.SetIsControllable(true);

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForSession(std::nullopt);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForSession(media_session_1.request_id());
  }

  {
    test::TestMediaControllerObserver observer(controller());
    RequestAudioFocus(media_session_2, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerObserver observer(controller());
    observer.WaitForSession(media_session_2.request_id());
  }

  {
    test::TestMediaControllerObserver observer(controller());
    media_session_2.AbandonAudioFocusFromClient();
    observer.WaitForSession(media_session_1.request_id());
  }

  {
    test::TestMediaControllerObserver observer(controller());
    media_session_1.SetIsControllable(false);
    observer.WaitForSession(std::nullopt);
  }
}

TEST_F(MediaControllerTest, BoundController_Observer_SessionChanged) {
  test::MockMediaSession media_session;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  mojo::Remote<mojom::MediaController> controller;
  manager()->CreateMediaControllerForSession(
      controller.BindNewPipeAndPassReceiver(), media_session.request_id());
  manager().FlushForTesting();

  {
    test::TestMediaControllerObserver observer(controller);
    observer.WaitForSession(media_session.request_id());
  }
}

TEST_F(MediaControllerTest, Manager_SuspendAllSessions) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1, mojom::AudioFocusType::kGain);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_2);
    RequestAudioFocus(media_session_2,
                      mojom::AudioFocusType::kGainTransientMayDuck);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPlaying);
  }

  manager()->SuspendAllSessions();

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_2);
    observer.WaitForPlaybackState(mojom::MediaPlaybackState::kPaused);
  }
}

TEST_F(MediaControllerTest, ActiveController_SimulateChapterChanged) {
  std::vector<media_session::ChapterInformation> expected_chapters;

  media_session::MediaImage test_image_1;
  test_image_1.src = GURL("https://www.google.com");
  media_session::MediaImage test_image_2;
  test_image_2.src = GURL("https://www.example.org");

  media_session::ChapterInformation test_chapter_1(
      /*title=*/u"chapter1", /*start_time=*/base::Seconds(10),
      /*artwork=*/{test_image_1});

  media_session::ChapterInformation test_chapter_2(
      /*title=*/u"chapter2", /*start_time=*/base::Seconds(20),
      /*artwork=*/{test_image_2});

  expected_chapters.push_back(test_chapter_1);
  expected_chapters.push_back(test_chapter_2);

  MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";
  metadata.album = u"album";
  metadata.chapters = expected_chapters;

  test::MockMediaSession media_session;
  media_session.SetIsControllable(true);

  std::optional<MediaMetadata> test_metadata(metadata);

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, mojom::AudioFocusType::kGain);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::TestMediaControllerImageObserver observer(controller(), 0, 0);

    // By default, the image is empty but no notification should be received.
    EXPECT_TRUE(media_session.last_image_src().is_empty());

    // Checks that we receive the correct image and that it was requested from
    // `media_session` by the controller.
    media_session.SimulateMetadataChanged(test_metadata);
    base::RunLoop().RunUntilIdle();
    observer.WaitForExpectedChapterImage(0, /*expect_null_image=*/false);
    observer.WaitForExpectedChapterImage(1, /*expect_null_image=*/false);
    EXPECT_EQ(test_image_2.src, media_session.last_image_src());

    MediaMetadata metadata1;
    metadata1.title = u"title1";
    metadata1.artist = u"artist1";
    metadata1.album = u"album1";
    std::optional<MediaMetadata> test_metadata1(metadata1);

    // Checks that we receive the correct image and that it was requested from
    // `media_session` by the controller after a media change with no chapter.
    media_session.SimulateMetadataChanged(test_metadata1);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(test_image_2.src, media_session.last_image_src());

    media_session::MediaImage test_image_3;
    test_image_3.src = GURL("https://www.chrome.com");
    media_session::ChapterInformation test_chapter_3(
        /*title=*/u"chapter3", /*start_time=*/base::Seconds(30),
        /*artwork=*/{test_image_3});

    MediaMetadata metadata2;
    metadata2.title = u"title2";
    metadata2.artist = u"artist2";
    metadata2.album = u"album2";
    metadata2.chapters = {test_chapter_1, test_chapter_2, test_chapter_3};
    std::optional<MediaMetadata> test_metadata2(metadata2);

    // Checks that we receive the correct image and that it was requested from
    // `media_session` by the controller after a media change with 3 chapters.
    media_session.SimulateMetadataChanged(test_metadata2);
    base::RunLoop().RunUntilIdle();
    observer.WaitForExpectedChapterImage(0, /*expect_null_image=*/false);
    observer.WaitForExpectedChapterImage(1, /*expect_null_image=*/false);
    observer.WaitForExpectedChapterImage(2, /*expect_null_image=*/false);
    EXPECT_EQ(test_image_3.src, media_session.last_image_src());
  }
}

}  // namespace media_session
