// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/media/session/audio_focus_delegate.h"
#include "content/browser/media/session/media_session_controller.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/test/mock_agent_scheduling_group_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/audio/audio_device_description.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class FakeAudioFocusDelegate : public content::AudioFocusDelegate {
 public:
  void set_audio_focus_result(AudioFocusResult result) {
    audio_focus_result_ = result;
  }

  // content::AudioFocusDelegate:
  AudioFocusResult RequestAudioFocus(
      media_session::mojom::AudioFocusType audio_focus_type) override {
    audio_focus_type_ = audio_focus_type;
    return audio_focus_result_;
  }
  void AbandonAudioFocus() override { audio_focus_type_.reset(); }
  base::Optional<media_session::mojom::AudioFocusType> GetCurrentFocusType()
      const override {
    return audio_focus_type_;
  }
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr) override {}
  const base::UnguessableToken& request_id() const override {
    return base::UnguessableToken::Null();
  }

 private:
  base::Optional<media_session::mojom::AudioFocusType> audio_focus_type_;
  AudioFocusResult audio_focus_result_ = AudioFocusResult::kSuccess;
};

// Helper class that provides an implementation of the media::mojom::MediaPlayer
// mojo interface to allow checking that messages sent over mojo are received
// with the right values in the other end.
//
// Note this relies on MediaSessionController::BindMediaPlayer() to provide the
// MediaSessionController instance owned by the test with a valid mojo remote,
// that will be bound to the mojo receiver provided by this class instead of the
// real one used in production which would be owned by HTMLMediaElement instead.
class MockMediaPlayerReceiverForTesting : public media::mojom::MediaPlayer {
 public:
  enum class PauseRequestType {
    kNone,
    kTriggeredByUser,
    kNotTriggeredByUser,
  };

  explicit MockMediaPlayerReceiverForTesting(
      MediaWebContentsObserver* media_web_contents_observer,
      const MediaPlayerId& player_id) {
    // Bind the remote to the receiver, so that we can intercept incoming
    // messages sent via the different methods that use the remote.
    media_web_contents_observer->OnMediaPlayerAdded(
        receiver_.BindNewEndpointAndPassDedicatedRemote(), player_id);
  }

  // Needs to be called from tests after invoking a method from the MediaPlayer
  // mojo interface, so that we have enough time to process the message.
  void WaitUntilReceivedMessage() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  // media::mojom::MediaPlayer implementation.
  void AddMediaPlayerObserver(
      mojo::PendingAssociatedRemote<media::mojom::MediaPlayerObserver>)
      override {}

  void RequestPlay() override {
    received_play_ = true;
    run_loop_->Quit();
  }

  void RequestPause(bool triggered_by_user) override {
    received_pause_type_ = triggered_by_user
                               ? PauseRequestType::kTriggeredByUser
                               : PauseRequestType::kNotTriggeredByUser;
    run_loop_->Quit();
  }

  void RequestSeekForward(base::TimeDelta seek_time) override {
    received_seek_forward_time_ = seek_time;
    run_loop_->Quit();
  }

  void RequestSeekBackward(base::TimeDelta seek_time) override {
    received_seek_backward_time_ = seek_time;
    run_loop_->Quit();
  }

  void RequestEnterPictureInPicture() override {}

  void RequestExitPictureInPicture() override {}

  void SetAudioSinkId(const std::string& sink_id) override {}

  // Getters used from MediaSessionControllerTest.
  bool received_play() const { return received_play_; }

  PauseRequestType received_pause() const { return received_pause_type_; }

  const base::TimeDelta& received_seek_forward_time() const {
    return received_seek_forward_time_;
  }

  const base::TimeDelta& received_seek_backward_time() const {
    return received_seek_backward_time_;
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  mojo::AssociatedReceiver<media::mojom::MediaPlayer> receiver_{this};

  bool received_play_{false};
  PauseRequestType received_pause_type_{PauseRequestType::kNone};
  base::TimeDelta received_seek_forward_time_;
  base::TimeDelta received_seek_backward_time_;
};

class MediaSessionControllerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    id_ = MediaPlayerId(contents()->GetMainFrame(), 0);
    controller_ = CreateController();
    media_player_receiver_ = CreateMediaPlayerReceiver(controller_.get());

    auto delegate = std::make_unique<FakeAudioFocusDelegate>();
    audio_focus_delegate_ = delegate.get();
    media_session()->SetDelegateForTests(std::move(delegate));
  }

  void TearDown() override {
    // Destruct the controller prior to any other teardown to avoid out of order
    // destruction relative to the MediaSession instance.
    controller_.reset();
    media_player_receiver_.reset();

    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<MediaSessionController> CreateController() {
    return std::make_unique<MediaSessionController>(id_, contents());
  }

  std::unique_ptr<MockMediaPlayerReceiverForTesting> CreateMediaPlayerReceiver(
      MediaSessionController* controller) {
    MediaWebContentsObserver* media_web_contents_observer =
        contents()->media_web_contents_observer();
    DCHECK(media_web_contents_observer);
    return std::make_unique<MockMediaPlayerReceiverForTesting>(
        media_web_contents_observer, id_);
  }

  MediaSessionImpl* media_session() {
    return MediaSessionImpl::Get(contents());
  }

  IPC::TestSink& test_sink() {
    return main_test_rfh()->GetAgentSchedulingGroup().sink();
  }

  void Suspend() {
    controller_->OnSuspend(controller_->get_player_id_for_testing());
    media_player_receiver_->WaitUntilReceivedMessage();
  }

  void Resume() {
    controller_->OnResume(controller_->get_player_id_for_testing());
    media_player_receiver_->WaitUntilReceivedMessage();
  }

  void SeekForward(base::TimeDelta seek_time) {
    controller_->OnSeekForward(controller_->get_player_id_for_testing(),
                               seek_time);
    media_player_receiver_->WaitUntilReceivedMessage();
  }

  void SeekBackward(base::TimeDelta seek_time) {
    controller_->OnSeekBackward(controller_->get_player_id_for_testing(),
                                seek_time);
    media_player_receiver_->WaitUntilReceivedMessage();
  }

  void SetVolumeMultiplier(double multiplier) {
    controller_->OnSetVolumeMultiplier(controller_->get_player_id_for_testing(),
                                       multiplier);
  }

  // Helpers to check the results of using the basic controls.
  bool ReceivedMessagePlay() { return media_player_receiver_->received_play(); }

  bool ReceivedMessagePause(bool triggered_by_user) {
    MockMediaPlayerReceiverForTesting::PauseRequestType expected_pause_request =
        triggered_by_user ? MockMediaPlayerReceiverForTesting::
                                PauseRequestType::kTriggeredByUser
                          : MockMediaPlayerReceiverForTesting::
                                PauseRequestType::kNotTriggeredByUser;
    return media_player_receiver_->received_pause() == expected_pause_request;
  }

  bool ReceivedMessageSeekForward(base::TimeDelta expected_seek_time) {
    return expected_seek_time ==
           media_player_receiver_->received_seek_forward_time();
  }

  bool ReceivedMessageSeekBackward(base::TimeDelta expected_seek_time) {
    return expected_seek_time ==
           media_player_receiver_->received_seek_backward_time();
  }

  // Legacy IPC-based helpers to check the results of using the basic controls.
  // TODO: Remove these ones as more legacy IPC messages get migrated to mojo.

  template <typename T>
  bool ReceivedMessageVolumeMultiplierUpdate(double expected_multiplier) {
    const IPC::Message* msg = test_sink().GetUniqueMessageMatching(T::ID);
    if (!msg)
      return false;

    std::tuple<int, double> result;
    if (!T::Read(msg, &result))
      return false;

    EXPECT_EQ(id_.delegate_id, std::get<0>(result));
    if (id_.delegate_id != std::get<0>(result))
      return false;

    EXPECT_EQ(expected_multiplier, std::get<1>(result));
    test_sink().ClearMessages();
    return expected_multiplier == std::get<1>(result);
  }

  MediaPlayerId id_ = MediaPlayerId::CreateMediaPlayerIdForTests();
  std::unique_ptr<MediaSessionController> controller_;
  std::unique_ptr<MockMediaPlayerReceiverForTesting> media_player_receiver_;
  FakeAudioFocusDelegate* audio_focus_delegate_ = nullptr;
};

TEST_F(MediaSessionControllerTest, NoAudioNoSession) {
  controller_->SetMetadata(false, true, media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  EXPECT_FALSE(media_session()->IsActive());
  EXPECT_FALSE(media_session()->IsControllable());
}

TEST_F(MediaSessionControllerTest, TransientNoControllableSession) {
  controller_->SetMetadata(true, false, media::MediaContentType::Transient);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  EXPECT_TRUE(media_session()->IsActive());
  EXPECT_FALSE(media_session()->IsControllable());
}

TEST_F(MediaSessionControllerTest, BasicControls) {
  controller_->SetMetadata(true, false, media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  EXPECT_TRUE(media_session()->IsActive());
  EXPECT_TRUE(media_session()->IsControllable());

  // Verify suspend notifies the renderer and maintains its session.
  Suspend();
  EXPECT_TRUE(ReceivedMessagePause(/*triggered_by_user=*/true));

  // Likewise verify the resume behavior.
  Resume();
  EXPECT_TRUE(ReceivedMessagePlay());

  // ...as well as the seek behavior.
  const base::TimeDelta kTestSeekForwardTime = base::TimeDelta::FromSeconds(1);
  SeekForward(kTestSeekForwardTime);
  EXPECT_TRUE(ReceivedMessageSeekForward(kTestSeekForwardTime));
  const base::TimeDelta kTestSeekBackwardTime = base::TimeDelta::FromSeconds(2);
  SeekBackward(kTestSeekBackwardTime);
  EXPECT_TRUE(ReceivedMessageSeekBackward(kTestSeekBackwardTime));

  // Verify destruction of the controller removes its session.
  controller_.reset();
  EXPECT_FALSE(media_session()->IsActive());
  EXPECT_FALSE(media_session()->IsControllable());
}

TEST_F(MediaSessionControllerTest, VolumeMultiplier) {
  controller_->SetMetadata(true, false, media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  EXPECT_TRUE(media_session()->IsActive());
  EXPECT_TRUE(media_session()->IsControllable());

  // Upon creation of the MediaSession the default multiplier will be sent.
  EXPECT_TRUE(ReceivedMessageVolumeMultiplierUpdate<
              MediaPlayerDelegateMsg_UpdateVolumeMultiplier>(1.0));

  // Verify a different volume multiplier is sent.
  const double kTestMultiplier = 0.5;
  SetVolumeMultiplier(kTestMultiplier);
  EXPECT_TRUE(ReceivedMessageVolumeMultiplierUpdate<
              MediaPlayerDelegateMsg_UpdateVolumeMultiplier>(kTestMultiplier));
}

TEST_F(MediaSessionControllerTest, ControllerSidePause) {
  controller_->SetMetadata(true, false, media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  EXPECT_TRUE(media_session()->IsActive());
  EXPECT_TRUE(media_session()->IsControllable());

  // Verify pause behavior.
  controller_->OnPlaybackPaused(false);
  EXPECT_FALSE(media_session()->IsActive());
  EXPECT_TRUE(media_session()->IsControllable());

  // Verify the next OnPlaybackStarted() call restores the session.
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  EXPECT_TRUE(media_session()->IsActive());
  EXPECT_TRUE(media_session()->IsControllable());
}

TEST_F(MediaSessionControllerTest, Reinitialize) {
  controller_->SetMetadata(false, true, media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  EXPECT_FALSE(media_session()->IsActive());
  EXPECT_FALSE(media_session()->IsControllable());

  // Create a transient type session.
  controller_->SetMetadata(true, false, media::MediaContentType::Transient);
  EXPECT_TRUE(media_session()->IsActive());
  EXPECT_FALSE(media_session()->IsControllable());
  const int current_player_id = controller_->get_player_id_for_testing();

  // Reinitialize the session as a content type.
  controller_->SetMetadata(true, false, media::MediaContentType::Persistent);
  EXPECT_TRUE(media_session()->IsActive());
  EXPECT_TRUE(media_session()->IsControllable());
  // Player id should not change when there's an active session.
  EXPECT_EQ(current_player_id, controller_->get_player_id_for_testing());

  // Verify suspend notifies the renderer and maintains its session.
  Suspend();
  EXPECT_TRUE(ReceivedMessagePause(/*triggered_by_user=*/true));

  // Likewise verify the resume behavior.
  Resume();
  EXPECT_TRUE(ReceivedMessagePlay());
}

TEST_F(MediaSessionControllerTest, PositionState) {
  media_session::MediaPosition expected_position(
      0.0, base::TimeDelta::FromSeconds(10), base::TimeDelta());

  controller_->OnMediaPositionStateChanged(expected_position);

  EXPECT_EQ(expected_position,
            controller_->GetPosition(controller_->get_player_id_for_testing()));
}

TEST_F(MediaSessionControllerTest, RemovePlayerIfSessionReset) {
  controller_->SetMetadata(true, false, media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  EXPECT_TRUE(media_session()->IsActive());

  controller_.reset();
  EXPECT_FALSE(media_session()->IsActive());
}

TEST_F(MediaSessionControllerTest, PictureInPictureAvailability) {
  EXPECT_FALSE(controller_->IsPictureInPictureAvailable(
      controller_->get_player_id_for_testing()));

  controller_->OnPictureInPictureAvailabilityChanged(true);
  EXPECT_TRUE(controller_->IsPictureInPictureAvailable(
      controller_->get_player_id_for_testing()));
}

TEST_F(MediaSessionControllerTest, AudioOutputSinkIdChange) {
  EXPECT_EQ(controller_->GetAudioOutputSinkId(
                controller_->get_player_id_for_testing()),
            media::AudioDeviceDescription::kDefaultDeviceId);

  controller_->OnAudioOutputSinkChanged("1");
  EXPECT_EQ(controller_->GetAudioOutputSinkId(
                controller_->get_player_id_for_testing()),
            "1");
}

TEST_F(MediaSessionControllerTest, AddPlayerWhenUnmuted) {
  contents()->SetAudioMuted(true);

  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ false,
      media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  ASSERT_FALSE(media_session()->IsActive());

  contents()->SetAudioMuted(false);
  controller_->WebContentsMutedStateChanged(false);
  EXPECT_TRUE(media_session()->IsActive());
}

TEST_F(MediaSessionControllerTest, RemovePlayerWhenMuted) {
  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ false,
      media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  ASSERT_TRUE(media_session()->IsActive());

  contents()->SetAudioMuted(true);
  controller_->WebContentsMutedStateChanged(true);
  EXPECT_FALSE(media_session()->IsActive());
}

TEST_F(MediaSessionControllerTest, EnterLeavePictureInPictureMuted) {
  contents()->SetAudioMuted(true);

  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ false,
      media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  ASSERT_FALSE(media_session()->IsActive());

  // Entering PictureInPicture means the user expects to control the media, so
  // this should activate the session.
  contents()->SetHasPictureInPictureVideo(true);
  controller_->PictureInPictureStateChanged(true);
  EXPECT_TRUE(media_session()->IsActive());

  contents()->SetHasPictureInPictureVideo(false);
  controller_->PictureInPictureStateChanged(false);
  EXPECT_FALSE(media_session()->IsActive());
}

TEST_F(MediaSessionControllerTest, MuteWithPictureInPicture) {
  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ false,
      media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  contents()->SetHasPictureInPictureVideo(true);
  controller_->PictureInPictureStateChanged(true);
  ASSERT_TRUE(media_session()->IsActive());

  contents()->SetAudioMuted(true);
  controller_->WebContentsMutedStateChanged(true);
  EXPECT_TRUE(media_session()->IsActive());
}

TEST_F(MediaSessionControllerTest, LeavePictureInPictureUnmuted) {
  contents()->SetAudioMuted(true);

  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ false,
      media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  ASSERT_FALSE(media_session()->IsActive());

  contents()->SetAudioMuted(false);
  controller_->WebContentsMutedStateChanged(false);
  contents()->SetHasPictureInPictureVideo(true);
  controller_->PictureInPictureStateChanged(true);

  // Media was unmuted, so we now have audio focus, which should keep the
  // session active.
  contents()->SetHasPictureInPictureVideo(false);
  controller_->PictureInPictureStateChanged(false);
  EXPECT_TRUE(media_session()->IsActive());
}

TEST_F(MediaSessionControllerTest, AddPlayerWhenAddingAudio) {
  controller_->SetMetadata(
      /* has_audio = */ false, /* has_video = */ true,
      media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  ASSERT_FALSE(media_session()->IsActive());

  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ false,
      media::MediaContentType::Persistent);
  EXPECT_TRUE(media_session()->IsActive());
}

TEST_F(MediaSessionControllerTest,
       AddPlayerWhenEnteringPictureInPictureWithNoAudio) {
  controller_->SetMetadata(
      /* has_audio = */ false, /* has_video = */ true,
      media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  ASSERT_FALSE(media_session()->IsActive());

  contents()->SetHasPictureInPictureVideo(true);
  controller_->PictureInPictureStateChanged(true);
  EXPECT_TRUE(media_session()->IsActive());
}

TEST_F(MediaSessionControllerTest,
       AddPlayerWhenEnteringPictureInPicturePaused) {
  controller_->SetMetadata(
      /*has_audio=*/false, /*has_video=*/true,
      media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  controller_->OnPlaybackPaused(/*reached_end_of_stream=*/false);
  ASSERT_FALSE(media_session()->IsActive());

  contents()->SetHasPictureInPictureVideo(true);
  controller_->PictureInPictureStateChanged(true);
  EXPECT_FALSE(media_session()->IsActive());
}

TEST_F(MediaSessionControllerTest,
       AddPlayerInitiallyPictureInPictureWithNoAudio) {
  contents()->SetHasPictureInPictureVideo(true);

  controller_->SetMetadata(
      /* has_audio = */ false, /* has_video = */ true,
      media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  EXPECT_TRUE(media_session()->IsActive());

  contents()->SetHasPictureInPictureVideo(false);
  controller_->PictureInPictureStateChanged(false);

  EXPECT_FALSE(media_session()->IsActive());
}

TEST_F(MediaSessionControllerTest, HasVideo_True) {
  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ true,
      media::MediaContentType::Persistent);
  EXPECT_TRUE(controller_->HasVideo(controller_->get_player_id_for_testing()));
}

TEST_F(MediaSessionControllerTest, HasVideo_False) {
  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ false,
      media::MediaContentType::Persistent);
  EXPECT_FALSE(controller_->HasVideo(controller_->get_player_id_for_testing()));
}

TEST_F(MediaSessionControllerTest, AudioFocusRequestFailure) {
  // Start playback with the audio track only.
  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ false,
      media::MediaContentType::Persistent);
  ASSERT_TRUE(controller_->OnPlaybackStarted());
  ASSERT_TRUE(media_session()->IsActive());

  // Add a video track while audio focus cannot be obtained.
  audio_focus_delegate_->set_audio_focus_result(
      AudioFocusDelegate::AudioFocusResult::kFailed);
  media_session()->Suspend(MediaSession::SuspendType::kSystem);
  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ true,
      media::MediaContentType::Persistent);
  EXPECT_FALSE(media_session()->IsActive());

  // Have a one-shot player re-activate the session, then discard it.
  audio_focus_delegate_->set_audio_focus_result(
      AudioFocusDelegate::AudioFocusResult::kSuccess);
  auto transient_controller = CreateController();
  transient_controller->SetMetadata(
      /* has_audio = */ true, /* has_video = */ true,
      media::MediaContentType::OneShot);
  ASSERT_TRUE(transient_controller->OnPlaybackStarted());
  EXPECT_TRUE(media_session()->IsActive());
  transient_controller->OnPlaybackPaused(false);

  // Activate the first player.
  controller_->SetMetadata(
      /* has_audio = */ true, /* has_video = */ true,
      media::MediaContentType::Persistent);
  EXPECT_TRUE(media_session()->IsActive());

  // Remove the controller's session and make sure position updates are simply
  // ignored (no active player).
  controller_.reset();
  EXPECT_FALSE(media_session()->IsActive());
  media_session()->RebuildAndNotifyMediaPositionChanged();
}

}  // namespace content
