// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/browser/picture_in_picture/video_picture_in_picture_window_controller_impl.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_content_browser_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/picture_in_picture_events_info.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

using testing::_;

namespace content {

class DummyPictureInPictureSessionObserver final
    : public blink::mojom::PictureInPictureSessionObserver {
 public:
  DummyPictureInPictureSessionObserver() = default;

  DummyPictureInPictureSessionObserver(
      const DummyPictureInPictureSessionObserver&) = delete;
  DummyPictureInPictureSessionObserver& operator=(
      const DummyPictureInPictureSessionObserver&) = delete;

  ~DummyPictureInPictureSessionObserver() override = default;

  // Implementation of PictureInPictureSessionObserver.
  void OnWindowSizeChanged(const gfx::Size&) override {}
  void OnStopped() override {}
};

class PictureInPictureDelegate : public WebContentsDelegate {
 public:
  PictureInPictureDelegate() = default;

  PictureInPictureDelegate(const PictureInPictureDelegate&) = delete;
  PictureInPictureDelegate& operator=(const PictureInPictureDelegate&) = delete;

  MOCK_METHOD(PictureInPictureResult,
              EnterPictureInPicture,
              (WebContents*),
              (override));
  MOCK_METHOD(bool, IsPictureInPictureEnabled, (), (const, override));
  MOCK_METHOD(bool, IsImmersivePlaybackEnabled, (), (const, override));
  MOCK_METHOD(void,
              RequestImmersivePlaybackConfirmation,
              (base::OnceCallback<
                  void(blink::mojom::ImmersivePlaybackConfirmationResultPtr)>),
              (override));

  void EnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override {
    is_fullscreen_ = true;
  }

  void ExitFullscreenModeForTab(WebContents* web_contents) override {
    is_fullscreen_ = false;
  }

  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override {
    return is_fullscreen_;
  }

 private:
  bool is_fullscreen_ = false;
};

class TestOverlayWindow : public VideoOverlayWindow {
 public:
  TestOverlayWindow() = default;

  TestOverlayWindow(const TestOverlayWindow&) = delete;
  TestOverlayWindow& operator=(const TestOverlayWindow&) = delete;

  ~TestOverlayWindow() override {}

  static std::unique_ptr<VideoOverlayWindow> Create(
      VideoPictureInPictureWindowController* controller) {
    return std::unique_ptr<VideoOverlayWindow>(
        new testing::NiceMock<TestOverlayWindow>());
  }

  bool IsActive() const override { return false; }
  void Close() override {}
  void ShowInactive() override {}
  void Hide() override {}
  bool IsVisible() const override { return false; }
  gfx::Rect GetBounds() override { return gfx::Rect(size_); }
  void UpdateNaturalSize(const gfx::Size& natural_size) override {
    size_ = natural_size;
  }
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetPlayPauseButtonVisibility(bool is_visible) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetHidePictureInPictureButtonVisibility(bool is_visible) override {}
  void SetMicrophoneMuted(bool muted) override {}
  void SetCameraState(bool turned_on) override {}
  void SetMediaMuted(bool muted) override {}
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override {}
  void SetToggleCameraButtonVisibility(bool is_visible) override {}
  void SetHangUpButtonVisibility(bool is_visible) override {}
  void SetNextSlideButtonVisibility(bool is_visible) override {}
  void SetPreviousSlideButtonVisibility(bool is_visible) override {}
  void SetMediaPosition(const media_session::MediaPosition&) override {}
  void SetSourceTitle(const std::u16string& source_title) override {}
  void SetFaviconImages(
      const std::vector<media_session::MediaImage>& images) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}
  void SetPlaybackControlsVisibility(bool is_visible) override {}
  MOCK_METHOD(void,
              SetImmersiveVideoOptions,
              (blink::mojom::ImmersiveOptionsPtr options));

 private:
  gfx::Size size_;
};

class PictureInPictureTestBrowserClient : public TestContentBrowserClient {
 public:
  PictureInPictureTestBrowserClient() = default;
  ~PictureInPictureTestBrowserClient() override = default;

  std::unique_ptr<VideoOverlayWindow> CreateWindowForVideoPictureInPicture(
      VideoPictureInPictureWindowController* controller) override {
    return TestOverlayWindow::Create(controller);
  }
};

// Helper class with a dummy implementation of the media::mojom::MediaPlayer
// mojo interface to allow providing a valid PendingRemote to StartSession from
// inside the PictureInPictureServiceImplTest unit tests.
class PictureInPictureMediaPlayerReceiver : public media::mojom::MediaPlayer {
 public:
  mojo::PendingAssociatedRemote<media::mojom::MediaPlayer>
  BindMediaPlayerReceiverAndPassRemote() {
    // A tests could potentially call StartSession() multiple times.
    receiver_.reset();
    return receiver_.BindNewEndpointAndPassDedicatedRemote();
  }

  mojo::AssociatedReceiver<media::mojom::MediaPlayer>& receiver() {
    return receiver_;
  }

  // media::mojom::MediaPlayer implementation.
  void RequestPlay(bool triggered_by_user) override {}
  void RequestPause(bool triggered_by_user) override {}
  void RequestSeekForward(base::TimeDelta seek_time) override {}
  void RequestSeekBackward(base::TimeDelta seek_time) override {}
  void RequestSeekTo(base::TimeDelta seek_time) override {}
  void RequestEnterPictureInPicture() override {}
  void RequestMute(bool mute) override {}
  void SetVolumeMultiplier(double multiplier) override {}
  void SetPersistentState(bool persistent) override {}
  void SetPowerExperimentState(bool enabled) override {}
  void SetAudioSinkId(const std::string& sink_id) override {}
  void SuspendForFrameClosed() override {}
  void RequestMediaRemoting() override {}
  void RequestVisibility(
      RequestVisibilityCallback request_visibility_callback) override {}
  void RecordAutoPictureInPictureInfo(
      const media::PictureInPictureEventsInfo::AutoPipInfo&
          auto_picture_in_picture_info) override {}

 private:
  mojo::AssociatedReceiver<media::mojom::MediaPlayer> receiver_{this};
};

class PictureInPictureServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    SetBrowserClientForTesting(&browser_client_);

    TestRenderFrameHost* render_frame_host = contents()->GetPrimaryMainFrame();
    render_frame_host->InitializeRenderFrameIfNeeded();

    contents()->SetDelegate(&delegate_);

    mojo::Remote<blink::mojom::PictureInPictureService> service_remote;
    service_impl_ = PictureInPictureServiceImpl::CreateForTesting(
        render_frame_host, service_remote.BindNewPipeAndPassReceiver());

    surface_id_ = viz::SurfaceId(
        viz::FrameSinkId(1, 1),
        viz::LocalSurfaceId(
            11, base::UnguessableToken::CreateForTesting(0x111111, 0)));

    source_bounds_ = gfx::Rect(1, 2, 3, 4);
    window_size_ = gfx::Size(42, 42);
    show_play_pause_button_ = true;
    player_id_ = 30;

    default_immersive_options_ = blink::mojom::ImmersiveOptions::New();
    default_immersive_options_->stereo_mode =
        blink::mojom::ImmersiveStereoMode::kMono;
    default_immersive_options_->projection_type =
        blink::mojom::ImmersiveProjectionType::kQuad;
  }

  void TearDown() override {
    service_impl_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  PictureInPictureServiceImpl& service() { return *service_impl_; }

  PictureInPictureDelegate& delegate() { return delegate_; }

  const viz::SurfaceId& surface_id() const { return surface_id_; }

  const gfx::Rect& source_bounds() const { return source_bounds_; }

  const gfx::Size& window_size() const { return window_size_; }

  bool show_play_pause_button() const { return show_play_pause_button_; }

  int player_id() const { return player_id_; }

  const blink::mojom::ImmersiveOptionsPtr& default_immersive_options() const {
    return default_immersive_options_;
  }

  mojo::PendingAssociatedRemote<media::mojom::MediaPlayer>
  BindMediaPlayerReceiverAndPassRemote() {
    return media_player_receiver_.BindMediaPlayerReceiverAndPassRemote();
  }

  void ResetMediaPlayerReceiver() { media_player_receiver_.receiver().reset(); }

  PictureInPictureServiceImpl::StartSessionCallback BindSession(
      mojo::Remote<blink::mojom::PictureInPictureSession>& session_remote_out,
      gfx::Size& window_size_out) {
    return base::BindLambdaForTesting(
        [&session_remote_out, &window_size_out](
            mojo::PendingRemote<blink::mojom::PictureInPictureSession> remote,
            const gfx::Size& b) {
          if (remote.is_valid()) {
            session_remote_out.Bind(std::move(remote));
          }
          window_size_out = b;
        });
  }

  void EnterFullscreen() {
    // Simulate fullscreen being entered.
    std::ignore = main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kNotifyActivation,
        blink::mojom::UserActivationNotificationType::kTest);
    main_test_rfh()->EnterFullscreen(blink::mojom::FullscreenOptions::New(),
                                     base::DoNothing());
    ASSERT_TRUE(contents()->IsFullscreen());
  }

  VideoPictureInPictureWindowControllerImpl* GetController() {
    auto* controller =
        VideoPictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
            contents());
    CHECK(controller);
    return controller;
  }

 private:
  PictureInPictureTestBrowserClient browser_client_;
  PictureInPictureDelegate delegate_;
  // Will be deleted when the frame is destroyed.
  raw_ptr<PictureInPictureServiceImpl> service_impl_;
  // Required to pass a valid PendingRemote to StartSession() in the tests.
  PictureInPictureMediaPlayerReceiver media_player_receiver_;
  viz::SurfaceId surface_id_;
  blink::mojom::ImmersiveOptionsPtr default_immersive_options_;
  gfx::Rect source_bounds_;
  gfx::Size window_size_;
  bool show_play_pause_button_;
  int player_id_;
};

TEST_F(PictureInPictureServiceImplTest, EnterPictureInPicture) {
  auto* controller = GetController();

  DummyPictureInPictureSessionObserver observer;
  mojo::Receiver<blink::mojom::PictureInPictureSessionObserver>
      observer_receiver(&observer);
  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  observer_receiver.Bind(observer_remote.InitWithNewPipeAndPassReceiver());

  // If Picture-in-Picture there shouldn't be an active session.
  EXPECT_FALSE(controller->active_session_for_testing());

  EXPECT_CALL(delegate(), IsPictureInPictureEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate(), EnterPictureInPicture(contents()))
      .WillRepeatedly(testing::Return(PictureInPictureResult::kSuccess));

  mojo::Remote<blink::mojom::PictureInPictureSession> session_remote_out;
  gfx::Size window_size_out;

  service().StartSession(player_id(), BindMediaPlayerReceiverAndPassRemote(),
                         surface_id(), window_size(), show_play_pause_button(),
                         std::move(observer_remote), source_bounds(),
                         /*request_immersive=*/false,
                         BindSession(session_remote_out, window_size_out));

  EXPECT_TRUE(session_remote_out);
  EXPECT_EQ(window_size(), window_size_out);
  EXPECT_EQ(source_bounds(), controller->GetSourceBounds());

  // Picture-in-Picture media player id should not be reset when the media is
  // destroyed (e.g. video stops playing). This allows the Picture-in-Picture
  // window to continue to control the media.
  ResetMediaPlayerReceiver();
  EXPECT_TRUE(controller->active_session_for_testing());
}

TEST_F(PictureInPictureServiceImplTest, EnterPictureInPicture_NotSupported) {
  EXPECT_CALL(delegate(), IsPictureInPictureEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate(), EnterPictureInPicture(contents()))
      .WillRepeatedly(testing::Return(PictureInPictureResult::kNotSupported));

  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  mojo::Remote<blink::mojom::PictureInPictureSession> session_remote_out;
  gfx::Size window_size_out;
  service().StartSession(player_id(), BindMediaPlayerReceiverAndPassRemote(),
                         surface_id(), window_size(), show_play_pause_button(),
                         std::move(observer_remote), source_bounds(),
                         /*request_immersive=*/false,
                         BindSession(session_remote_out, window_size_out));

  EXPECT_FALSE(GetController()->active_session_for_testing());

  // The |session_remote_out| won't be bound because the |remote| received in
  // the StartSessionCallback will be invalid due to PictureInPictureSession not
  // ever being created (meaning the the receiver won't be bound either).
  EXPECT_FALSE(session_remote_out);
  EXPECT_EQ(gfx::Size(), window_size_out);
}

TEST_F(PictureInPictureServiceImplTest, EnterImmersivePlayback) {
  DummyPictureInPictureSessionObserver observer;
  mojo::Receiver<blink::mojom::PictureInPictureSessionObserver>
      observer_receiver(&observer);
  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  observer_receiver.Bind(observer_remote.InitWithNewPipeAndPassReceiver());

  EnterFullscreen();

  EXPECT_CALL(delegate(), IsImmersivePlaybackEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate(), EnterPictureInPicture(contents()))
      .WillRepeatedly(testing::Return(PictureInPictureResult::kSuccess));

  // Expect the delegate to confirm immersive playback with default options.
  EXPECT_CALL(delegate(), RequestImmersivePlaybackConfirmation(_))
      .WillOnce([options = default_immersive_options().Clone()](
                    base::OnceCallback<void(
                        blink::mojom::ImmersivePlaybackConfirmationResultPtr)>
                        callback) mutable {
        auto result = blink::mojom::ImmersivePlaybackConfirmationResult::New();
        result->status =
            blink::mojom::ImmersivePlaybackConfirmationStatus::kConfirmed;
        result->options = std::move(options);
        std::move(callback).Run(std::move(result));
      });

  mojo::Remote<blink::mojom::PictureInPictureSession> session_remote_out;
  gfx::Size window_size_out;

  service().StartSession(player_id(), BindMediaPlayerReceiverAndPassRemote(),
                         surface_id(), window_size(), show_play_pause_button(),
                         std::move(observer_remote), source_bounds(),
                         /*request_immersive=*/true,
                         BindSession(session_remote_out, window_size_out));

  auto* controller = GetController();
  EXPECT_TRUE(session_remote_out);
  EXPECT_TRUE(controller->active_session_for_testing());
  EXPECT_TRUE(controller->IsImmersive());
}

TEST_F(PictureInPictureServiceImplTest, EnterImmersivePlayback_NotSupported) {
  DummyPictureInPictureSessionObserver observer;
  mojo::Receiver<blink::mojom::PictureInPictureSessionObserver>
      observer_receiver(&observer);
  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  observer_receiver.Bind(observer_remote.InitWithNewPipeAndPassReceiver());

  EnterFullscreen();

  EXPECT_CALL(delegate(), IsImmersivePlaybackEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate(), EnterPictureInPicture(contents()))
      .WillRepeatedly(testing::Return(PictureInPictureResult::kNotSupported));

  EXPECT_CALL(delegate(), RequestImmersivePlaybackConfirmation(_))
      .WillOnce([options = default_immersive_options().Clone()](
                    base::OnceCallback<void(
                        blink::mojom::ImmersivePlaybackConfirmationResultPtr)>
                        callback) mutable {
        auto result = blink::mojom::ImmersivePlaybackConfirmationResult::New();
        result->status =
            blink::mojom::ImmersivePlaybackConfirmationStatus::kConfirmed;
        result->options = std::move(options);
        std::move(callback).Run(std::move(result));
      });

  mojo::Remote<blink::mojom::PictureInPictureSession> session_remote_out;
  gfx::Size window_size_out;
  service().StartSession(player_id(), BindMediaPlayerReceiverAndPassRemote(),
                         surface_id(), window_size(), show_play_pause_button(),
                         std::move(observer_remote), source_bounds(),
                         /*request_immersive=*/true,
                         BindSession(session_remote_out, window_size_out));

  auto* controller = GetController();
  EXPECT_FALSE(session_remote_out);
  EXPECT_FALSE(controller->active_session_for_testing());
  EXPECT_FALSE(controller->IsImmersive());
}

TEST_F(PictureInPictureServiceImplTest,
       EnterImmersivePlayback_NoFullscreenFails) {
  // Page is not in fullscreen.
  ASSERT_FALSE(contents()->IsFullscreen());

  DummyPictureInPictureSessionObserver observer;
  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  mojo::Remote<blink::mojom::PictureInPictureSession> session_remote_out;
  gfx::Size window_size_out;

  service().StartSession(player_id(), BindMediaPlayerReceiverAndPassRemote(),
                         surface_id(), window_size(), show_play_pause_button(),
                         std::move(observer_remote), source_bounds(),
                         /*request_immersive=*/true,
                         BindSession(session_remote_out, window_size_out));

  auto* controller = GetController();
  EXPECT_FALSE(session_remote_out);
  EXPECT_FALSE(controller->active_session_for_testing());
  EXPECT_FALSE(controller->IsImmersive());
}

TEST_F(PictureInPictureServiceImplTest, EnterImmersivePlayback_DeclinedFails) {
  DummyPictureInPictureSessionObserver observer;
  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  mojo::Remote<blink::mojom::PictureInPictureSession> session_remote_out;
  gfx::Size window_size_out;

  EnterFullscreen();

  EXPECT_CALL(delegate(), IsImmersivePlaybackEnabled())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(delegate(), RequestImmersivePlaybackConfirmation(_))
      .WillOnce([](base::OnceCallback<void(
                       blink::mojom::ImmersivePlaybackConfirmationResultPtr)>
                       callback) {
        auto result = blink::mojom::ImmersivePlaybackConfirmationResult::New();
        result->status =
            blink::mojom::ImmersivePlaybackConfirmationStatus::kDeclined;
        std::move(callback).Run(std::move(result));
      });

  service().StartSession(player_id(), BindMediaPlayerReceiverAndPassRemote(),
                         surface_id(), window_size(), show_play_pause_button(),
                         std::move(observer_remote), source_bounds(),
                         /*request_immersive=*/true,
                         BindSession(session_remote_out, window_size_out));

  auto* controller = GetController();
  EXPECT_FALSE(session_remote_out);
  EXPECT_FALSE(controller->active_session_for_testing());
  EXPECT_FALSE(controller->IsImmersive());
}

TEST_F(PictureInPictureServiceImplTest,
       EnterImmersivePlayback_SubsequentSessionCancelsFirst) {
  auto* controller = GetController();

  EnterFullscreen();

  EXPECT_CALL(delegate(), IsImmersivePlaybackEnabled())
      .WillRepeatedly(testing::Return(true));

  // Capture the first confirmation callback.
  base::OnceCallback<void(blink::mojom::ImmersivePlaybackConfirmationResultPtr)>
      first_confirm_callback;
  EXPECT_CALL(delegate(), RequestImmersivePlaybackConfirmation(_))
      .WillOnce(
          [&first_confirm_callback](
              base::OnceCallback<void(
                  blink::mojom::ImmersivePlaybackConfirmationResultPtr)>
                  callback) { first_confirm_callback = std::move(callback); });

  DummyPictureInPictureSessionObserver observer;
  mojo::Receiver<blink::mojom::PictureInPictureSessionObserver>
      observer_receiver(&observer);
  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  observer_receiver.Bind(observer_remote.InitWithNewPipeAndPassReceiver());

  mojo::Remote<blink::mojom::PictureInPictureSession> first_session_remote;
  gfx::Size first_window_size;
  bool first_callback_called = false;
  bool first_callback_remote_is_valid = false;

  // Start the first immersive session.
  service().StartSession(
      player_id(), BindMediaPlayerReceiverAndPassRemote(), surface_id(),
      window_size(), show_play_pause_button(), std::move(observer_remote),
      source_bounds(),
      /*request_immersive=*/true,
      base::BindLambdaForTesting(
          [&first_session_remote, &first_window_size, &first_callback_called,
           &first_callback_remote_is_valid](
              mojo::PendingRemote<blink::mojom::PictureInPictureSession> remote,
              const gfx::Size& b) {
            first_callback_called = true;
            first_callback_remote_is_valid = remote.is_valid();
            if (remote.is_valid()) {
              first_session_remote.Bind(std::move(remote));
            }
            first_window_size = b;
          }));

  // The first session is pending confirmation.
  ASSERT_TRUE(first_confirm_callback);
  EXPECT_FALSE(controller->active_session_for_testing());
  EXPECT_FALSE(first_callback_called);

  // Now start a second session.
  // Starting a non-immersive session should immediately succeed and invalidate
  // the first one.
  EXPECT_CALL(delegate(), IsPictureInPictureEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate(), EnterPictureInPicture(contents()))
      .WillRepeatedly(testing::Return(PictureInPictureResult::kSuccess));

  DummyPictureInPictureSessionObserver observer2;
  mojo::Receiver<blink::mojom::PictureInPictureSessionObserver>
      observer_receiver2(&observer2);
  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote2;
  observer_receiver2.Bind(observer_remote2.InitWithNewPipeAndPassReceiver());

  mojo::Remote<blink::mojom::PictureInPictureSession> second_session_remote;
  gfx::Size second_window_size;

  service().StartSession(
      player_id(), BindMediaPlayerReceiverAndPassRemote(), surface_id(),
      window_size(), show_play_pause_button(), std::move(observer_remote2),
      source_bounds(),
      /*request_immersive=*/false,
      BindSession(second_session_remote, second_window_size));

  // Second session should immediately succeed.
  EXPECT_TRUE(second_session_remote);
  EXPECT_TRUE(controller->active_session_for_testing());

  // The first session callback is not called yet because the captured
  // callback (which owns the PendingSession) is still alive in the test scope.
  EXPECT_FALSE(first_callback_called);

  // Now, attempt to run the first session's confirmation callback.
  auto result = blink::mojom::ImmersivePlaybackConfirmationResult::New();
  result->status =
      blink::mojom::ImmersivePlaybackConfirmationStatus::kConfirmed;
  result->options = default_immersive_options().Clone();

  std::move(first_confirm_callback).Run(std::move(result));

  // The first session should be destroyed, which runs its callback with null.
  EXPECT_TRUE(first_callback_called);
  EXPECT_FALSE(first_callback_remote_is_valid);
  EXPECT_FALSE(first_session_remote);
}

}  // namespace content
