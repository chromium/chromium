// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/video_wake_lock.h"

#include <memory>
#include <utility>

#include "cc/layers/layer.h"
#include "media/mojo/mojom/media_player.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

constexpr gfx::Size kWindowSize(800, 600);
constexpr gfx::Size kNormalVideoSize(640, 480);
constexpr gfx::Size kSmallVideoSize(320, 240);  // Too small to take wake lock.

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
        viz::LocalSurfaceId(
            11, base::UnguessableToken::CreateForTesting(0x111111, 0)));
  }
  std::optional<viz::SurfaceId> GetSurfaceId() override { return surface_id_; }

  bool HasAudio() const override { return has_audio_; }
  void SetHasAudio(bool has_audio) { has_audio_ = has_audio; }
  bool HasVideo() const override { return has_video_; }
  void SetHasVideo(bool has_video) { has_video_ = has_video; }

  gfx::Size NaturalSize() const override { return size_; }
  gfx::Size VisibleSize() const override { return size_; }
  void SetSize(const gfx::Size& size) { size_ = size; }

 private:
  bool has_audio_ = true;
  bool has_video_ = true;
  gfx::Size size_ = kNormalVideoSize;
  std::optional<viz::SurfaceId> surface_id_;
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

  std::unique_ptr<WebMediaPlayer> CreateMediaPlayer(
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client,
      blink::MediaInspectorContext*,
      WebMediaPlayerEncryptedMediaClient*,
      WebContentDecryptionModule*,
      const WebString& sink_id,
      const cc::LayerTreeSettings* settings,
      scoped_refptr<base::TaskRunner> compositor_worker_task_runner) override {
    web_media_player_client_ = client;
    return std::move(web_media_player_);
  }

  WebMediaPlayerClient* web_media_player_client() const {
    return web_media_player_client_;
  }

  void SetWebMediaPlayer(std::unique_ptr<WebMediaPlayer> web_media_player) {
    web_media_player_ = std::move(web_media_player);
  }

 private:
  WebMediaPlayerClient* web_media_player_client_ = nullptr;
  std::unique_ptr<WebMediaPlayer> web_media_player_;
};

class VideoWakeLockTest : public testing::Test,
                          public testing::WithParamInterface<bool>,
                          private ScopedIntersectionOptimizationForTest {
 public:
  VideoWakeLockTest() : ScopedIntersectionOptimizationForTest(GetParam()) {}

  void SetUp() override {
    auto media_player = std::make_unique<VideoWakeLockMediaPlayer>();
    media_player_ = media_player.get();
    client_ = std::make_unique<VideoWakeLockTestWebFrameClient>(
        std::move(media_player));

    helper_.Initialize(client_.get());
    helper_.Resize(kWindowSize);

    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PictureInPictureService::Name_,
        WTF::BindRepeating(&VideoWakeLockPictureInPictureService::Bind,
                           WTF::Unretained(&pip_service_)));

    fake_layer_ = cc::Layer::Create();

    GetDocument().body()->setInnerHTML(
        "<body><div></div><video></video></body>");
    video_ = To<HTMLVideoElement>(
        GetDocument().QuerySelector(AtomicString("video")));
    div_ = To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
    SetFakeCcLayer(fake_layer_.get());
    video_->SetReadyState(HTMLMediaElement::ReadyState::kHaveMetadata);
    video_wake_lock_ = MakeGarbageCollected<VideoWakeLock>(*video_.Get());
    video_->SetSrc(AtomicString("http://example.com/foo.mp4"));
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
  WebMediaPlayerClient* GetMediaPlayerClient() const {
    return client_->web_media_player_client();
  }

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

  void ProcessEvents() { test::RunPendingTasks(); }

  void UpdateObservers() {
    UpdateAllLifecyclePhasesForTest();
    test::RunPendingTasks();
  }

  void HideVideo() {
    video_->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
  }

  void ShowVideo() {
    video_->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kBlock);
  }

  bool HasWakeLock() { return GetVideoWakeLock()->active_for_tests(); }

  void SetDivHeight(double height_in_pixels) {
    div_->SetInlineStyleProperty(CSSPropertyID::kHeight, height_in_pixels,
                                 CSSPrimitiveValue::UnitType::kPixels);
    div_->SetInlineStyleProperty(CSSPropertyID::kWidth, kWindowSize.width(),
                                 CSSPrimitiveValue::UnitType::kPixels);
  }

  HTMLDivElement* div() { return div_; }
  HTMLVideoElement* video() { return video_; }

  void RecreateWebMediaPlayer() {
    auto media_player = std::make_unique<VideoWakeLockMediaPlayer>();
    media_player_ = media_player.get();
    client_->SetWebMediaPlayer(std::move(media_player));
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoWakeLockTestWebFrameClient> client_;
  Persistent<HTMLDivElement> div_;
  Persistent<HTMLVideoElement> video_;
  Persistent<VideoWakeLock> video_wake_lock_;

  VideoWakeLockMediaPlayer* media_player_ = nullptr;
  scoped_refptr<cc::Layer> fake_layer_;

  VideoWakeLockPictureInPictureService pip_service_;
  frame_test_helpers::WebViewHelper helper_;
};

INSTANTIATE_TEST_SUITE_P(All, VideoWakeLockTest, testing::Bool());

TEST_P(VideoWakeLockTest, NoLockByDefault) {
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, PlayingVideoRequestsLock) {
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, PausingVideoCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  SimulatePause();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, HiddingPageCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, PlayingWhileHiddenDoesNotRequestLock) {
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  SimulatePlaying();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, ShowingPageRequestsLock) {
  SimulatePlaying();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  EXPECT_FALSE(HasWakeLock());

  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               false);
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, ShowingPageDoNotRequestsLockIfPaused) {
  SimulatePlaying();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  EXPECT_FALSE(HasWakeLock());

  SimulatePause();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               false);
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, RemotePlaybackDisconnectedDoesNotCancelLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CLOSED);
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, RemotePlaybackConnectingDoesNotCancelLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTING);
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, ActiveRemotePlaybackCancelsLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CLOSED);
  EXPECT_TRUE(HasWakeLock());

  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTED);
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, LeavingRemotePlaybackResumesLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTED);
  EXPECT_FALSE(HasWakeLock());

  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CLOSED);
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, PictureInPictureLocksWhenPageNotVisible) {
  SimulatePlaying();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  EXPECT_FALSE(HasWakeLock());

  SimulateEnterPictureInPicture();
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, PictureInPictureDoesNoLockWhenPaused) {
  SimulatePlaying();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  EXPECT_FALSE(HasWakeLock());

  SimulatePause();
  SimulateEnterPictureInPicture();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, LeavingPictureInPictureCancelsLock) {
  SimulatePlaying();
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               false);
  SimulateEnterPictureInPicture();
  EXPECT_TRUE(HasWakeLock());

  SimulateLeavePictureInPicture();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, RemotingVideoInPictureInPictureDoesNotRequestLock) {
  SimulatePlaying();
  SimulateEnterPictureInPicture();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTED);
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, PausingContextCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  SimulateContextPause();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, ResumingContextResumesLock) {
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  SimulateContextPause();
  EXPECT_FALSE(HasWakeLock());

  SimulateContextRunning();
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, DestroyingContextCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  SimulateContextDestroyed();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, LoadingCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  // The network state has to be non-empty for the resetting to actually kick.
  SimulateNetworkState(HTMLMediaElement::kNetworkIdle);

  Video()->SetSrc(g_empty_atom);
  test::RunPendingTasks();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, MutedHiddenVideoDoesNotTakeLock) {
  Video()->setMuted(true);
  HideVideo();
  UpdateObservers();

  SimulatePlaying();

  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, AudibleHiddenVideoTakesLock) {
  Video()->setMuted(false);
  HideVideo();
  UpdateObservers();

  SimulatePlaying();

  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, UnmutingHiddenVideoTakesLock) {
  Video()->setMuted(true);
  HideVideo();
  UpdateObservers();

  SimulatePlaying();
  EXPECT_FALSE(HasWakeLock());

  Video()->setMuted(false);
  test::RunPendingTasks();
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, MutingHiddenVideoReleasesLock) {
  Video()->setMuted(false);
  HideVideo();
  UpdateObservers();

  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  Video()->setMuted(true);
  test::RunPendingTasks();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, HidingAudibleVideoDoesNotReleaseLock) {
  Video()->setMuted(false);
  ShowVideo();
  UpdateObservers();
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  HideVideo();
  UpdateObservers();
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, HidingMutedVideoReleasesLock) {
  Video()->setMuted(true);
  ShowVideo();
  UpdateObservers();
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  HideVideo();
  UpdateObservers();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, HiddenMutedVideoAlwaysVisibleInPictureInPicture) {
  Video()->setMuted(true);
  HideVideo();
  UpdateObservers();
  SimulatePlaying();
  EXPECT_FALSE(HasWakeLock());

  SimulateEnterPictureInPicture();
  EXPECT_TRUE(HasWakeLock());

  SimulateLeavePictureInPicture();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, VideoWithNoFramesReleasesLock) {
  GetMediaPlayer()->SetHasVideo(false);
  SimulatePlaying();

  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, VideoWithFramesTakesLock) {
  GetMediaPlayer()->SetHasVideo(true);
  SimulatePlaying();

  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, HidingVideoOnlyReleasesLock) {
  GetMediaPlayer()->SetHasAudio(false);
  ShowVideo();
  UpdateObservers();
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  HideVideo();
  UpdateObservers();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, SmallMutedVideoDoesNotTakeLock) {
  ASSERT_LT(
      kSmallVideoSize.Area64() / static_cast<double>(kWindowSize.Area64()),
      GetVideoWakeLock()->GetSizeThresholdForTests());
  ASSERT_GT(
      kNormalVideoSize.Area64() / static_cast<double>(kWindowSize.Area64()),
      GetVideoWakeLock()->GetSizeThresholdForTests());

  // Set player to take less than 20% of the page and mute it.
  GetMediaPlayer()->SetSize(kSmallVideoSize);
  Video()->setMuted(true);

  ShowVideo();
  UpdateObservers();
  SimulatePlaying();
  EXPECT_FALSE(HasWakeLock());

  // Unmuting the video should take the lock.
  Video()->setMuted(false);
  ProcessEvents();
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, SizeChangeTakesLock) {
  // Set player to take less than 20% of the page and mute it.
  GetMediaPlayer()->SetSize(kSmallVideoSize);
  Video()->setMuted(true);

  ShowVideo();
  UpdateObservers();
  SimulatePlaying();
  EXPECT_FALSE(HasWakeLock());

  // Resizing the video should take the lock.
  GetMediaPlayer()->SetSize(kNormalVideoSize);
  GetMediaPlayerClient()->SizeChanged();
  UpdateObservers();
  EXPECT_TRUE(HasWakeLock());

  // Getting too small should release the lock.
  GetMediaPlayer()->SetSize(kSmallVideoSize);
  GetMediaPlayerClient()->SizeChanged();
  UpdateObservers();
  EXPECT_FALSE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, MutedVideoTooFarOffscreenDoesNotTakeLock) {
  Video()->setMuted(true);

  // Move enough of the video off screen to not take the lock.
  const auto kThreshold = GetVideoWakeLock()->visibility_threshold_for_tests();
  SetDivHeight(kWindowSize.height() - kNormalVideoSize.height() +
               (1 - kThreshold) * kNormalVideoSize.height());

  ShowVideo();
  UpdateObservers();
  SimulatePlaying();
  EXPECT_FALSE(HasWakeLock());

  // Scroll video far enough back on screen.
  SetDivHeight(kWindowSize.height() - kNormalVideoSize.height() +
               (1 - kThreshold * 1.10) * kNormalVideoSize.height());
  UpdateObservers();
  EXPECT_TRUE(HasWakeLock());
}

TEST_P(VideoWakeLockTest, WakeLockTracksDocumentsPage) {
  // Create a document that has no Page.
  auto* another_document = Document::Create(GetDocument());
  ASSERT_FALSE(another_document->GetPage());

  // Move the video there, and notify our wake lock.
  another_document->AppendChild(video());
  GetVideoWakeLock()->ElementDidMoveToNewDocument();
  EXPECT_FALSE(GetVideoWakeLock()->GetPage());

  // Move the video back to the main page and verify that the wake lock notices.
  div()->AppendChild(video());
  GetVideoWakeLock()->ElementDidMoveToNewDocument();
  EXPECT_EQ(GetVideoWakeLock()->GetPage(), video()->GetDocument().GetPage());
}

TEST_P(VideoWakeLockTest, VideoOnlyMediaStreamAlwaysTakesLock) {
  // Default player is consumed on the first src=file load, so we must provide a
  // new one for the MediaStream load below.
  RecreateWebMediaPlayer();

  // The "with audio" case is the same as the src=file case, so we only test the
  // video only MediaStream case here.
  GetMediaPlayer()->SetHasAudio(false);

  MediaStreamComponentVector dummy_components;
  auto* descriptor = MakeGarbageCollected<MediaStreamDescriptor>(
      dummy_components, dummy_components);
  Video()->SetSrcObjectVariant(descriptor);
  test::RunPendingTasks();

  ASSERT_EQ(Video()->GetLoadType(), WebMediaPlayer::kLoadTypeMediaStream);
  EXPECT_FALSE(HasWakeLock());

  GetMediaPlayer()->SetSize(kNormalVideoSize);
  ShowVideo();
  UpdateObservers();
  SimulatePlaying();
  EXPECT_TRUE(HasWakeLock());

  // Set player to take less than 20% of the page and ensure wake lock is held.
  GetMediaPlayer()->SetSize(kSmallVideoSize);
  GetMediaPlayerClient()->SizeChanged();
  UpdateObservers();
  EXPECT_TRUE(HasWakeLock());

  // Ensure normal wake lock release.
  SimulatePause();
  EXPECT_FALSE(HasWakeLock());
}

}  // namespace blink
