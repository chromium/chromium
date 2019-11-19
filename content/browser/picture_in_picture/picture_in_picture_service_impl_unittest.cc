// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"

#include <memory>
#include <utility>

#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class DummyPictureInPictureSessionObserver
    : public blink::mojom::PictureInPictureSessionObserver {
 public:
  DummyPictureInPictureSessionObserver() = default;
  ~DummyPictureInPictureSessionObserver() final = default;

  // Implementation of PictureInPictureSessionObserver.
  void OnWindowSizeChanged(const gfx::Size&) final {}
  void OnStopped() final {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyPictureInPictureSessionObserver);
};

class PictureInPictureDelegate : public WebContentsDelegate {
 public:
  PictureInPictureDelegate() = default;

  MOCK_METHOD3(EnterPictureInPicture,
               PictureInPictureResult(WebContents*,
                                      const viz::SurfaceId&,
                                      const gfx::Size&));

 private:
  DISALLOW_COPY_AND_ASSIGN(PictureInPictureDelegate);
};

class TestOverlayWindow : public OverlayWindow {
 public:
  TestOverlayWindow() = default;
  ~TestOverlayWindow() override {}

  static std::unique_ptr<OverlayWindow> Create(
      PictureInPictureWindowController* controller) {
    return std::unique_ptr<OverlayWindow>(new TestOverlayWindow());
  }

  bool IsActive() override { return false; }
  void Close() override {}
  void ShowInactive() override {}
  void Hide() override {}
  bool IsVisible() override { return false; }
  bool IsAlwaysOnTop() override { return false; }
  gfx::Rect GetBounds() override { return gfx::Rect(size_); }
  void UpdateVideoSize(const gfx::Size& natural_size) override {
    size_ = natural_size;
  }
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetAlwaysHidePlayPauseButton(bool is_visible) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}
  cc::Layer* GetLayerForTesting() override { return nullptr; }

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(TestOverlayWindow);
};

class PictureInPictureTestBrowserClient : public TestContentBrowserClient {
 public:
  PictureInPictureTestBrowserClient() = default;
  ~PictureInPictureTestBrowserClient() override = default;

  std::unique_ptr<OverlayWindow> CreateWindowForPictureInPicture(
      PictureInPictureWindowController* controller) override {
    return TestOverlayWindow::Create(controller);
  }
};

class PictureInPictureServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    SetBrowserClientForTesting(&browser_client_);

    TestRenderFrameHost* render_frame_host = contents()->GetMainFrame();
    render_frame_host->InitializeRenderFrameIfNeeded();

    contents()->SetDelegate(&delegate_);

    mojo::Remote<blink::mojom::PictureInPictureService> service_remote;
    service_impl_ = PictureInPictureServiceImpl::CreateForTesting(
        render_frame_host, service_remote.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    RenderViewHostImplTestHarness::TearDown();
  }

  PictureInPictureServiceImpl& service() { return *service_impl_; }

  PictureInPictureDelegate& delegate() { return delegate_; }

 private:
  PictureInPictureTestBrowserClient browser_client_;
  PictureInPictureDelegate delegate_;
  // Will be deleted when the frame is destroyed.
  PictureInPictureServiceImpl* service_impl_;
};

// Flaky on Android. https://crbug.com/970866
#if defined(OS_ANDROID)
#define MAYBE_EnterPictureInPicture DISABLED_EnterPictureInPicture
#else
#define MAYBE_EnterPictureInPicture EnterPictureInPicture
#endif

TEST_F(PictureInPictureServiceImplTest, MAYBE_EnterPictureInPicture) {
  const int kPlayerVideoOnlyId = 30;

  DummyPictureInPictureSessionObserver observer;
  mojo::Receiver<blink::mojom::PictureInPictureSessionObserver>
      observer_receiver(&observer);
  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  observer_receiver.Bind(observer_remote.InitWithNewPipeAndPassReceiver());

  // If Picture-in-Picture there shouldn't be an active session.
  EXPECT_FALSE(service().active_session_for_testing());

  viz::SurfaceId surface_id =
      viz::SurfaceId(viz::FrameSinkId(1, 1),
                     viz::LocalSurfaceId(
                         11, base::UnguessableToken::Deserialize(0x111111, 0)));

  EXPECT_CALL(delegate(),
              EnterPictureInPicture(contents(), surface_id, gfx::Size(42, 42)))
      .WillRepeatedly(testing::Return(PictureInPictureResult::kSuccess));

  mojo::Remote<blink::mojom::PictureInPictureSession> session_remote;
  gfx::Size window_size;

  service().StartSession(
      kPlayerVideoOnlyId, surface_id, gfx::Size(42, 42),
      true /* show_play_pause_button */, std::move(observer_remote),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<blink::mojom::PictureInPictureSession> remote,
              const gfx::Size& b) {
            if (remote.is_valid())
              session_remote.Bind(std::move(remote));
            window_size = b;
          }));

  EXPECT_TRUE(service().active_session_for_testing());
  EXPECT_TRUE(session_remote);
  EXPECT_EQ(gfx::Size(42, 42), window_size);

  // Picture-in-Picture media player id should not be reset when the media is
  // destroyed (e.g. video stops playing). This allows the Picture-in-Picture
  // window to continue to control the media.
  contents()->GetMainFrame()->OnMessageReceived(
      MediaPlayerDelegateHostMsg_OnMediaDestroyed(
          contents()->GetMainFrame()->GetRoutingID(), kPlayerVideoOnlyId));
  EXPECT_TRUE(service().active_session_for_testing());
}

TEST_F(PictureInPictureServiceImplTest, EnterPictureInPicture_NotSupported) {
  const int kPlayerVideoOnlyId = 30;
  mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>
      observer_remote;
  EXPECT_FALSE(service().active_session_for_testing());

  viz::SurfaceId surface_id =
      viz::SurfaceId(viz::FrameSinkId(1, 1),
                     viz::LocalSurfaceId(
                         11, base::UnguessableToken::Deserialize(0x111111, 0)));

  EXPECT_CALL(delegate(),
              EnterPictureInPicture(contents(), surface_id, gfx::Size(42, 42)))
      .WillRepeatedly(testing::Return(PictureInPictureResult::kNotSupported));

  mojo::Remote<blink::mojom::PictureInPictureSession> session_remote;
  gfx::Size window_size;

  service().StartSession(
      kPlayerVideoOnlyId, surface_id, gfx::Size(42, 42),
      true /* show_play_pause_button */, std::move(observer_remote),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<blink::mojom::PictureInPictureSession> remote,
              const gfx::Size& b) {
            if (remote.is_valid())
              session_remote.Bind(std::move(remote));
            window_size = b;
          }));

  EXPECT_FALSE(service().active_session_for_testing());
  // The |session_remote| won't be bound because the |pending_remote| received
  // in the StartSessionCallback will be invalid due to PictureInPictureSession
  // not ever being created (meaning the the receiver won't be bound either).
  EXPECT_FALSE(session_remote);
  EXPECT_EQ(gfx::Size(), window_size);
}

}  // namespace content
