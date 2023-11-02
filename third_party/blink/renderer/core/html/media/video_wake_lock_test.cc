// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/video_wake_lock.h"

#include <memory>

#include "cc/layers/layer.h"
#include "media/mojo/mojom/media_player.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom-blink.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

// The VideoWakeLockPictureInPictureSession implements a PictureInPicture
// session in the same process as the test and guarantees that the callbacks are
// called in order for the events to be fired.
class VideoWakeLockPictureInPictureSession final
    : public mojom::blink::PictureInPictureSession {
 public:
  explicit VideoWakeLockPictureInPictureSession(
      mojo::PendingReceiver<mojom::blink::PictureInPictureSession> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~VideoWakeLockPictureInPictureSession() override = default;

  void Stop(StopCallback callback) override { std::move(callback).Run(); }
  void Update(uint32_t player_id,
              mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>
                  player_remote,
              const viz::SurfaceId&,
              const gfx::Size&,
              bool show_play_pause_button) override {}

 private:
  mojo::Receiver<mojom::blink::PictureInPictureSession> receiver_;
};

// The VideoWakeLockPictureInPictureService implements the PictureInPicture
// service in the same process as the test and guarantees that the callbacks are
// called in order for the events to be fired.
class VideoWakeLockPictureInPictureService final
    : public mojom::blink::PictureInPictureService {
 public:
  VideoWakeLockPictureInPictureService() : receiver_(this) {}
  ~VideoWakeLockPictureInPictureService() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<mojom::blink::PictureInPictureService>(
        std::move(handle)));
  }

  void StartSession(
      uint32_t,
      mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>,
      const viz::SurfaceId&,
      const gfx::Size&,
      bool,
      mojo::PendingRemote<mojom::blink::PictureInPictureSessionObserver>,
      const gfx::Rect&,
      StartSessionCallback callback) override {
    mojo::PendingRemote<mojom::blink::PictureInPictureSession> session_remote;
    session_ = std::make_unique<VideoWakeLockPictureInPictureSession>(
        session_remote.InitWithNewPipeAndPassReceiver());

    std::move(callback).Run(std::move(session_remote), gfx::Size());
  }

 private:
  mojo::Receiver<mojom::blink::PictureInPictureService> receiver_;
  std::unique_ptr<VideoWakeLockPictureInPictureSession> session_;
};

class VideoWakeLockMediaPlayer final : public EmptyWebMediaPlayer {
 public:
  ReadyState GetReadyState() const override { return kReadyStateHaveMetadata; }
  void OnRequestPictureInPicture() override {
    // Use a fake but valid viz::SurfaceId.
    surface_id_ = viz::SurfaceId(
        viz::FrameSinkId(1, 1),
        viz::LocalSurfaceId(11,
                            base::UnguessableToken::Deserialize(0x111111, 0)));
  }
  absl::optional<viz::SurfaceId> GetSurfaceId() override { return surface_id_; }

  bool HasVideo() const override { return has_video_; }
  void SetHasVideo(bool has_video) { has_video_ = has_video; }

 private:
  bool has_video_ = true;
  absl::optional<viz::SurfaceId> surface_id_;
};

class VideoWakeLockFrameClient : public test::MediaStubLocalFrameClient {
 public:
  explicit VideoWakeLockFrameClient(std::unique_ptr<WebMediaPlayer> player)
      : test::MediaStubLocalFrameClient(std::move(player)) {}
  VideoWakeLockFrameClient(const VideoWakeLockFrameClient&) = delete;
  VideoWakeLockFrameClient& operator=(const VideoWakeLockFrameClient&) = delete;
};

class VideoWakeLockTestWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  explicit VideoWakeLockTestWebFrameClient(
      std::unique_ptr<WebMediaPlayer> web_media_player)
      : web_media_player_(std::move(web_media_player)) {}

  WebMediaPlayer* CreateMediaPlayer(
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      blink::MediaInspectorContext*,
      WebMediaPlayerEncryptedMediaClient*,
      WebContentDecryptionModule*,
      const WebString& sink_id,
      const cc::LayerTreeSettings& settings,
      scoped_refptr<base::TaskRunner> compositor_worker_task_runner) override {
    return web_media_player_.release();
  }

 private:
  std::unique_ptr<WebMediaPlayer> web_media_player_;
};

class VideoWakeLockTest : public testing::Test {
 public:
  void SetUp() override {
    auto media_player = std::make_unique<VideoWakeLockMediaPlayer>();
    media_player_ = media_player.get();
    client_ = std::make_unique<VideoWakeLockTestWebFrameClient>(
        std::move(media_player));

    helper_.Initialize(client_.get());
    helper_.Resize(gfx::Size(800, 600));

    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PictureInPictureService::Name_,
        WTF::BindRepeating(&VideoWakeLockPictureInPictureService::Bind,
                           WTF::Unretained(&pip_service_)));

    fake_layer_ = cc::Layer::Create();

    GetDocument().body()->setInnerHTML("<body><video></video></body>");
    video_ = To<HTMLVideoElement>(GetDocument().QuerySelector("video"));
    SetFakeCcLayer(fake_layer_.get());
    video_->SetReadyState(HTMLMediaElement::ReadyState::kHaveMetadata);
    video_wake_lock_ = MakeGarbageCollected<VideoWakeLock>(*video_.Get());
    video_->SetSrc("http://example.com/foo.mp4");
    test::RunPendingTasks();

    GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                                 true);
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PictureInPictureService::Name_, {});
  }

  HTMLVideoElement* Video() const { return video_.Get(); }
  VideoWakeLock* GetVideoWakeLock() const { return video_wake_lock_.Get(); }
  VideoWakeLockMediaPlayer* GetMediaPlayer() const { return media_player_; }

  LocalFrame& GetFrame() const { return *helper_.LocalMainFrame()->GetFrame(); }
  Page& GetPage() const { return *GetDocument().GetPage(); }

  void UpdateAllLifecyclePhasesForTest() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  Document& GetDocument() const { return *GetFrame().GetDocument(); }

  void SetFakeCcLayer(cc::Layer* layer) { video_->SetCcLayer(layer); }

  void SimulatePlaying() {
    video_wake_lock_->Invoke(GetFrame().DomWindow(),
                             Event::Create(event_type_names::kPlaying));
  }

  void SimulatePause() {
    video_wake_lock_->Invoke(GetFrame().DomWindow(),
                             Event::Create(event_type_names::kPause));
  }

  void SimulateEnterPictureInPicture() {
    PictureInPictureController::From(GetDocument())
        .EnterPictureInPicture(Video(), /*promise=*/nullptr);

    MakeGarbageCollected<WaitForEvent>(
        video_.Get(), event_type_names::kEnterpictureinpicture);
  }

  void SimulateLeavePictureInPicture() {
    PictureInPictureController::From(GetDocument())
        .ExitPictureInPicture(Video(), nullptr);

    MakeGarbageCollected<WaitForEvent>(
        video_.Get(), event_type_names::kLeavepictureinpicture);
  }

  void SimulateContextPause() {
    GetFrame().DomWindow()->SetLifecycleState(
        mojom::FrameLifecycleState::kPaused);
  }

  void SimulateContextRunning() {
    GetFrame().DomWindow()->SetLifecycleState(
        mojom::FrameLifecycleState::kRunning);
  }

  void SimulateContextDestroyed() { GetFrame().DomWindow()->FrameDestroyed(); }

  void SimulateNetworkState(HTMLMediaElement::NetworkState network_state) {
    video_->SetNetworkState(network_state);
  }

  void UpdateVisibilityObserver() {
    UpdateAllLifecyclePhasesForTest();
    test::RunPendingTasks();
  }

  void HideVideo() {
    video_->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
  }

  void ShowVideo() {
    video_->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kBlock);
  }

 private:
  std::unique_ptr<frame_test_helpers::TestWebFrameClient> client_;
  Persistent<HTMLVideoElement> video_;
  Persistent<VideoWakeLock> video_wake_lock_;

  VideoWakeLockMediaPlayer* media_player_;
  scoped_refptr<cc::Layer> fake_layer_;

  VideoWakeLockPictureInPictureService pip_service_;
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(VideoWakeLockTest, NoLockByDefault) {
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PlayingVideoRequestsLock) {
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PausingVideoCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  SimulatePause();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, HiddingPageCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PlayingWhileHiddenDoesNotRequestLock) {
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  SimulatePlaying();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, ShowingPageRequestsLock) {
  SimulatePlaying();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               false);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, ShowingPageDoNotRequestsLockIfPaused) {
  SimulatePlaying();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  SimulatePause();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, RemotePlaybackDisconnectedDoesNotCancelLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CLOSED);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, RemotePlaybackConnectingDoesNotCancelLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTING);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, ActiveRemotePlaybackCancelsLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CLOSED);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTED);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, LeavingRemotePlaybackResumesLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTED);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CLOSED);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PictureInPictureLocksWhenPageNotVisible) {
  SimulatePlaying();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  SimulateEnterPictureInPicture();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PictureInPictureDoesNoLockWhenPaused) {
  SimulatePlaying();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  SimulatePause();
  SimulateEnterPictureInPicture();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, LeavingPictureInPictureCancelsLock) {
  SimulatePlaying();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  SimulateEnterPictureInPicture();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  SimulateLeavePictureInPicture();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, RemotingVideoInPictureInPictureDoesNotRequestLock) {
  SimulatePlaying();
  SimulateEnterPictureInPicture();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTED);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PausingContextCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  SimulateContextPause();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, ResumingContextResumesLock) {
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  SimulateContextPause();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  SimulateContextRunning();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, DestroyingContextCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  SimulateContextDestroyed();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, LoadingCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  // The network state has to be non-empty for the resetting to actually kick.
  SimulateNetworkState(HTMLMediaElement::kNetworkIdle);

  Video()->SetSrc("");
  test::RunPendingTasks();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, MutedHiddenVideoDoesNotTakeLock) {
  Video()->setMuted(true);
  HideVideo();
  UpdateVisibilityObserver();

  SimulatePlaying();

  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, AudibleHiddenVideoTakesLock) {
  Video()->setMuted(false);
  HideVideo();
  UpdateVisibilityObserver();

  SimulatePlaying();

  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, UnmutingHiddenVideoTakesLock) {
  Video()->setMuted(true);
  HideVideo();
  UpdateVisibilityObserver();

  SimulatePlaying();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  Video()->setMuted(false);
  test::RunPendingTasks();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, MutingHiddenVideoReleasesLock) {
  Video()->setMuted(false);
  HideVideo();
  UpdateVisibilityObserver();

  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  Video()->setMuted(true);
  test::RunPendingTasks();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, HidingAudibleVideoDoesNotReleaseLock) {
  Video()->setMuted(false);
  ShowVideo();
  UpdateVisibilityObserver();
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  HideVideo();
  UpdateVisibilityObserver();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, HidingMutedVideoReleasesLock) {
  Video()->setMuted(true);
  ShowVideo();
  UpdateVisibilityObserver();
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  HideVideo();
  UpdateVisibilityObserver();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, HiddenMutedVideoAlwaysVisibleInPictureInPicture) {
  Video()->setMuted(true);
  HideVideo();
  UpdateVisibilityObserver();
  SimulatePlaying();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  SimulateEnterPictureInPicture();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  SimulateLeavePictureInPicture();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, VideoWithNoFramesReleasesLock) {
  GetMediaPlayer()->SetHasVideo(false);
  SimulatePlaying();

  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, VideoWithFramesTakesLock) {
  GetMediaPlayer()->SetHasVideo(true);
  SimulatePlaying();

  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

}  // namespace blink
