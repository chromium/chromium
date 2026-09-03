// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/page/prerender_page_param.mojom.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_renderer.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_source.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

namespace {

class MockAudioTransport : public webrtc::AudioTransport {
 public:
  MockAudioTransport() = default;

  MockAudioTransport(const MockAudioTransport&) = delete;
  MockAudioTransport& operator=(const MockAudioTransport&) = delete;

  MOCK_METHOD10(RecordedDataIsAvailable,
                int32_t(const void* audioSamples,
                        size_t nSamples,
                        size_t nBytesPerSample,
                        size_t nChannels,
                        uint32_t samplesPerSec,
                        uint32_t totalDelayMS,
                        int32_t clockDrift,
                        uint32_t currentMicLevel,
                        bool keyPressed,
                        uint32_t& newMicLevel));

  MOCK_METHOD8(NeedMorePlayData,
               int32_t(size_t nSamples,
                       size_t nBytesPerSample,
                       size_t nChannels,
                       uint32_t samplesPerSec,
                       void* audioSamples,
                       size_t& nSamplesOut,
                       int64_t* elapsed_time_ms,
                       int64_t* ntp_time_ms));

  MOCK_METHOD7(PullRenderData,
               void(int bits_per_sample,
                    int sample_rate,
                    size_t number_of_channels,
                    size_t number_of_frames,
                    void* audio_data,
                    int64_t* elapsed_time_ms,
                    int64_t* ntp_time_ms));
};

const int kHardwareSampleRate = 44100;
const int kHardwareBufferSize = 512;

const media::AudioParameters kAudioParameters =
    media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                           media::ChannelLayoutConfig::Stereo(),
                           kHardwareSampleRate,
                           kHardwareBufferSize);

}  // namespace

class WebRtcAudioDeviceImplTest : public testing::Test {
 public:
  WebRtcAudioDeviceImplTest()
      : audio_transport_(new MockAudioTransport()),
        audio_device_(
            new webrtc::RefCountedObject<blink::WebRtcAudioDeviceImpl>()) {
    audio_device_module()->Init();
    audio_device_module()->RegisterAudioCallback(audio_transport_.get());
  }

  ~WebRtcAudioDeviceImplTest() override { audio_device_module()->Terminate(); }

 protected:
  webrtc::AudioDeviceModule* audio_device_module() {
    return static_cast<webrtc::AudioDeviceModule*>(audio_device_.get());
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockAudioTransport> audio_transport_;
  scoped_refptr<blink::WebRtcAudioDeviceImpl> audio_device_;
};

// Verify that stats are accumulated during calls to RenderData and are
// available through GetStats().
TEST_F(WebRtcAudioDeviceImplTest, GetStats) {
  auto audio_bus = media::AudioBus::Create(kAudioParameters);
  int sample_rate = kAudioParameters.sample_rate();
  auto audio_delay = base::Seconds(1);
  base::TimeDelta current_time;
  media::AudioGlitchInfo glitch_info;
  glitch_info.duration = base::Seconds(2);
  glitch_info.count = 3;

  for (int i = 0; i < 10; i++) {
    webrtc::AudioDeviceModule::Stats stats = *audio_device_->GetStats();
    EXPECT_EQ(stats.synthesized_samples_duration_s,
              (base::Seconds(2) * i).InSecondsF());
    EXPECT_EQ(stats.synthesized_samples_events, 3ull * i);
    EXPECT_EQ(stats.total_samples_count,
              static_cast<uint64_t>(audio_bus->frames() * i));
    EXPECT_EQ(stats.total_playout_delay_s,
              (audio_bus->frames() * i * base::Seconds(1)).InSecondsF());
    base::TimeDelta buffer_duration = media::AudioTimestampHelper::FramesToTime(
        audio_bus->frames(), sample_rate);
    base::TimeDelta glitch_duration = glitch_info.duration;
    base::TimeDelta buffer_plus_glitch_duration =
        buffer_duration + glitch_duration;
    EXPECT_EQ(stats.total_samples_duration_s,
              (buffer_plus_glitch_duration * i).InSecondsF());

    audio_device_->RenderData(audio_bus.get(), sample_rate, audio_delay,
                              &current_time, glitch_info);
  }
}

class AudioDeviceFactoryTestingPlatformSupport : public TestingPlatformSupport {
 public:
  scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      blink::WebLocalFrame* web_frame,
      const media::AudioSinkParameters& params) override {
    mock_sink_ = base::MakeRefCounted<media::MockAudioRendererSink>(
        params.device_id, media::OUTPUT_DEVICE_STATUS_OK,
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               media::ChannelLayoutConfig::Stereo(), 44100,
                               512));

    EXPECT_CALL(*mock_sink_.get(), Start());
    return mock_sink_;
  }

  media::MockAudioRendererSink* mock_sink() { return mock_sink_.get(); }

 private:
  scoped_refptr<media::MockAudioRendererSink> mock_sink_;
};

class TestWebRtcAudioRenderer : public blink::WebRtcAudioRenderer {
 public:
  TestWebRtcAudioRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& signaling_thread,
      MediaStreamDescriptor* media_stream_descriptor,
      WebLocalFrame& web_frame,
      const base::UnguessableToken& session_id,
      const String& device_id,
      base::RepeatingCallback<void()> on_render_error_callback,
      base::OnceClosure destructor_callback)
      : WebRtcAudioRenderer(signaling_thread,
                            media_stream_descriptor,
                            web_frame,
                            session_id,
                            device_id,
                            std::move(on_render_error_callback)),
        destructor_callback_(std::move(destructor_callback)) {}

 private:
  ~TestWebRtcAudioRenderer() override {
    destructor_thread_id_ = base::PlatformThread::CurrentId();
    if (destructor_callback_) {
      std::move(destructor_callback_).Run();
    }
  }

 public:
  static base::PlatformThreadId destructor_thread_id_;
  base::OnceClosure destructor_callback_;
};

base::PlatformThreadId TestWebRtcAudioRenderer::destructor_thread_id_ =
    base::kInvalidThreadId;

class WebRtcAudioDeviceImplReleaseTest : public WebRtcAudioDeviceImplTest {
 public:
  WebRtcAudioDeviceImplReleaseTest() {
    agent_group_scheduler_ =
        std::make_unique<blink::scheduler::WebAgentGroupScheduler>(
            ThreadScheduler::Current()
                ->ToMainThreadScheduler()
                ->CreateAgentGroupScheduler());

    web_view_ = blink::WebView::Create(
        /*client=*/nullptr,
        /*is_hidden=*/false,
        /*prerender_param=*/nullptr,
        /*fenced_frame_mode=*/std::nullopt,
        /*compositing_enabled=*/false,
        /*widgets_never_composited=*/false,
        /*opener=*/nullptr, mojo::NullAssociatedReceiver(),
        *agent_group_scheduler_,
        /*session_storage_namespace_id=*/std::string(),
        /*page_base_background_color=*/std::nullopt,
        /*browsing_context_group_token=*/base::UnguessableToken::Create(),
        /*color_provider_colors=*/nullptr,
        /*history_index=*/-1,
        /*history_length=*/0);

    web_local_frame_ = blink::WebLocalFrame::CreateMainFrame(
        web_view_, &web_local_frame_client_, nullptr, mojo::NullRemote(),
        LocalFrameToken(), DocumentToken(), InitiatorStateToken(),
        /*policy_container=*/nullptr);

    MediaStreamComponentVector dummy_components;
    stream_descriptor_ = MakeGarbageCollected<MediaStreamDescriptor>(
        "new stream", dummy_components, dummy_components);

    TestWebRtcAudioRenderer::destructor_thread_id_ = base::kInvalidThreadId;

    renderer_ = base::MakeRefCounted<TestWebRtcAudioRenderer>(
        scheduler::GetSingleThreadTaskRunnerForTesting(), stream_descriptor_,
        *web_local_frame_, base::UnguessableToken::Create(), "",
        base::RepeatingCallback<void()>(), run_loop_.QuitClosure());

    test_audio_device_ = new webrtc::RefCountedObject<WebRtcAudioDeviceImpl>();
    EXPECT_TRUE(test_audio_device_->SetAudioRenderer(renderer_.get()));

    worker_thread_ = std::make_unique<base::Thread>("WebRTC_Worker");
    worker_thread_->Start();

    base::RunLoop init_loop;
    worker_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](WebRtcAudioDeviceImpl* ADM, base::RunLoop* loop) {
                         static_cast<webrtc::AudioDeviceModule*>(ADM)->Init();
                         loop->Quit();
                       },
                       base::Unretained(test_audio_device_.get()),
                       base::Unretained(&init_loop)));
    init_loop.Run();
  }

  ~WebRtcAudioDeviceImplReleaseTest() override {
    test_audio_device_ = nullptr;
    web_local_frame_ = nullptr;
    if (web_view_) {
      web_view_.ExtractAsDangling()->Close();
    }
    blink::WebHeap::CollectAllGarbageForTesting();
    worker_thread_->Stop();
  }

  void TerminateADMOnWorkerThread() {
    base::RunLoop terminate_loop;
    worker_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](WebRtcAudioDeviceImpl* ADM, base::RunLoop* loop) {
              static_cast<webrtc::AudioDeviceModule*>(ADM)->Terminate();
              loop->Quit();
            },
            base::Unretained(test_audio_device_.get()),
            base::Unretained(&terminate_loop)));
    terminate_loop.Run();
  }

  void StartStopUnderlyingRenderer() {
    MediaStreamAudioRenderer* underlying_renderer =
        static_cast<MediaStreamAudioRenderer*>(renderer_.get());
    underlying_renderer->Start();
    underlying_renderer->Stop();
  }

  void DetachFrame() { web_local_frame_->Detach(); }
  void ReleaseAudioDevice() { test_audio_device_ = nullptr; }
  void ReleaseRenderer() { renderer_ = nullptr; }
  void RunLoop() { run_loop_.Run(); }

 private:
  std::unique_ptr<blink::scheduler::WebAgentGroupScheduler>
      agent_group_scheduler_;
  WebLocalFrameClient web_local_frame_client_;
  raw_ptr<WebView> web_view_;
  raw_ptr<WebLocalFrame> web_local_frame_;
  ScopedTestingPlatformSupport<AudioDeviceFactoryTestingPlatformSupport>
      platform_support_;
  Persistent<MediaStreamDescriptor> stream_descriptor_;
  base::RunLoop run_loop_;
  scoped_refptr<TestWebRtcAudioRenderer> renderer_;
  scoped_refptr<WebRtcAudioDeviceImpl> test_audio_device_;
  std::unique_ptr<base::Thread> worker_thread_;
};

TEST_F(WebRtcAudioDeviceImplReleaseTest, ReleaseRendererSoonWithShutDownQueue) {
  base::PlatformThreadId main_thread_id = base::PlatformThread::CurrentId();

  // Detach the frame to shut down its task queues.
  DetachFrame();

  // Terminate() bounces the renderer reference to the main thread. With the
  // frame queue shut down, ReleaseSoon must successfully fall back to the
  // default main thread task runner to ensure safe destruction.
  TerminateADMOnWorkerThread();

  // Destroy the ADM to simulate full teardown.
  ReleaseAudioDevice();

  // Transition renderer state to kUninitialized to satisfy destructor DCHECKs.
  StartStopUnderlyingRenderer();

  // Release the local reference so ReleaseSoon holds the sole remaining ref.
  ReleaseRenderer();

  // Execute the posted destruction task.
  RunLoop();

  EXPECT_EQ(TestWebRtcAudioRenderer::destructor_thread_id_, main_thread_id);
}

TEST_F(WebRtcAudioDeviceImplReleaseTest, TerminateReleasesRenderer) {
  base::PlatformThreadId main_thread_id = base::PlatformThread::CurrentId();

  // Terminate() must move renderer_ out of the ADM and post it via ReleaseSoon.
  TerminateADMOnWorkerThread();

  // Transition renderer state to kUninitialized to satisfy destructor DCHECKs.
  StartStopUnderlyingRenderer();

  // Release the local reference. The ADM is intentionally kept alive.
  ReleaseRenderer();

  // Run the loop. If Terminate() correctly cleared renderer_ via std::move,
  // ReleaseSoon will drop the final reference and invoke the destructor.
  // Otherwise, the ADM would retain a reference and this loop would hang.
  RunLoop();

  EXPECT_EQ(TestWebRtcAudioRenderer::destructor_thread_id_, main_thread_id);
}

}  // namespace blink
