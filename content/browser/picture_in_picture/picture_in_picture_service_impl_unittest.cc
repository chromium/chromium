// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/browser/picture_in_picture/video_picture_in_picture_window_controller_impl.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

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

  MOCK_METHOD1(EnterPictureInPicture, PictureInPictureResult(WebContents*));
};

class TestOverlayWindow : public VideoOverlayWindow {
 public:
  TestOverlayWindow() = default;

  TestOverlayWindow(const TestOverlayWindow&) = delete;
  TestOverlayWindow& operator=(const TestOverlayWindow&) = delete;

  ~TestOverlayWindow() override {}

  static std::unique_ptr<VideoOverlayWindow> Create(
      VideoPictureInPictureWindowController* controller) {
    return std::unique_ptr<VideoOverlayWindow>(new TestOverlayWindow());
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
  void SetMicrophoneMuted(bool muted) override {}
  void SetCameraState(bool turned_on) override {}
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override {}
  void SetToggleCameraButtonVisibility(bool is_visible) override {}
  void SetHangUpButtonVisibility(bool is_visible) override {}
  void SetNextSlideButtonVisibility(bool is_visible) override {}
  void SetPreviousSlideButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}

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
  void RequestPlay() override {}
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
  }

  void TearDown() override {
    service_impl_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  PictureInPictureServiceImpl& service() { return *service_impl_; }

  PictureInPictureDelegate& delegate() { return delegate_; }

  mojo::PendingAssociatedRemote<media::mojom::MediaPlayer>
  BindMediaPlayerReceiverAndPassRemote() {
    return media_player_receiver_.BindMediaPlayerReceiverAndPassRemote();
  }

  void ResetMediaPlayerReceiver() { media_player_receiver_.receiver().reset(); }

 private:
  PictureInPictureTestBrowserClient browser_client_;
  PictureInPictureDelegate delegate_;
  // Will be deleted when the frame is destroyed.
  raw_ptr<PictureInPictureServiceImpl> service_impl_;
  // Required to pass a valid PendingRemote to StartSession() in the tests.
  PictureInPictureMediaPlayerReceiver media_player_receiver_;
};

// Flaky on Android. https://crbug.com/970866
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_EnterPictureInPicture DISABLED_EnterPictureInPicture
#else
#define MAYBE_EnterPictureInPicture EnterPictureInPicture
#endif

TEST_F(PictureInPictureServiceImplTest, MAYBE_EnterPictureInPicture) {
  const int kPlayerVideoOnlyId = 30;
  const VideoPictureInPictureWindowControllerImpl* controller =
      VideoPictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
          contents());

  ASSERT_TRUE(controller);

  DummyPictureInPictureSessionObserver observer;
  mojo::Receiver<blink::mojom::PictureInPictureSessionObserver>
      observer_receiver(&observer);
  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  observer_receiver.Bind(observer_remote.InitWithNewPipeAndPassReceiver());

  // If Picture-in-Picture there shouldn't be an active session.
  EXPECT_FALSE(controller->active_session_for_testing());

  viz::SurfaceId surface_id = viz::SurfaceId(
      viz::FrameSinkId(1, 1),
      viz::LocalSurfaceId(
          11, base::UnguessableToken::CreateForTesting(0x111111, 0)));

  EXPECT_CALL(delegate(), EnterPictureInPicture(contents()))
      .WillRepeatedly(testing::Return(PictureInPictureResult::kSuccess));

  mojo::Remote<blink::mojom::PictureInPictureSession> session_remote;
  gfx::Size window_size;

  const gfx::Rect source_bounds(1, 2, 3, 4);
  service().StartSession(
      kPlayerVideoOnlyId, BindMediaPlayerReceiverAndPassRemote(), surface_id,
      gfx::Size(42, 42), true /* show_play_pause_button */,
      std::move(observer_remote), source_bounds,
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<blink::mojom::PictureInPictureSession> remote,
              const gfx::Size& b) {
            if (remote.is_valid())
              session_remote.Bind(std::move(remote));
            window_size = b;
          }));

  EXPECT_TRUE(session_remote);
  EXPECT_EQ(gfx::Size(42, 42), window_size);
  EXPECT_EQ(source_bounds, controller->GetSourceBounds());

  // Picture-in-Picture media player id should not be reset when the media is
  // destroyed (e.g. video stops playing). This allows the Picture-in-Picture
  // window to continue to control the media.
  ResetMediaPlayerReceiver();
  EXPECT_TRUE(controller->active_session_for_testing());
}

TEST_F(PictureInPictureServiceImplTest, EnterPictureInPicture_NotSupported) {
  const int kPlayerVideoOnlyId = 30;
  const VideoPictureInPictureWindowControllerImpl* controller =
      VideoPictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
          contents());

  ASSERT_TRUE(controller);
  EXPECT_FALSE(controller->active_session_for_testing());

  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  viz::SurfaceId surface_id = viz::SurfaceId(
      viz::FrameSinkId(1, 1),
      viz::LocalSurfaceId(
          11, base::UnguessableToken::CreateForTesting(0x111111, 0)));

  EXPECT_CALL(delegate(), EnterPictureInPicture(contents()))
      .WillRepeatedly(testing::Return(PictureInPictureResult::kNotSupported));

  mojo::Remote<blink::mojom::PictureInPictureSession> session_remote;
  gfx::Size window_size;
  const gfx::Rect source_bounds(1, 2, 3, 4);

  service().StartSession(
      kPlayerVideoOnlyId, BindMediaPlayerReceiverAndPassRemote(), surface_id,
      gfx::Size(42, 42), true /* show_play_pause_button */,
      std::move(observer_remote), source_bounds,
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<blink::mojom::PictureInPictureSession> remote,
              const gfx::Size& b) {
            if (remote.is_valid())
              session_remote.Bind(std::move(remote));
            window_size = b;
          }));

  EXPECT_FALSE(controller->active_session_for_testing());

  // The |session_remote| won't be bound because the |remote| received in the
  // StartSessionCallback will be invalid due to PictureInPictureSession not
  // ever being created (meaning the the receiver won't be bound either).
  EXPECT_FALSE(session_remote);
  EXPECT_EQ(gfx::Size(), window_size);
}

}  // namespace content
