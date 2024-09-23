// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/mediastream/web_media_player_ms.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/layers/layer.h"
#include "media/base/media_content_type.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "media/video/mock_gpu_memory_buffer_video_frame_pool.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "third_party/blink/public/common/media/display_type.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_renderer.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_renderer_factory.h"
#include "third_party/blink/renderer/modules/mediastream/web_media_player_ms_compositor.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace blink {

enum class FrameType {
  NORMAL_FRAME = 0,
  BROKEN_FRAME = -1,
  TEST_BRAKE = -2,  // Signal to pause message loop.
  MIN_TYPE = TEST_BRAKE
};

class MockSurfaceLayerBridge : public WebSurfaceLayerBridge {
 public:
  MockSurfaceLayerBridge() {
    ON_CALL(*this, GetSurfaceId).WillByDefault(ReturnRef(surface_id_));
  }

  MOCK_CONST_METHOD0(GetCcLayer, cc::Layer*());
  MOCK_CONST_METHOD0(GetFrameSinkId, const viz::FrameSinkId&());
  MOCK_CONST_METHOD0(GetSurfaceId, const viz::SurfaceId&());
  MOCK_METHOD1(SetContentsOpaque, void(bool));
  MOCK_METHOD0(CreateSurfaceLayer, void());
  MOCK_METHOD0(ClearSurfaceId, void());
  MOCK_METHOD0(ClearObserver, void());
  MOCK_METHOD0(RegisterFrameSinkHierarchy, void());
  MOCK_METHOD0(UnregisterFrameSinkHierarchy, void());

  viz::FrameSinkId frame_sink_id_ = viz::FrameSinkId(1, 1);
  viz::LocalSurfaceId local_surface_id_ = viz::LocalSurfaceId(
      11,
      base::UnguessableToken::CreateForTesting(0x111111, 0));
  viz::SurfaceId surface_id_ =
      viz::SurfaceId(frame_sink_id_, local_surface_id_);
};

using TestFrame = std::pair<FrameType, scoped_refptr<media::VideoFrame>>;

static const int kOddSizeOffset = 3;
static const int kStandardWidth = 320;
static const int kStandardHeight = 240;

class FakeWebMediaPlayerDelegate : public WebMediaPlayerDelegate {
 public:
  FakeWebMediaPlayerDelegate() {}

  FakeWebMediaPlayerDelegate(const FakeWebMediaPlayerDelegate&) = delete;
  FakeWebMediaPlayerDelegate& operator=(const FakeWebMediaPlayerDelegate&) =
      delete;

  ~FakeWebMediaPlayerDelegate() override {
    DCHECK(!observer_);
    DCHECK(is_gone_);
  }

  int AddObserver(Observer* observer) override {
    observer_ = observer;
    return delegate_id_;
  }

  void RemoveObserver(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    observer_ = nullptr;
  }

  void DidMediaMetadataChange(int delegate_id,
                              bool has_audio,
                              bool has_video,
                              media::MediaContentType type) override {
    EXPECT_EQ(delegate_id_, delegate_id);
  }

  void DidPlay(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    is_gone_ = false;
  }

  void DidPause(int delegate_id, bool reached_end_of_stream) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    EXPECT_FALSE(reached_end_of_stream);
    EXPECT_FALSE(is_gone_);
  }

  void PlayerGone(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    is_gone_ = true;
  }

  void SetIdle(int delegate_id, bool is_idle) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    is_idle_ = is_idle;
  }

  bool IsIdle(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    return is_idle_;
  }

  void ClearStaleFlag(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
  }

  bool IsStale(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    return false;
  }

  bool IsPageHidden() override { return is_page_hidden_; }

  bool IsFrameHidden() override { return false; }

  void set_page_hidden(bool is_page_hidden) {
    is_page_hidden_ = is_page_hidden;
  }

  int delegate_id() { return delegate_id_; }

 private:
  int delegate_id_ = 1234;
  raw_ptr<Observer> observer_ = nullptr;
  bool is_page_hidden_ = false;
  bool is_gone_ = true;
  bool is_idle_ = false;
};

class ReusableMessageLoopEvent {
 public:
  ReusableMessageLoopEvent() : event_(new media::WaitableMessageLoopEvent()) {}

  base::OnceClosure GetClosure() const { return event_->GetClosure(); }

  media::PipelineStatusCallback GetPipelineStatusCB() const {
    return event_->GetPipelineStatusCB();
  }

  void RunAndWait() {
    event_->RunAndWait();
    event_ = std::make_unique<media::WaitableMessageLoopEvent>();
  }

  void RunAndWaitForStatus(media::PipelineStatus expected) {
    event_->RunAndWaitForStatus(expected);
    event_ = std::make_unique<media::WaitableMessageLoopEvent>();
  }

 private:
  std::unique_ptr<media::WaitableMessageLoopEvent> event_;
};

// The class is used mainly to inject VideoFrames into WebMediaPlayerMS.
class MockMediaStreamVideoRenderer : public MediaStreamVideoRenderer {
 public:
  MockMediaStreamVideoRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ReusableMessageLoopEvent* message_loop_controller,
      const MediaStreamVideoRenderer::RepaintCB& repaint_cb,
      raw_ptr<base::test::TaskEnvironment> task_environment)
      : started_(false),
        standard_size_(kStandardWidth, kStandardHeight),
        task_runner_(task_runner),
        message_loop_controller_(message_loop_controller),
        repaint_cb_(repaint_cb),
        delay_till_next_generated_frame_(base::Seconds(1.0 / 30.0)),
        task_environment_(task_environment) {}

  // Implementation of MediaStreamVideoRenderer
  void Start() override;
  void Stop() override;
  void Resume() override;
  void Pause() override;

  // Methods for test use
  void QueueFrames(const Vector<int>& timestamps_or_frame_type,
                   bool opaque_frame = true,
                   bool odd_size_frame = false,
                   int double_size_index = -1,
                   media::VideoRotation rotation = media::VIDEO_ROTATION_0);
  bool Started() { return started_; }
  bool Paused() { return paused_; }

  void set_standard_size(const gfx::Size& size) { standard_size_ = size; }
  const gfx::Size& get_standard_size() { return standard_size_; }

  // Main function that pushes a frame into WebMediaPlayerMS
  void InjectFrame();

 private:
  ~MockMediaStreamVideoRenderer() override = default;

  // Methods for test use
  void AddFrame(FrameType category, scoped_refptr<media::VideoFrame> frame);

  bool started_;
  bool paused_;
  gfx::Size standard_size_;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const raw_ptr<ReusableMessageLoopEvent, DanglingUntriaged>
      message_loop_controller_;
  const MediaStreamVideoRenderer::RepaintCB repaint_cb_;

  base::circular_deque<TestFrame> frames_;
  base::TimeDelta delay_till_next_generated_frame_;
  // Used for computing the display time for frames.
  raw_ptr<base::test::TaskEnvironment> task_environment_;
};

class MockMediaStreamAudioRenderer : public MediaStreamAudioRenderer {
 public:
  MockMediaStreamAudioRenderer() {}

  void Start() override {}
  void Stop() override {}
  void Play() override {}
  void Pause() override {}
  void SetVolume(float volume) override {}

  void SwitchOutputDevice(const std::string& device_id,
                          media::OutputDeviceStatusCB callback) override {}
  base::TimeDelta GetCurrentRenderTime() override { return base::TimeDelta(); }

 protected:
  ~MockMediaStreamAudioRenderer() override {}
};

void MockMediaStreamVideoRenderer::Start() {
  started_ = true;
  paused_ = false;
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&MockMediaStreamVideoRenderer::InjectFrame,
                          WrapRefCounted(this)));
}

void MockMediaStreamVideoRenderer::Stop() {
  started_ = false;
  frames_.clear();
}

void MockMediaStreamVideoRenderer::Resume() {
  CHECK(started_);
  paused_ = false;
}

void MockMediaStreamVideoRenderer::Pause() {
  CHECK(started_);
  paused_ = true;
}

void MockMediaStreamVideoRenderer::AddFrame(
    FrameType category,
    scoped_refptr<media::VideoFrame> frame) {
  frames_.push_back(std::make_pair(category, std::move(frame)));
}

void MockMediaStreamVideoRenderer::QueueFrames(
    const Vector<int>& timestamp_or_frame_type,
    bool opaque_frame,
    bool odd_size_frame,
    int double_size_index,
    media::VideoRotation rotation) {
  gfx::Size standard_size = standard_size_;
  // Advance the tick clock by 100 milliseconds at the start of QueueFrames.
  task_environment_->AdvanceClock(base::Milliseconds(100));
  for (wtf_size_t i = 0; i < timestamp_or_frame_type.size(); i++) {
    // Advance the tick clock by 10 milliseconds for each frame.
    task_environment_->AdvanceClock(base::Milliseconds(10));
    const int token = timestamp_or_frame_type[i];
    if (static_cast<int>(i) == double_size_index) {
      standard_size =
          gfx::Size(standard_size_.width() * 2, standard_size_.height() * 2);
    }
    if (token < static_cast<int>(FrameType::MIN_TYPE)) {
      CHECK(false) << "Unrecognized frame type: " << token;
      return;
    }

    if (token < 0) {
      AddFrame(static_cast<FrameType>(token), nullptr);
      continue;
    }

    if (token >= 0) {
      gfx::Size frame_size;
      if (odd_size_frame) {
        frame_size.SetSize(standard_size.width() - kOddSizeOffset,
                           standard_size.height() - kOddSizeOffset);
      } else {
        frame_size.SetSize(standard_size.width(), standard_size.height());
      }

      auto frame = media::VideoFrame::CreateZeroInitializedFrame(
          opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
          frame_size, gfx::Rect(frame_size), frame_size,
          base::Milliseconds(token));

      // MediaStreamRemoteVideoSource does not explicitly set the rotation
      // for unrotated frames, so that is not done here either.
      if (rotation != media::VIDEO_ROTATION_0)
        frame->metadata().transformation = rotation;

      frame->metadata().reference_time =
          base::TimeTicks::Now() + base::Milliseconds(token);

      AddFrame(FrameType::NORMAL_FRAME, frame);
      continue;
    }
  }
}

void MockMediaStreamVideoRenderer::InjectFrame() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!started_)
    return;

  if (frames_.empty()) {
    message_loop_controller_->GetClosure().Run();
    return;
  }

  auto frame = frames_.front();
  frames_.pop_front();

  if (frame.first == FrameType::BROKEN_FRAME)
    return;

  // For pause case, the provider will still let the stream continue, but
  // not send the frames to the player. As is the same case in reality.
  if (frame.first == FrameType::NORMAL_FRAME) {
    if (!paused_)
      repaint_cb_.Run(frame.second);

    for (size_t i = 0; i < frames_.size(); ++i) {
      if (frames_[i].first == FrameType::NORMAL_FRAME) {
        delay_till_next_generated_frame_ =
            (frames_[i].second->timestamp() - frame.second->timestamp()) /
            (i + 1);
        break;
      }
    }
  }

  PostDelayedCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&MockMediaStreamVideoRenderer::InjectFrame,
                          WrapRefCounted(this)),
      delay_till_next_generated_frame_);

  // This will pause the |message_loop_|, and the purpose is to allow the main
  // test function to do some operations (e.g. call pause(), switch to
  // background rendering, etc) on WebMediaPlayerMS before resuming
  // |message_loop_|.
  if (frame.first == FrameType::TEST_BRAKE)
    message_loop_controller_->GetClosure().Run();
}

class MockWebVideoFrameSubmitter : public WebVideoFrameSubmitter {
 public:
  // WebVideoFrameSubmitter implementation.
  MOCK_METHOD0(StopUsingProvider, void());
  MOCK_METHOD0(DidReceiveFrame, void());
  MOCK_METHOD1(EnableSubmission, void(viz::SurfaceId));
  MOCK_METHOD0(StartRendering, void());
  MOCK_METHOD0(StopRendering, void());
  MOCK_METHOD1(MockInitialize, void(cc::VideoFrameProvider*));
  MOCK_METHOD1(SetTransform, void(media::VideoTransformation));
  MOCK_METHOD1(SetIsSurfaceVisible, void(bool));
  MOCK_METHOD1(SetIsPageVisible, void(bool));
  MOCK_METHOD1(SetForceSubmit, void(bool));
  MOCK_METHOD1(SetForceBeginFrames, void(bool));
  MOCK_CONST_METHOD0(IsDrivingFrameUpdates, bool());

  void Initialize(cc::VideoFrameProvider* provider,
                  bool is_media_stream) override {
    provider_ = provider;
    MockInitialize(provider);
  }

 private:
  raw_ptr<cc::VideoFrameProvider> provider_;
};

// The class is used to generate a MockVideoProvider in
// WebMediaPlayerMS::load().
class MockRenderFactory : public MediaStreamRendererFactory {
 public:
  MockRenderFactory(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      ReusableMessageLoopEvent* message_loop_controller,
      raw_ptr<base::test::TaskEnvironment> task_environment)
      : task_runner_(task_runner),
        message_loop_controller_(message_loop_controller),
        task_environment_(task_environment) {}

  scoped_refptr<MediaStreamVideoRenderer> GetVideoRenderer(
      const WebMediaStream& web_stream,
      const MediaStreamVideoRenderer::RepaintCB& repaint_cb,
      scoped_refptr<base::SequencedTaskRunner> video_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner)
      override;

  MockMediaStreamVideoRenderer* provider() {
    return static_cast<MockMediaStreamVideoRenderer*>(provider_.get());
  }

  scoped_refptr<MediaStreamAudioRenderer> GetAudioRenderer(
      const WebMediaStream& web_stream,
      WebLocalFrame* web_frame,
      const WebString& device_id,
      base::RepeatingCallback<void()> on_render_error_callback) override {
    return audio_renderer_;
  }

  void set_audio_renderer(scoped_refptr<MediaStreamAudioRenderer> renderer) {
    audio_renderer_ = std::move(renderer);
  }

  void set_support_video_renderer(bool support) {
    DCHECK(!provider_);
    support_video_renderer_ = support;
  }

  bool support_video_renderer() const { return support_video_renderer_; }

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<MediaStreamVideoRenderer> provider_;
  const raw_ptr<ReusableMessageLoopEvent> message_loop_controller_;
  bool support_video_renderer_ = true;
  scoped_refptr<MediaStreamAudioRenderer> audio_renderer_;
  raw_ptr<base::test::TaskEnvironment> task_environment_;
};

scoped_refptr<MediaStreamVideoRenderer> MockRenderFactory::GetVideoRenderer(
    const WebMediaStream& web_stream,
    const MediaStreamVideoRenderer::RepaintCB& repaint_cb,
    scoped_refptr<base::SequencedTaskRunner> video_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner) {
  if (!support_video_renderer_)
    return nullptr;

  provider_ = base::MakeRefCounted<MockMediaStreamVideoRenderer>(
      task_runner_, message_loop_controller_, repaint_cb, task_environment_);

  return provider_;
}

// This is the main class coordinating the tests.
// Basic workflow:
// 1. WebMediaPlayerMS::Load will generate and start
// MediaStreamVideoRenderer.
// 2. MediaStreamVideoRenderer will start pushing frames into
//    WebMediaPlayerMS repeatedly.
// 3. On WebMediaPlayerMS receiving the first frame, a cc::Layer will be
//    created.
// 4. The cc::Layer will call
//    WebMediaPlayerMSCompositor::SetVideoFrameProviderClient, which in turn
//    will trigger cc::VideoFrameProviderClient::StartRendering.
// 5. Then cc::VideoFrameProviderClient will start calling
//    WebMediaPlayerMSCompositor::UpdateCurrentFrame, GetCurrentFrame for
//    rendering repeatedly.
// 6. When WebMediaPlayerMS::pause gets called, it should trigger
//    MediaStreamVideoRenderer::Pause, and then the provider will stop
//    pushing frames into WebMediaPlayerMS, but instead digesting them;
//    simultanously, it should call cc::VideoFrameProviderClient::StopRendering,
//    so cc::VideoFrameProviderClient will stop asking frames from
//    WebMediaPlayerMSCompositor.
// 7. When WebMediaPlayerMS::play gets called, evething paused in step 6 should
//    be resumed.
class WebMediaPlayerMSTest
    : public testing::TestWithParam<
          testing::tuple<bool /* enable_surface_layer_for_video */,
                         bool /* opaque_frame */,
                         bool /* odd_size_frame */>>,
      public WebMediaPlayerClient,
      public cc::VideoFrameProvider::Client {
 public:
  WebMediaPlayerMSTest()
      : render_factory_(new MockRenderFactory(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            &message_loop_controller_,
            &task_environment_)),
        gpu_factories_(new media::MockGpuVideoAcceleratorFactories(nullptr)),
        surface_layer_bridge_(
            std::make_unique<NiceMock<MockSurfaceLayerBridge>>()),
        submitter_(std::make_unique<NiceMock<MockWebVideoFrameSubmitter>>()),
        layer_set_(false),
        rendering_(false),
        background_rendering_(false) {
    surface_layer_bridge_ptr_ = surface_layer_bridge_.get();
    submitter_ptr_ = submitter_.get();
  }
  ~WebMediaPlayerMSTest() override {
    player_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void InitializeWebMediaPlayerMS();

  MockMediaStreamVideoRenderer* LoadAndGetFrameProvider(bool algorithm_enabled);

  // Implementation of WebMediaPlayerClient
  void NetworkStateChanged() override;
  void ReadyStateChanged() override;
  void TimeChanged() override {}
  void Repaint() override {}
  void DurationChanged() override {}
  void SizeChanged() override;
  void SetCcLayer(cc::Layer* layer) override;
  void OnFirstFrame(base::TimeTicks, size_t) override {}

  void RemoveMediaTrack(const media::MediaTrack&) override {}
  void AddMediaTrack(const media::MediaTrack& track) override {}

  void MediaSourceOpened(std::unique_ptr<WebMediaSource>) override {}
  void RemotePlaybackCompatibilityChanged(const WebURL& url,
                                          bool is_compatible) override {}
  bool WasAlwaysMuted() override { return false; }
  bool HasSelectedVideoTrack() override { return false; }
  WebMediaPlayer::TrackId GetSelectedVideoTrackId() override {
    return WebMediaPlayer::TrackId();
  }
  bool HasNativeControls() override { return false; }
  bool IsAudioElement() override { return is_audio_element_; }
  bool IsInAutoPIP() const override { return false; }
  void MediaRemotingStarted(
      const WebString& remote_device_friendly_name) override {}
  void MediaRemotingStopped(int error_code) override {}
  void ResumePlayback() override {}
  void PausePlayback(PauseReason) override {}
  void DidPlayerStartPlaying() override {}
  void DidPlayerPaused(bool) override {}
  void DidPlayerMutedStatusChange(bool muted) override {}
  void DidMediaMetadataChange(bool has_audio,
                              bool has_video,
                              media::AudioCodec audio_codec,
                              media::VideoCodec video_codec,
                              media::MediaContentType media_content_type,
                              bool is_encrypted_media) override {}
  void DidPlayerMediaPositionStateChange(double playback_rate,
                                         base::TimeDelta duration,
                                         base::TimeDelta position,
                                         bool end_of_media) override {}
  void DidDisableAudioOutputSinkChanges() override {}
  void DidUseAudioServiceChange(bool uses_audio_service) override {}
  void DidPlayerSizeChange(const gfx::Size& size) override {}
  void OnRemotePlaybackDisabled(bool disabled) override {}

  // Implementation of cc::VideoFrameProvider::Client
  void StopUsingProvider() override;
  void StartRendering() override;
  void StopRendering() override;
  void DidReceiveFrame() override;
  bool IsDrivingFrameUpdates() const override { return true; }
  void OnPictureInPictureStateChange() override {}

  // For test use
  void SetBackgroundRendering(bool background_rendering) {
    background_rendering_ = background_rendering;
  }

  void SetGpuMemoryBufferVideoForTesting() {
#if BUILDFLAG(IS_WIN)
    render_factory_->provider()->set_standard_size(
        WebMediaPlayerMS::kUseGpuMemoryBufferVideoFramesMinResolution);
#endif  // BUILDFLAG(IS_WIN)

    player_->SetGpuMemoryBufferVideoForTesting(
        new media::MockGpuMemoryBufferVideoFramePool(&frame_ready_cbs_));
  }

  void DisableMaxVsyncDelayForRendererReset() {
    compositor_->maximum_vsync_delay_for_renderer_reset_ =
        base::TimeDelta::Max();
  }
  // Sets the value of the rendering_ flag. Called from expectations in the
  // test.
  void SetRendering(bool rendering) { rendering_ = rendering; }

 protected:
  MOCK_METHOD0(DoStartRendering, void());
  MOCK_METHOD0(DoStopRendering, void());
  MOCK_METHOD0(DoDidReceiveFrame, void());
  MOCK_METHOD0(DoOnPictureInPictureStateChange, void());

  MOCK_METHOD1(DoSetCcLayer, void(bool));
  MOCK_METHOD1(DoNetworkStateChanged, void(WebMediaPlayer::NetworkState));
  MOCK_METHOD1(DoReadyStateChanged, void(WebMediaPlayer::ReadyState));
  MOCK_METHOD1(CheckSizeChanged, void(gfx::Size));
  MOCK_CONST_METHOD0(GetDisplayType, DisplayType());
  MOCK_CONST_METHOD0(CouldPlayIfEnoughData, bool());
  MOCK_METHOD0(OnRequestVideoFrameCallback, void());
  MOCK_METHOD0(GetElementId, int());

  std::unique_ptr<WebSurfaceLayerBridge> CreateMockSurfaceLayerBridge(
      WebSurfaceLayerBridgeObserver*,
      cc::UpdateSubmissionStateCB) {
    return std::move(surface_layer_bridge_);
  }

  // Testing harness for the GetVideoFramePresentationMetadata test.
  void TestGetVideoFramePresentationMetadata(bool algorithm_enabled);

  // Testing harness for the RequestVideoFrameCallback test.
  void TestRequestFrameCallbackWithVideoFrameMetadata(bool algorithm_enabled);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<MockRenderFactory, DanglingUntriaged> render_factory_;
  std::unique_ptr<media::MockGpuVideoAcceleratorFactories> gpu_factories_;
  FakeWebMediaPlayerDelegate delegate_;
  std::unique_ptr<WebMediaPlayerMS> player_;
  raw_ptr<WebMediaPlayerMSCompositor, DanglingUntriaged> compositor_;
  ReusableMessageLoopEvent message_loop_controller_;
  raw_ptr<cc::Layer> layer_;
  bool is_audio_element_ = false;
  std::vector<base::OnceClosure> frame_ready_cbs_;
  std::unique_ptr<NiceMock<MockSurfaceLayerBridge>> surface_layer_bridge_;
  std::unique_ptr<NiceMock<MockWebVideoFrameSubmitter>> submitter_;
  raw_ptr<NiceMock<MockSurfaceLayerBridge>, DanglingUntriaged>
      surface_layer_bridge_ptr_ = nullptr;
  raw_ptr<NiceMock<MockWebVideoFrameSubmitter>, DanglingUntriaged>
      submitter_ptr_ = nullptr;
  bool enable_surface_layer_for_video_ = false;
  base::TimeTicks deadline_min_;
  base::TimeTicks deadline_max_;

 private:
  // Main function trying to ask WebMediaPlayerMS to submit a frame for
  // rendering.
  void RenderFrame();

  bool layer_set_;
  bool rendering_;
  bool background_rendering_;

  base::WeakPtrFactory<WebMediaPlayerMSTest> weak_factory_{this};
};

void WebMediaPlayerMSTest::InitializeWebMediaPlayerMS() {
  enable_surface_layer_for_video_ = testing::get<0>(GetParam());
  player_ = std::make_unique<WebMediaPlayerMS>(
      nullptr, this, &delegate_, std::make_unique<media::NullMediaLog>(),
      scheduler::GetSingleThreadTaskRunnerForTesting(),
      scheduler::GetSingleThreadTaskRunnerForTesting(),
      scheduler::GetSingleThreadTaskRunnerForTesting(),
      scheduler::GetSingleThreadTaskRunnerForTesting(),
      scheduler::GetSingleThreadTaskRunnerForTesting(), gpu_factories_.get(),
      WebString(),
      WTF::BindOnce(&WebMediaPlayerMSTest::CreateMockSurfaceLayerBridge,
                    WTF::Unretained(this)),
      std::move(submitter_), enable_surface_layer_for_video_);
  player_->SetMediaStreamRendererFactoryForTesting(
      std::unique_ptr<MediaStreamRendererFactory>(render_factory_));
}

MockMediaStreamVideoRenderer* WebMediaPlayerMSTest::LoadAndGetFrameProvider(
    bool algorithm_enabled) {
  EXPECT_FALSE(!!render_factory_->provider()) << "There should not be a "
                                                 "FrameProvider yet.";

  EXPECT_CALL(*this,
              DoNetworkStateChanged(WebMediaPlayer::kNetworkStateLoading));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveNothing));
  player_->Load(WebMediaPlayer::kLoadTypeURL, WebMediaPlayerSource(),
                WebMediaPlayer::kCorsModeUnspecified,
                /*is_cache_disabled=*/false);
  compositor_ = player_->compositor_.get();
  EXPECT_TRUE(!!compositor_);
  compositor_->SetAlgorithmEnabledForTesting(algorithm_enabled);

  MockMediaStreamVideoRenderer* provider = nullptr;
  if (render_factory_->support_video_renderer()) {
    provider = render_factory_->provider();
    EXPECT_TRUE(!!provider);
    EXPECT_TRUE(provider->Started());
  }

  testing::Mock::VerifyAndClearExpectations(this);
  return provider;
}

void WebMediaPlayerMSTest::NetworkStateChanged() {
  WebMediaPlayer::NetworkState state = player_->GetNetworkState();
  DoNetworkStateChanged(state);
  if (state == WebMediaPlayer::NetworkState::kNetworkStateFormatError ||
      state == WebMediaPlayer::NetworkState::kNetworkStateDecodeError ||
      state == WebMediaPlayer::NetworkState::kNetworkStateNetworkError) {
    message_loop_controller_.GetPipelineStatusCB().Run(
        media::PIPELINE_ERROR_NETWORK);
  }
}

void WebMediaPlayerMSTest::ReadyStateChanged() {
  WebMediaPlayer::ReadyState state = player_->GetReadyState();
  DoReadyStateChanged(state);
  if (state == WebMediaPlayer::ReadyState::kReadyStateHaveMetadata &&
      !player_->HasAudio()) {
    const auto& size = player_->NaturalSize();
    EXPECT_GT(size.width(), 0);
    EXPECT_GT(size.height(), 0);
  }
  if (state == WebMediaPlayer::ReadyState::kReadyStateHaveEnoughData)
    player_->Play();
}

void WebMediaPlayerMSTest::SetCcLayer(cc::Layer* layer) {
  // Make sure that the old layer is still alive, see https://crbug.com/705448.
  if (layer_set_)
    EXPECT_TRUE(layer_);
  layer_set_ = layer ? true : false;

  layer_ = layer;
  if (layer) {
    if (enable_surface_layer_for_video_)
      compositor_->SetVideoFrameProviderClient(submitter_ptr_);
    else
      compositor_->SetVideoFrameProviderClient(this);
  }
  DoSetCcLayer(!!layer);
}

void WebMediaPlayerMSTest::StopUsingProvider() {
  if (rendering_)
    StopRendering();
}

void WebMediaPlayerMSTest::StartRendering() {
  if (!rendering_) {
    rendering_ = true;
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE, WTF::BindOnce(&WebMediaPlayerMSTest::RenderFrame,
                                 weak_factory_.GetWeakPtr()));
  }
  DoStartRendering();
}

void WebMediaPlayerMSTest::StopRendering() {
  rendering_ = false;
  DoStopRendering();
}

void WebMediaPlayerMSTest::DidReceiveFrame() {
  if (background_rendering_)
    DoDidReceiveFrame();
}

void WebMediaPlayerMSTest::RenderFrame() {
  if (!rendering_ || !compositor_)
    return;

  // Advance the clock by 100 milliseconds for each RenderFrame call.
  task_environment_.AdvanceClock(base::Milliseconds(100));

  base::TimeTicks now = base::TimeTicks::Now();
  deadline_min_ = now + base::Seconds(1.0 / 60.0);
  deadline_max_ = deadline_min_ + base::Seconds(1.0 / 60.0);

  // Background rendering is different from stop rendering. The rendering loop
  // is still running but we do not ask frames from |compositor_|. And
  // background rendering is not initiated from |compositor_|.
  if (!background_rendering_) {
    compositor_->UpdateCurrentFrame(deadline_min_, deadline_max_);
    auto frame = compositor_->GetCurrentFrame();
    compositor_->PutCurrentFrame();
  }
  scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
      FROM_HERE,
      WTF::BindOnce(&WebMediaPlayerMSTest::RenderFrame,
                    weak_factory_.GetWeakPtr()),
      base::Seconds(1.0 / 60.0));
}

void WebMediaPlayerMSTest::SizeChanged() {
  gfx::Size frame_size = compositor_->GetMetadata().natural_size;
  CheckSizeChanged(frame_size);
}

void WebMediaPlayerMSTest::TestGetVideoFramePresentationMetadata(
    bool algorithm_enabled) {
  InitializeWebMediaPlayerMS();

  MockMediaStreamVideoRenderer* provider =
      LoadAndGetFrameProvider(algorithm_enabled);

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  Vector<int> timestamps({0, kTestBrake, 33, kTestBrake, 66, kTestBrake});
  provider->QueueFrames(timestamps);

  // Chain calls to video.rVFC.
  int num_frames = 3;
  player_->RequestVideoFrameCallback();

  // Verify that the presentation frame counter is monotonically increasing.
  // Queue up a rVFC call immediately after each frame.
  int last_frame_counter = -1;
  EXPECT_CALL(*this, OnRequestVideoFrameCallback())
      .Times(num_frames)
      .WillRepeatedly([&]() {
        auto metadata = player_->GetVideoFramePresentationMetadata();
        EXPECT_GT((int)metadata->presented_frames, last_frame_counter);
        last_frame_counter = metadata->presented_frames;
        if (!algorithm_enabled && !enable_surface_layer_for_video_ &&
            !deadline_min_.is_null()) {
          // We use EXPECT_GE to compare the deadline_max value with the
          // expected display time. This is because the deadline_max_ member
          // gets updated in the RenderFrame() function which may get called
          // multiple times before the OnRequestVideoFrameCallback() is invoked.
          EXPECT_GE(deadline_max_, metadata->expected_display_time);
        }
        player_->RequestVideoFrameCallback();
      });

  // Wait for each of the frame/kTestBreak pairs.
  while (num_frames--) {
    // Advance the clock by 10 milliseconds before each frame is retrieved to
    // emulate real system clock behavior.
    task_environment_.AdvanceClock(base::Milliseconds(10));
    message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  }
  testing::Mock::VerifyAndClearExpectations(this);
}

void WebMediaPlayerMSTest::TestRequestFrameCallbackWithVideoFrameMetadata(
    bool algorithm_enabled) {
  InitializeWebMediaPlayerMS();

  MockMediaStreamVideoRenderer* provider =
      LoadAndGetFrameProvider(algorithm_enabled);

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  Vector<int> timestamps({0, 33, kTestBrake, 66, 100, 133, 166});
  provider->QueueFrames(timestamps);

  // Verify a basic call to rVFC
  player_->RequestVideoFrameCallback();
  EXPECT_CALL(*this, OnRequestVideoFrameCallback()).Times(1);
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);

  auto metadata = player_->GetVideoFramePresentationMetadata();

  EXPECT_GT(metadata->presentation_time, base::TimeTicks());
  EXPECT_GE(metadata->expected_display_time, metadata->presentation_time);
  testing::Mock::VerifyAndClearExpectations(this);

  // Make sure multiple calls to rVFC only result in one call per frame to
  // OnRVFC.
  player_->RequestVideoFrameCallback();
  player_->RequestVideoFrameCallback();
  player_->RequestVideoFrameCallback();

  EXPECT_CALL(*this, OnRequestVideoFrameCallback())
      .Times(1)
      .WillOnce(testing::Invoke([&]() {
        if (!algorithm_enabled && !enable_surface_layer_for_video_) {
          metadata = player_->GetVideoFramePresentationMetadata();
          // We use EXPECT_GE to compare the deadline_max value with the
          // expected display time. This is because the deadline_max_ member
          // gets updated in the RenderFrame() function which may get called
          // multiple times before the OnRequestVideoFrameCallback() is invoked.
          EXPECT_GE(deadline_max_, metadata->expected_display_time);
        }
      }));
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_P(WebMediaPlayerMSTest, NoDataDuringLoadForVideo) {
  InitializeWebMediaPlayerMS();
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata))
      .Times(0);
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData))
      .Times(0);

  LoadAndGetFrameProvider(true);

  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetCcLayer(false));
}

TEST_P(WebMediaPlayerMSTest, NoWaitForFrameForAudio) {
  InitializeWebMediaPlayerMS();
  is_audio_element_ = true;
  auto audio_renderer = base::MakeRefCounted<MockMediaStreamAudioRenderer>();
  render_factory_->set_audio_renderer(audio_renderer);
  EXPECT_CALL(*this,
              DoNetworkStateChanged(WebMediaPlayer::kNetworkStateLoading));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveNothing));

  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));

  player_->Load(WebMediaPlayer::kLoadTypeURL, WebMediaPlayerSource(),
                WebMediaPlayer::kCorsModeUnspecified,
                /*is_cache_disabled=*/false);

  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetCcLayer(false));
}

// Test that OnPictureInPictureStateChange is not called for audio elements.
// This test explicitly sets display type to picture in picture, for an audio
// element, for testing purposes only (See crbug.com/1403547 for reference).
TEST_P(WebMediaPlayerMSTest, PictureInPictureStateChangeNotCalled) {
  InitializeWebMediaPlayerMS();
  is_audio_element_ = true;
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  Vector<int> timestamps({0, 33, 66, 100, 133, 166, 200, 233, 266, 300, 333,
                          366, 400, 433, 466, 500, 533, 566, 600});
  provider->QueueFrames(timestamps);

  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*submitter_ptr_, StartRendering());
    EXPECT_CALL(*this, GetDisplayType())
        .WillRepeatedly(Return(DisplayType::kPictureInPicture));

  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStartRendering());
  }
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));
  EXPECT_CALL(*this,
              CheckSizeChanged(gfx::Size(kStandardWidth, kStandardHeight)));
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  const gfx::Size& natural_size = player_->NaturalSize();
  EXPECT_EQ(kStandardWidth, natural_size.width());
  EXPECT_EQ(kStandardHeight, natural_size.height());
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetCcLayer(false));
  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*submitter_ptr_, StopUsingProvider());
  } else {
    EXPECT_CALL(*this, DoStopRendering());
  }
  EXPECT_CALL(*this, DoOnPictureInPictureStateChange()).Times(0);
}

TEST_P(WebMediaPlayerMSTest, NoWaitForFrameForAudioOnly) {
  InitializeWebMediaPlayerMS();
  render_factory_->set_support_video_renderer(false);
  auto audio_renderer = base::MakeRefCounted<MockMediaStreamAudioRenderer>();
  render_factory_->set_audio_renderer(audio_renderer);
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));
  LoadAndGetFrameProvider(true);
  EXPECT_CALL(*this, DoSetCcLayer(false));
}

TEST_P(WebMediaPlayerMSTest, Playing_Normal) {
  // This test sends a bunch of normal frames with increasing timestamps
  // and verifies that they are produced by WebMediaPlayerMS in appropriate
  // order.

  InitializeWebMediaPlayerMS();

  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  Vector<int> timestamps({0, 33, 66, 100, 133, 166, 200, 233, 266, 300, 333,
                          366, 400, 433, 466, 500, 533, 566, 600});
  provider->QueueFrames(timestamps);

  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*submitter_ptr_, StartRendering());
  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStartRendering());
  }
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));
  EXPECT_CALL(*this,
              CheckSizeChanged(gfx::Size(kStandardWidth, kStandardHeight)));
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  const gfx::Size& natural_size = player_->NaturalSize();
  EXPECT_EQ(kStandardWidth, natural_size.width());
  EXPECT_EQ(kStandardHeight, natural_size.height());
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetCcLayer(false));
  if (enable_surface_layer_for_video_)
    EXPECT_CALL(*submitter_ptr_, StopUsingProvider());
  else
    EXPECT_CALL(*this, DoStopRendering());
}

TEST_P(WebMediaPlayerMSTest, PlayThenPause) {
  InitializeWebMediaPlayerMS();
  const bool opaque_frame = testing::get<1>(GetParam());
  const bool odd_size_frame = testing::get<2>(GetParam());
  // In the middle of this test, WebMediaPlayerMS::pause will be called, and we
  // are going to verify that during the pause stage, a frame gets freezed, and
  // cc::VideoFrameProviderClient should also be paused.
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(false);

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  Vector<int> timestamps({0,   33,  66,  100, 133, kTestBrake, 166,
                          200, 233, 266, 300, 333, 366,        400,
                          433, 466, 500, 533, 566, 600});
  provider->QueueFrames(timestamps, opaque_frame, odd_size_frame);

  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(*submitter_ptr_, StartRendering());
  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStartRendering());
  }
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));
  gfx::Size frame_size =
      gfx::Size(kStandardWidth - (odd_size_frame ? kOddSizeOffset : 0),
                kStandardHeight - (odd_size_frame ? kOddSizeOffset : 0));
  EXPECT_CALL(*this, CheckSizeChanged(frame_size));
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);

  // Here we call pause, and expect a freezing frame.
  if (enable_surface_layer_for_video_)
    EXPECT_CALL(*submitter_ptr_, StopRendering());
  else
    EXPECT_CALL(*this, DoStopRendering());

  player_->Pause();
  auto prev_frame = compositor_->GetCurrentFrame();
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  auto after_frame = compositor_->GetCurrentFrame();
  EXPECT_EQ(prev_frame->timestamp(), after_frame->timestamp());
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetCcLayer(false));
}

TEST_P(WebMediaPlayerMSTest, PlayThenPauseThenPlay) {
  InitializeWebMediaPlayerMS();
  const bool opaque_frame = testing::get<1>(GetParam());
  const bool odd_size_frame = testing::get<2>(GetParam());
  // Similary to PlayAndPause test above, this one focuses on testing that
  // WebMediaPlayerMS can be resumed after a period of paused status.
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(false);

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  Vector<int> timestamps({0,   33,         66,  100, 133, kTestBrake, 166,
                          200, 233,        266, 300, 333, 366,        400,
                          433, kTestBrake, 466, 500, 533, 566,        600});
  provider->QueueFrames(timestamps, opaque_frame, odd_size_frame);

  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(*submitter_ptr_, StartRendering());
  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStartRendering());
  }
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));
  gfx::Size frame_size =
      gfx::Size(kStandardWidth - (odd_size_frame ? kOddSizeOffset : 0),
                kStandardHeight - (odd_size_frame ? kOddSizeOffset : 0));
  EXPECT_CALL(*this, CheckSizeChanged(frame_size));
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);

  // Here we call pause, and expect a freezing frame.
  if (enable_surface_layer_for_video_)
    EXPECT_CALL(*submitter_ptr_, StopRendering());
  else
    EXPECT_CALL(*this, DoStopRendering());

  player_->Pause();
  auto prev_frame = compositor_->GetCurrentFrame();
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  auto after_frame = compositor_->GetCurrentFrame();
  EXPECT_EQ(prev_frame->timestamp(), after_frame->timestamp());
  testing::Mock::VerifyAndClearExpectations(this);

  // We resume the player, and expect rendering can continue.
  if (enable_surface_layer_for_video_)
    EXPECT_CALL(*submitter_ptr_, StartRendering());
  else
    EXPECT_CALL(*this, DoStartRendering());

  player_->Play();
  prev_frame = compositor_->GetCurrentFrame();
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  after_frame = compositor_->GetCurrentFrame();
  EXPECT_NE(prev_frame->timestamp(), after_frame->timestamp());
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetCcLayer(false));
  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*submitter_ptr_, StopUsingProvider());
  } else {
    EXPECT_CALL(*this, DoStopRendering());
  }
}

// During this test, we check that when we send rotated video frames, it applies
// to player's natural size.
TEST_P(WebMediaPlayerMSTest, RotationChange) {
  InitializeWebMediaPlayerMS();
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  Vector<int> timestamps({0, kTestBrake});
  provider->QueueFrames(timestamps, false, false, 17, media::VIDEO_ROTATION_90);
  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(*submitter_ptr_, StartRendering());
  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStartRendering());
  }
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));
  // Expect SizeChanged happens 3 times for the 3 rotation changes because the
  // natural size changes on each rotation change. However, CheckSizeChanged
  // checks the compositor size, which is pre-rotation and is the same for the
  // 3 rotation changes.
  EXPECT_CALL(*this,
              CheckSizeChanged(gfx::Size(kStandardWidth, kStandardHeight)))
      .Times(3);
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  // Advance the clock by 100 milliseconds to emulate real clock timing.
  task_environment_.AdvanceClock(base::Milliseconds(100));
  // The exact ordering of delayed vs non-delayed tasks is not defined.
  // Make sure we run all non-delayed tasks (E.G. CheckForFrameChanges) before
  // testing state.
  base::RunLoop().RunUntilIdle();
  gfx::Size natural_size = player_->NaturalSize();
  // Check that height and width are flipped.
  EXPECT_EQ(kStandardHeight, natural_size.width());
  EXPECT_EQ(kStandardWidth, natural_size.height());

  // Change rotation.
  timestamps = Vector<int>({33, kTestBrake});
  provider->QueueFrames(timestamps, false, false, 17, media::VIDEO_ROTATION_0);
  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*submitter_ptr_, SetTransform(media::kNoTransformation));
  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStopRendering()).WillOnce([&]() {
      scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
          FROM_HERE, WTF::BindOnce(
                         [](WebMediaPlayerMSTest* test) {
                           // Turn off rendering here to avoid an infinite loop.
                           test->SetRendering(/*rendering=*/false);
                         },
                         WTF::Unretained(this)));
    });
    EXPECT_CALL(*this, DoStartRendering());
  }
  // Kickstart the Provider InjectFrame as that task may not be running
  // anymore. This is needed now as we switched to mock clock.
  provider->InjectFrame();
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  // Advance the clock by 100 milliseconds to emulate real clock timing.
  task_environment_.AdvanceClock(base::Milliseconds(100));
  base::RunLoop().RunUntilIdle();
  natural_size = player_->NaturalSize();
  EXPECT_EQ(kStandardHeight, natural_size.height());
  EXPECT_EQ(kStandardWidth, natural_size.width());

  // Change rotation again.
  // Enable rendering again as we turned it off above.
  SetRendering(/*rendering=*/true);
  timestamps = Vector<int>({66, kTestBrake});
  provider->QueueFrames(timestamps, false, false, 17, media::VIDEO_ROTATION_90);
  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(
        *submitter_ptr_,
        SetTransform(media::VideoTransformation(media::VIDEO_ROTATION_90)));
  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStopRendering()).WillOnce([&]() {
      scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
          FROM_HERE, WTF::BindOnce(
                         [](WebMediaPlayerMSTest* test) {
                           // Turn off rendering here to avoid an infinite loop.
                           test->SetRendering(/*rendering=*/false);
                         },
                         WTF::Unretained(this)));
    });
    EXPECT_CALL(*this, DoStartRendering());
  }
  // Kickstart the Provider InjectFrame as that task may not be running
  // anymore. This is needed now as we switched to mock clock.
  provider->InjectFrame();
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  // Advance the clock by 100 milliseconds to emulate real clock timing.
  task_environment_.AdvanceClock(base::Milliseconds(100));
  base::RunLoop().RunUntilIdle();
  natural_size = player_->NaturalSize();
  EXPECT_EQ(kStandardHeight, natural_size.width());
  EXPECT_EQ(kStandardWidth, natural_size.height());

  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_CALL(*this, DoSetCcLayer(false));

  if (enable_surface_layer_for_video_)
    EXPECT_CALL(*submitter_ptr_, StopUsingProvider());
}

// During this test, we check that web layer changes opacity according to the
// given frames.
TEST_P(WebMediaPlayerMSTest, OpacityChange) {
  InitializeWebMediaPlayerMS();
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  // Push one opaque frame.
  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  Vector<int> timestamps({0, kTestBrake});
  provider->QueueFrames(timestamps, true);

  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(*submitter_ptr_, StartRendering());
  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStartRendering());
  }
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));
  EXPECT_CALL(*this,
              CheckSizeChanged(gfx::Size(kStandardWidth, kStandardHeight)));
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  // The exact ordering of delayed vs non-delayed tasks is not defined.
  // Make sure we run all non-delayed tasks before testing state.
  base::RunLoop().RunUntilIdle();
  if (!enable_surface_layer_for_video_) {
    ASSERT_TRUE(layer_);
    EXPECT_TRUE(layer_->contents_opaque());
  }

  // Push one transparent frame.
  timestamps = Vector<int>({33, kTestBrake});
  provider->QueueFrames(timestamps, false);
  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  }
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  base::RunLoop().RunUntilIdle();
  if (!enable_surface_layer_for_video_) {
    EXPECT_FALSE(layer_->contents_opaque());
  }

  // Push another transparent frame.
  timestamps = Vector<int>({66, kTestBrake});
  provider->QueueFrames(timestamps, true);
  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(true));
  }
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  base::RunLoop().RunUntilIdle();
  if (!enable_surface_layer_for_video_) {
    EXPECT_TRUE(layer_->contents_opaque());
  }

  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_CALL(*this, DoSetCcLayer(false));
  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*submitter_ptr_, StopUsingProvider());
  } else {
    EXPECT_CALL(*this, DoStopRendering());
  }
}

TEST_P(WebMediaPlayerMSTest, BackgroundRendering) {
  // During this test, we will switch to background rendering mode, in which
  // WebMediaPlayerMS::pause does not get called, but
  // cc::VideoFrameProviderClient simply stops asking frames from
  // WebMediaPlayerMS without an explicit notification. We should expect that
  // WebMediaPlayerMS can digest old frames, rather than piling frames up and
  // explode.
  InitializeWebMediaPlayerMS();
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  Vector<int> timestamps({0,   33,         66,  100, 133, kTestBrake, 166,
                          200, 233,        266, 300, 333, 366,        400,
                          433, kTestBrake, 466, 500, 533, 566,        600});
  provider->QueueFrames(timestamps);

  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(*submitter_ptr_, StartRendering());
  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStartRendering());
  }
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));
  gfx::Size frame_size = gfx::Size(kStandardWidth, kStandardHeight);
  EXPECT_CALL(*this, CheckSizeChanged(frame_size));
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);

  // Switch to background rendering, expect rendering to continue for all the
  // frames between kTestBrake frames.
  if (enable_surface_layer_for_video_)
    EXPECT_CALL(*submitter_ptr_, DidReceiveFrame()).Times(testing::AtLeast(1));
  else
    EXPECT_CALL(*this, DoDidReceiveFrame()).Times(testing::AtLeast(1));

  SetBackgroundRendering(true);
  auto prev_frame = compositor_->GetCurrentFrame();
  // Advance the clock by 10 milliseconds to emulate the real system clock.
  task_environment_.AdvanceClock(base::Milliseconds(10));
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  auto after_frame = compositor_->GetCurrentFrame();
  EXPECT_NE(prev_frame->timestamp(), after_frame->timestamp());

  // Switch to foreground rendering.
  SetBackgroundRendering(false);
  prev_frame = compositor_->GetCurrentFrame();
  // Advance the clock by 10 milliseconds to emulate the real system clock.
  task_environment_.AdvanceClock(base::Milliseconds(10));
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  after_frame = compositor_->GetCurrentFrame();
  EXPECT_NE(prev_frame->timestamp(), after_frame->timestamp());
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetCcLayer(false));
  if (enable_surface_layer_for_video_)
    EXPECT_CALL(*submitter_ptr_, StopUsingProvider());
  else
    EXPECT_CALL(*this, DoStopRendering());
}

TEST_P(WebMediaPlayerMSTest, FrameSizeChange) {
  // During this test, the frame size of the input changes.
  // We need to make sure, when sizeChanged() gets called, new size should be
  // returned by GetMetadata().
  InitializeWebMediaPlayerMS();
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  Vector<int> timestamps({0, 33, 66, 100, 133, 166, 200, 233, 266, 300, 333,
                          366, 400, 433, 466, 500, 533, 566, 600});
  provider->QueueFrames(timestamps, false, false, 7);

  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(*submitter_ptr_, StartRendering());
  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStartRendering());
  }
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));
  EXPECT_CALL(*this,
              CheckSizeChanged(gfx::Size(kStandardWidth, kStandardHeight)));
  EXPECT_CALL(*this, CheckSizeChanged(
                         gfx::Size(kStandardWidth * 2, kStandardHeight * 2)));
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetCcLayer(false));
  if (enable_surface_layer_for_video_)
    EXPECT_CALL(*submitter_ptr_, StopUsingProvider());
  else
    EXPECT_CALL(*this, DoStopRendering());
}

// Tests that GpuMemoryBufferVideoFramePool is called in the expected sequence.
TEST_P(WebMediaPlayerMSTest, CreateHardwareFrames) {
  InitializeWebMediaPlayerMS();
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(false);
  SetGpuMemoryBufferVideoForTesting();

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  Vector<int> timestamps({0, kTestBrake});
  provider->QueueFrames(timestamps);
  message_loop_controller_.RunAndWaitForStatus(media::PIPELINE_OK);

  ASSERT_EQ(1u, frame_ready_cbs_.size());
  if (enable_surface_layer_for_video_) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(*submitter_ptr_, StartRendering());
  } else {
    EXPECT_CALL(*this, DoSetCcLayer(true));
    EXPECT_CALL(*this, DoStartRendering());
  }
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this,
              DoReadyStateChanged(WebMediaPlayer::kReadyStateHaveEnoughData));
  EXPECT_CALL(*this, CheckSizeChanged(provider->get_standard_size()));

  // Run all the tasks that will assign current frame in
  // WebMediaPlayerMSCompositor.
  std::move(frame_ready_cbs_[0]).Run();
  base::RunLoop().RunUntilIdle();

  auto frame = compositor_->GetCurrentFrame();
  ASSERT_TRUE(frame);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetCcLayer(false));
  if (enable_surface_layer_for_video_)
    EXPECT_CALL(*submitter_ptr_, StopUsingProvider());
  else
    EXPECT_CALL(*this, DoStopRendering());
}
#if BUILDFLAG(IS_ANDROID)
TEST_P(WebMediaPlayerMSTest, HiddenPlayerTests) {
  InitializeWebMediaPlayerMS();
  LoadAndGetFrameProvider(true);

  // Hidden status should not affect playback.
  delegate_.set_page_hidden(true);
  player_->Play();
  EXPECT_FALSE(player_->Paused());

  // A hidden player should start still be playing upon shown.
  delegate_.set_page_hidden(false);
  player_->OnPageShown();
  EXPECT_FALSE(player_->Paused());

  // A hidden event should not pause the player.
  delegate_.set_page_hidden(true);
  player_->OnPageHidden();
  EXPECT_FALSE(player_->Paused());

  // A user generated pause() should clear the automatic resumption.
  player_->Pause();
  delegate_.set_page_hidden(false);
  player_->OnPageShown();
  EXPECT_TRUE(player_->Paused());

  // A user generated play() should start playback.
  player_->Play();
  EXPECT_FALSE(player_->Paused());

  // An OnSuspendRequested() without forced suspension should do nothing.
  player_->OnIdleTimeout();
  EXPECT_FALSE(player_->Paused());

  // An OnSuspendRequested() with forced suspension should pause playback.
  player_->SuspendForFrameClosed();
  EXPECT_TRUE(player_->Paused());

  // OnShown() should restart after a forced suspension.
  player_->OnPageShown();
  EXPECT_FALSE(player_->Paused());
  EXPECT_CALL(*this, DoSetCcLayer(false));

  base::RunLoop().RunUntilIdle();
}
#endif

TEST_P(WebMediaPlayerMSTest, RequestVideoFrameCallback) {
  TestRequestFrameCallbackWithVideoFrameMetadata(/*algorithm_enabled=*/true);
}

TEST_P(WebMediaPlayerMSTest, RequestVideoFrameCallbackWithNoAlgorithm) {
  TestRequestFrameCallbackWithVideoFrameMetadata(/*algorithm_enabled=*/false);
}

TEST_P(WebMediaPlayerMSTest, RequestVideoFrameCallback_ForcesBeginFrames) {
  InitializeWebMediaPlayerMS();

  if (!enable_surface_layer_for_video_)
    return;

  LoadAndGetFrameProvider(true);

  EXPECT_CALL(*submitter_ptr_, SetForceBeginFrames(true));
  player_->RequestVideoFrameCallback();
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(submitter_ptr_);

  // The flag should be un-set when stop receiving callbacks.
  base::RunLoop run_loop;
  EXPECT_CALL(*submitter_ptr_, SetForceBeginFrames(false))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  run_loop.Run();

  testing::Mock::VerifyAndClear(submitter_ptr_);
}

TEST_P(WebMediaPlayerMSTest, GetVideoFramePresentationMetadata) {
  TestGetVideoFramePresentationMetadata(/*algorithm_enabled=*/true);
}

TEST_P(WebMediaPlayerMSTest, GetVideoFramePresentationMetadataWithNoAlgorithm) {
  TestGetVideoFramePresentationMetadata(/*algorithm_enabled=*/false);
}

TEST_P(WebMediaPlayerMSTest, DuplicateFrameTimestamp) {
  InitializeWebMediaPlayerMS();
  LoadAndGetFrameProvider(true);
  DisableMaxVsyncDelayForRendererReset();

  const bool opaque_frame = testing::get<1>(GetParam());
  const bool odd_size_frame = testing::get<2>(GetParam());

  gfx::Size frame_size(kStandardWidth - (odd_size_frame ? kOddSizeOffset : 0),
                       kStandardHeight - (odd_size_frame ? kOddSizeOffset : 0));

  constexpr auto kStep = base::Milliseconds(25);
  auto frame = media::VideoFrame::CreateZeroInitializedFrame(
      opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      frame_size, gfx::Rect(frame_size), frame_size, kStep);
  frame->metadata().reference_time = base::TimeTicks() + kStep;
  auto frame2 = media::VideoFrame::CreateZeroInitializedFrame(
      opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      frame_size, gfx::Rect(frame_size), frame_size, kStep);
  frame2->metadata().reference_time = base::TimeTicks() + kStep;
  auto frame3 = media::VideoFrame::CreateZeroInitializedFrame(
      opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      frame_size, gfx::Rect(frame_size), frame_size, kStep * 2);
  frame3->metadata().reference_time = base::TimeTicks() + kStep * 2;

  compositor_->EnqueueFrame(std::move(frame), true);
  compositor_->EnqueueFrame(std::move(frame2), true);
  compositor_->EnqueueFrame(std::move(frame3), true);

  compositor_->StartRendering();
  task_environment_.RunUntilIdle();

  base::TimeTicks deadline;
  deadline += kStep;  // Don't start deadline at zero.

  for (int i = 1; i <= 2; ++i) {
    EXPECT_TRUE(compositor_->UpdateCurrentFrame(deadline, deadline + kStep));
    deadline += kStep;
    frame = compositor_->GetCurrentFrame();
    EXPECT_EQ(frame->timestamp(), kStep * i);
    compositor_->PutCurrentFrame();
  }

  compositor_->StopRendering();
  task_environment_.RunUntilIdle();
}

TEST_P(WebMediaPlayerMSTest, HandlesArbitraryTimestampConversions) {
  InitializeWebMediaPlayerMS();
  LoadAndGetFrameProvider(true);
  DisableMaxVsyncDelayForRendererReset();

  const bool opaque_frame = testing::get<1>(GetParam());
  const bool odd_size_frame = testing::get<2>(GetParam());

  gfx::Size frame_size(kStandardWidth - (odd_size_frame ? kOddSizeOffset : 0),
                       kStandardHeight - (odd_size_frame ? kOddSizeOffset : 0));

  constexpr auto kStep = base::Milliseconds(25);
  auto frame = media::VideoFrame::CreateZeroInitializedFrame(
      opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      frame_size, gfx::Rect(frame_size), frame_size, kStep);
  frame->metadata().reference_time = base::TimeTicks() + kStep;
  frame->metadata().frame_duration = kStep - base::Microseconds(1);
  auto frame2 = media::VideoFrame::CreateZeroInitializedFrame(
      opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      frame_size, gfx::Rect(frame_size), frame_size, kStep * 2);
  frame2->metadata().reference_time = base::TimeTicks() + kStep * 2;
  frame2->metadata().frame_duration = kStep - base::Microseconds(1);

  compositor_->EnqueueFrame(std::move(frame), true);
  compositor_->EnqueueFrame(std::move(frame2), true);

  compositor_->StartRendering();
  task_environment_.RunUntilIdle();

  base::TimeTicks deadline;
  deadline += kStep;  // Don't start deadline at zero.

  for (int i = 1; i <= 2; ++i) {
    EXPECT_TRUE(compositor_->UpdateCurrentFrame(deadline, deadline + kStep));
    deadline += kStep;
    frame = compositor_->GetCurrentFrame();
    EXPECT_EQ(frame->timestamp(), kStep * i);
    compositor_->PutCurrentFrame();
  }

  compositor_->StopRendering();
  task_environment_.RunUntilIdle();
}

TEST_P(WebMediaPlayerMSTest, OutOfOrderEnqueue) {
  InitializeWebMediaPlayerMS();
  LoadAndGetFrameProvider(true);
  DisableMaxVsyncDelayForRendererReset();

  const bool opaque_frame = testing::get<1>(GetParam());
  const bool odd_size_frame = testing::get<2>(GetParam());

  gfx::Size frame_size(kStandardWidth - (odd_size_frame ? kOddSizeOffset : 0),
                       kStandardHeight - (odd_size_frame ? kOddSizeOffset : 0));

  constexpr auto kStep = base::Milliseconds(25);
  auto frame = media::VideoFrame::CreateZeroInitializedFrame(
      opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      frame_size, gfx::Rect(frame_size), frame_size, kStep);
  frame->metadata().reference_time = base::TimeTicks() + kStep;
  auto frame2 = media::VideoFrame::CreateZeroInitializedFrame(
      opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      frame_size, gfx::Rect(frame_size), frame_size, kStep * 2);
  frame2->metadata().reference_time = base::TimeTicks() + kStep * 2;
  auto frame3 = media::VideoFrame::CreateZeroInitializedFrame(
      opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      frame_size, gfx::Rect(frame_size), frame_size, kStep * 3);
  frame3->metadata().reference_time = base::TimeTicks() + kStep * 3;

  compositor_->EnqueueFrame(std::move(frame), true);
  compositor_->EnqueueFrame(std::move(frame3), true);
  compositor_->EnqueueFrame(std::move(frame2), true);

  compositor_->StartRendering();
  task_environment_.RunUntilIdle();

  // Frames 1, 3 should be dropped.
  base::TimeTicks deadline;
  deadline += kStep;  // Don't start deadline at zero.

  // Return value may be true or false depending on if surface layer is used.
  compositor_->UpdateCurrentFrame(deadline, deadline + kStep);

  frame = compositor_->GetCurrentFrame();
  ASSERT_TRUE(!!frame);
  EXPECT_EQ(frame->timestamp(), kStep * 2);
  compositor_->PutCurrentFrame();

  compositor_->StopRendering();
  task_environment_.RunUntilIdle();
}

TEST_P(WebMediaPlayerMSTest, ValidPreferredInterval) {
  InitializeWebMediaPlayerMS();
  LoadAndGetFrameProvider(true);

  const bool opaque_frame = testing::get<1>(GetParam());
  const bool odd_size_frame = testing::get<2>(GetParam());

  gfx::Size frame_size(kStandardWidth - (odd_size_frame ? kOddSizeOffset : 0),
                       kStandardHeight - (odd_size_frame ? kOddSizeOffset : 0));

  auto frame = media::VideoFrame::CreateZeroInitializedFrame(
      opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      frame_size, gfx::Rect(frame_size), frame_size, base::Seconds(10));

  compositor_->EnqueueFrame(std::move(frame), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_GE(compositor_->GetPreferredRenderInterval(), base::TimeDelta());

  frame = media::VideoFrame::CreateZeroInitializedFrame(
      opaque_frame ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      frame_size, gfx::Rect(frame_size), frame_size, base::Seconds(1));
  compositor_->EnqueueFrame(std::move(frame), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_GE(compositor_->GetPreferredRenderInterval(), base::TimeDelta());
}

TEST_P(WebMediaPlayerMSTest, OnContextLost) {
  InitializeWebMediaPlayerMS();
  LoadAndGetFrameProvider(true);

  gfx::Size frame_size(320, 240);
  auto non_gpu_frame = media::VideoFrame::CreateZeroInitializedFrame(
      media::PIXEL_FORMAT_I420, frame_size, gfx::Rect(frame_size), frame_size,
      base::Seconds(10));
  compositor_->EnqueueFrame(non_gpu_frame, true);
  base::RunLoop().RunUntilIdle();
  // frame without gpu resource should be remained even though context is lost
  compositor_->OnContextLost();
  EXPECT_EQ(non_gpu_frame, compositor_->GetCurrentFrame());

  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      std::make_unique<media::FakeGpuMemoryBuffer>(
          frame_size, gfx::BufferFormat::YUV_420_BIPLANAR);
  auto gpu_frame = media::VideoFrame::WrapExternalGpuMemoryBuffer(
      gfx::Rect(frame_size), frame_size, std::move(gmb), base::TimeDelta());
  compositor_->EnqueueFrame(gpu_frame, true);
  base::RunLoop().RunUntilIdle();
  // frame with gpu resource should be reset if context is lost
  compositor_->OnContextLost();
  EXPECT_NE(gpu_frame, compositor_->GetCurrentFrame());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebMediaPlayerMSTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));
}  // namespace blink
