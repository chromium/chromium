// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/audio_context.h"

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "media/base/audio_timestamp_helper.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom-blink.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_sink_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiocontextlatencycategory_double.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/sub_capture_target.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/webaudio/audio_playout_stats.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

constexpr char kFakeAudioOutput1[] = "fake_audio_output_1";
constexpr char kFakeAudioOutput2[] = "fake_audio_output_2";
constexpr char kInvalidAudioOutput[] = "INVALID_AUDIO_OUTPUT";
constexpr char kSecurityOrigin[] = "https://example.com";
constexpr char kTestData[] = "simple_div.html";
constexpr char kDefaultDeviceId[] = "";

bool web_audio_device_paused_;

class MockMediaDevicesDispatcherHost final
    : public mojom::blink::MediaDevicesDispatcherHost {
 public:
  MockMediaDevicesDispatcherHost()
      : enumeration_(
            {{},
             {},
             {
                 {kFakeAudioOutput1, "Fake Audio Output 1", "common_group_1"},
                 {kFakeAudioOutput2, "Fake Audio Output 2", "common_group_2"},
             }}) {}

  ~MockMediaDevicesDispatcherHost() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(
        this, mojo::PendingReceiver<mojom::blink::MediaDevicesDispatcherHost>(
                  std::move(handle)));
  }

  void Flush() {
    receivers_.FlushForTesting();
    listeners_.FlushForTesting();
  }

  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        bool request_video_input_capabilities,
                        bool request_audio_input_capabilities,
                        EnumerateDevicesCallback callback) override {
    Vector<Vector<WebMediaDeviceInfo>> enumeration(static_cast<size_t>(
        blink::mojom::blink::MediaDeviceType::kNumMediaDeviceTypes));
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities;
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities;
    if (request_audio_output) {
      wtf_size_t index = static_cast<wtf_size_t>(
          blink::mojom::blink::MediaDeviceType::kMediaAudioOutput);
      enumeration[index] = enumeration_[index];
    }
    std::move(callback).Run(std::move(enumeration),
                            std::move(video_input_capabilities),
                            std::move(audio_input_capabilities));
  }

  void GetVideoInputCapabilities(GetVideoInputCapabilitiesCallback) override {}

  void GetAllVideoInputDeviceFormats(
      const String&,
      GetAllVideoInputDeviceFormatsCallback) override {}

  void GetAvailableVideoInputDeviceFormats(
      const String&,
      GetAvailableVideoInputDeviceFormatsCallback) override {}

  void GetAudioInputCapabilities(GetAudioInputCapabilitiesCallback) override {}

  void AddMediaDevicesListener(
      bool subscribe_audio_input,
      bool subscribe_video_input,
      bool subscribe_audio_output,
      mojo::PendingRemote<mojom::blink::MediaDevicesListener> listener)
      override {
    listeners_.Add(std::move(listener));
  }

  void SetCaptureHandleConfig(
      mojom::blink::CaptureHandleConfigPtr config) override {}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void CloseFocusWindowOfOpportunity(const String& label) override {}

  void ProduceSubCaptureTargetId(
      SubCaptureTarget::Type type,
      ProduceSubCaptureTargetIdCallback callback) override {}
#endif

 private:
  mojo::RemoteSet<mojom::blink::MediaDevicesListener> listeners_;
  mojo::ReceiverSet<mojom::blink::MediaDevicesDispatcherHost> receivers_;

  Vector<Vector<WebMediaDeviceInfo>> enumeration_{static_cast<size_t>(
      blink::mojom::blink::MediaDeviceType::kNumMediaDeviceTypes)};
};

class MockWebAudioDeviceForAudioContext : public WebAudioDevice {
 public:
  explicit MockWebAudioDeviceForAudioContext(double sample_rate,
                                             int frames_per_buffer)
      : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}
  ~MockWebAudioDeviceForAudioContext() override = default;

  void Start() override {}
  void Stop() override {}
  void Pause() override { web_audio_device_paused_ = true; }
  void Resume() override { web_audio_device_paused_ = false; }
  double SampleRate() override { return sample_rate_; }
  int FramesPerBuffer() override { return frames_per_buffer_; }
  int MaxChannelCount() override { return 2; }
  void SetDetectSilence(bool detect_silence) override {}
  media::OutputDeviceStatus MaybeCreateSinkAndGetStatus() override {
    // In this test, we assume the sink creation always succeeds.
    return media::OUTPUT_DEVICE_STATUS_OK;
  }

 private:
  double sample_rate_;
  int frames_per_buffer_;
};

class AudioContextTestPlatform : public TestingPlatformSupport {
 public:
  std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      const WebAudioSinkDescriptor& sink_descriptor,
      unsigned number_of_output_channels,
      const WebAudioLatencyHint& latency_hint,
      media::AudioRendererSink::RenderCallback*) override {
    double buffer_size = 0;
    const double interactive_size = AudioHardwareBufferSize();
    const double balanced_size = AudioHardwareBufferSize() * 2;
    const double playback_size = AudioHardwareBufferSize() * 4;
    switch (latency_hint.Category()) {
      case WebAudioLatencyHint::kCategoryInteractive:
        buffer_size = interactive_size;
        break;
      case WebAudioLatencyHint::kCategoryBalanced:
        buffer_size = balanced_size;
        break;
      case WebAudioLatencyHint::kCategoryPlayback:
        buffer_size = playback_size;
        break;
      case WebAudioLatencyHint::kCategoryExact:
        buffer_size =
            ClampTo(latency_hint.Seconds() * AudioHardwareSampleRate(),
                    static_cast<double>(AudioHardwareBufferSize()),
                    static_cast<double>(playback_size));
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    return std::make_unique<MockWebAudioDeviceForAudioContext>(
        AudioHardwareSampleRate(), buffer_size);
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 128; }
};

String GetAecDevice(ExecutionContext* execution_context) {
  return PeerConnectionDependencyFactory::From(*execution_context)
      .GetWebRtcAudioDevice()
      ->GetOutputDeviceForAecForTesting();
}

}  // namespace

class AudioContextTest : public PageTestBase {
 protected:
  AudioContextTest() {
    mock_media_devices_dispatcher_host_ =
        std::make_unique<MockMediaDevicesDispatcherHost>();
  }

  ~AudioContextTest() override = default;

  void FlushMediaDevicesDispatcherHost() {
    mock_media_devices_dispatcher_host_->Flush();
  }

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    CoreInitializer::GetInstance().ProvideModulesToPage(GetPage(),
                                                        std::string());

    GetFrame().DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::MediaDevicesDispatcherHost::Name_,
        WTF::BindRepeating(
            &MockMediaDevicesDispatcherHost::BindRequest,
            WTF::Unretained(mock_media_devices_dispatcher_host_.get())));
  }

  void TearDown() override {
    GetFrame().DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::MediaDevicesDispatcherHost::Name_, {});
  }

  void ResetAudioContextManagerForAudioContext(AudioContext* audio_context) {
    audio_context->audio_context_manager_.reset();
  }

  void SetContextState(AudioContext* audio_context,
                       AudioContext::AudioContextState state) {
    audio_context->SetContextState(state);
  }

  AudioContextTestPlatform* platform() {
    return platform_.GetTestingPlatformSupport();
  }

  void VerifyPlayoutStats(AudioPlayoutStats* playout_stats,
                          ScriptState* script_state,
                          int total_processed_frames,
                          const media::AudioGlitchInfo& total_glitches,
                          base::TimeDelta average_delay,
                          base::TimeDelta min_delay,
                          base::TimeDelta max_delay,
                          int source_line) {
    EXPECT_EQ(playout_stats->fallbackFramesEvents(script_state),
              total_glitches.count)
        << " LINE " << source_line;
    EXPECT_FLOAT_EQ(playout_stats->fallbackFramesDuration(script_state),
                    total_glitches.duration.InMillisecondsF())
        << " LINE " << source_line;
    EXPECT_EQ(playout_stats->averageLatency(script_state),
              average_delay.InMillisecondsF())
        << " LINE " << source_line;
    EXPECT_EQ(playout_stats->minimumLatency(script_state),
              min_delay.InMillisecondsF())
        << " LINE " << source_line;
    EXPECT_EQ(playout_stats->maximumLatency(script_state),
              max_delay.InMillisecondsF())
        << " LINE " << source_line;
    EXPECT_NEAR(
        playout_stats->totalFramesDuration(script_state),
        (media::AudioTimestampHelper::FramesToTime(
             total_processed_frames, platform()->AudioHardwareSampleRate()) +
         total_glitches.duration)
            .InMillisecondsF(),
        0.01)
        << " LINE " << source_line;
  }

 private:
  ScopedTestingPlatformSupport<AudioContextTestPlatform> platform_;
  std::unique_ptr<MockMediaDevicesDispatcherHost>
      mock_media_devices_dispatcher_host_;
};

TEST_F(AudioContextTest, AudioContextOptions_WebAudioLatencyHint) {
  AudioContextOptions* interactive_options = AudioContextOptions::Create();
  interactive_options->setLatencyHint(
      MakeGarbageCollected<V8UnionAudioContextLatencyCategoryOrDouble>(
          V8AudioContextLatencyCategory(
              V8AudioContextLatencyCategory::Enum::kInteractive)));
  AudioContext* interactive_context = AudioContext::Create(
      GetFrame().DomWindow(), interactive_options, ASSERT_NO_EXCEPTION);

  AudioContextOptions* balanced_options = AudioContextOptions::Create();
  balanced_options->setLatencyHint(
      MakeGarbageCollected<V8UnionAudioContextLatencyCategoryOrDouble>(
          V8AudioContextLatencyCategory(
              V8AudioContextLatencyCategory::Enum::kBalanced)));
  AudioContext* balanced_context = AudioContext::Create(
      GetFrame().DomWindow(), balanced_options, ASSERT_NO_EXCEPTION);
  EXPECT_GT(balanced_context->baseLatency(),
            interactive_context->baseLatency());

  AudioContextOptions* playback_options = AudioContextOptions::Create();
  playback_options->setLatencyHint(
      MakeGarbageCollected<V8UnionAudioContextLatencyCategoryOrDouble>(
          V8AudioContextLatencyCategory(
              V8AudioContextLatencyCategory::Enum::kPlayback)));
  AudioContext* playback_context = AudioContext::Create(
      GetFrame().DomWindow(), playback_options, ASSERT_NO_EXCEPTION);
  EXPECT_GT(playback_context->baseLatency(), balanced_context->baseLatency());

  AudioContextOptions* exact_too_small_options = AudioContextOptions::Create();
  exact_too_small_options->setLatencyHint(
      MakeGarbageCollected<V8UnionAudioContextLatencyCategoryOrDouble>(
          interactive_context->baseLatency() / 2));
  AudioContext* exact_too_small_context = AudioContext::Create(
      GetFrame().DomWindow(), exact_too_small_options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(exact_too_small_context->baseLatency(),
            interactive_context->baseLatency());

  const double exact_latency_sec =
      (interactive_context->baseLatency() + playback_context->baseLatency()) /
      2;
  AudioContextOptions* exact_ok_options = AudioContextOptions::Create();
  exact_ok_options->setLatencyHint(
      MakeGarbageCollected<V8UnionAudioContextLatencyCategoryOrDouble>(
          exact_latency_sec));
  AudioContext* exact_ok_context = AudioContext::Create(
      GetFrame().DomWindow(), exact_ok_options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(exact_ok_context->baseLatency(), exact_latency_sec);

  AudioContextOptions* exact_too_big_options = AudioContextOptions::Create();
  exact_too_big_options->setLatencyHint(
      MakeGarbageCollected<V8UnionAudioContextLatencyCategoryOrDouble>(
          playback_context->baseLatency() * 2));
  AudioContext* exact_too_big_context = AudioContext::Create(
      GetFrame().DomWindow(), exact_too_big_options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(exact_too_big_context->baseLatency(),
            playback_context->baseLatency());
}

TEST_F(AudioContextTest, AudioContextAudibility_ServiceUnbind) {
  AudioContextOptions* options = AudioContextOptions::Create();
  AudioContext* audio_context = AudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);

  audio_context->set_was_audible_for_testing(true);
  ResetAudioContextManagerForAudioContext(audio_context);
  SetContextState(audio_context, AudioContext::AudioContextState::kSuspended);

  platform()->RunUntilIdle();
}

TEST_F(AudioContextTest, ExecutionContextPaused) {
  AudioContextOptions* options = AudioContextOptions::Create();
  AudioContext* audio_context = AudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);

  audio_context->set_was_audible_for_testing(true);
  EXPECT_FALSE(web_audio_device_paused_);
  GetFrame().DomWindow()->SetLifecycleState(
      mojom::FrameLifecycleState::kFrozen);
  EXPECT_TRUE(web_audio_device_paused_);
  GetFrame().DomWindow()->SetLifecycleState(
      mojom::FrameLifecycleState::kRunning);
  EXPECT_FALSE(web_audio_device_paused_);
}

// Test initialization/uninitialization of MediaDeviceService.
TEST_F(AudioContextTest, MediaDevicesService) {
  AudioContextOptions* options = AudioContextOptions::Create();
  AudioContext* audio_context = AudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);

  EXPECT_FALSE(audio_context->is_media_device_service_initialized_);
  audio_context->InitializeMediaDeviceService();
  EXPECT_TRUE(audio_context->is_media_device_service_initialized_);
  audio_context->UninitializeMediaDeviceService();
  EXPECT_FALSE(audio_context->media_device_service_.is_bound());
  EXPECT_FALSE(audio_context->media_device_service_receiver_.is_bound());
}

TEST_F(AudioContextTest, OnRenderErrorFromPlatformDestination) {
  AudioContextOptions* options = AudioContextOptions::Create();
  AudioContext* audio_context = AudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(audio_context->ContextState(),
            AudioContext::AudioContextState::kRunning);

  audio_context->invoke_onrendererror_from_platform_for_testing();
  EXPECT_TRUE(audio_context->render_error_occurred_);
}

class ContextRenderer : public GarbageCollected<ContextRenderer> {
 public:
  explicit ContextRenderer(AudioContext* context)
      : context_(context),
        audio_thread_(NonMainThread::CreateThread(
            ThreadCreationParams(ThreadType::kRealtimeAudioWorkletThread))) {}
  ~ContextRenderer() = default;

  void Init() {
    PostCrossThreadTask(
        *audio_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&ContextRenderer::SetContextAudioThread,
                            WrapCrossThreadWeakPersistent(this)));
    event_.Wait();
  }

  void Render(uint32_t frames_to_process,
              base::TimeDelta playout_delay,
              const media::AudioGlitchInfo& glitch_info) {
    PostCrossThreadTask(
        *audio_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&ContextRenderer::RenderOnAudioThread,
                            WrapCrossThreadWeakPersistent(this),
                            frames_to_process, playout_delay, glitch_info));
    event_.Wait();
  }

  void Trace(Visitor* visitor) const { visitor->Trace(context_); }

 private:
  void SetContextAudioThread() {
    static_cast<AudioContext*>(context_)
        ->GetDeferredTaskHandler()
        .SetAudioThreadToCurrentThread();
    event_.Signal();
  }

  void RenderOnAudioThread(uint32_t frames_to_process,
                           base::TimeDelta playout_delay,
                           const media::AudioGlitchInfo& glitch_info) {
    const AudioIOPosition output_position{0, 0, 0};
    const AudioCallbackMetric audio_callback_metric;
    static_cast<AudioContext*>(context_)->HandlePreRenderTasks(
        frames_to_process, &output_position, &audio_callback_metric,
        playout_delay, glitch_info);
    event_.Signal();
  }

  WeakMember<AudioContext> context_;
  const std::unique_ptr<blink::NonMainThread> audio_thread_;
  base::WaitableEvent event_{base::WaitableEvent::ResetPolicy::AUTOMATIC};
};

TEST_F(AudioContextTest, PlayoutStats) {
  blink::WebRuntimeFeatures::EnableFeatureFromString("AudioContextPlayoutStats",
                                                     true);
  AudioContextOptions* options = AudioContextOptions::Create();
  AudioContext* audio_context = AudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);

  const int kNumberOfRenderEvents = 9;
  uint32_t frames_to_process[kNumberOfRenderEvents]{100, 200, 300, 10, 500,
                                                    120, 120, 30,  100};
  base::TimeDelta playout_delay[kNumberOfRenderEvents]{
      base::Milliseconds(10),  base::Milliseconds(20), base::Milliseconds(300),
      base::Milliseconds(107), base::Milliseconds(17), base::Milliseconds(3),
      base::Milliseconds(500), base::Milliseconds(10), base::Milliseconds(112)};
  const media::AudioGlitchInfo glitch_info[kNumberOfRenderEvents]{
      {.duration = base::Milliseconds(5), .count = 1},
      {},
      {.duration = base::Milliseconds(60), .count = 3},
      {},
      {.duration = base::Milliseconds(600), .count = 20},
      {.duration = base::Milliseconds(200), .count = 5},
      {},
      {.duration = base::Milliseconds(2), .count = 1},
      {.duration = base::Milliseconds(15), .count = 5}};

  media::AudioGlitchInfo total_glitches;
  int total_processed_frames = 0;
  int interval_processed_frames = 0;
  base::TimeDelta interval_delay_sum;
  base::TimeDelta last_delay;
  base::TimeDelta max_delay;
  base::TimeDelta min_delay = base::TimeDelta::Max();

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  AudioPlayoutStats* playout_stats = audio_context->playoutStats();

  ContextRenderer* renderer =
      MakeGarbageCollected<ContextRenderer>(audio_context);
  renderer->Init();

  // Empty stats in be beginning, all latencies are zero.
  VerifyPlayoutStats(playout_stats, script_state, total_processed_frames,
                     total_glitches, last_delay, last_delay, last_delay,
                     __LINE__);

  int i = 0;
  for (; i < 3; ++i) {
    // Do some rendering.
    renderer->Render(frames_to_process[i], playout_delay[i], glitch_info[i]);

    total_glitches += glitch_info[i];
    last_delay = playout_delay[i];
    total_processed_frames += frames_to_process[i];
    interval_processed_frames += frames_to_process[i];
    interval_delay_sum += playout_delay[i] * frames_to_process[i];
    max_delay = std::max<base::TimeDelta>(max_delay, playout_delay[i]);
    min_delay = std::min<base::TimeDelta>(min_delay, playout_delay[i]);

    // New execution cycle.
    ToEventLoop(script_state).PerformMicrotaskCheckpoint();

    // Stats updated.
    VerifyPlayoutStats(playout_stats, script_state, total_processed_frames,
                       total_glitches,
                       interval_delay_sum / interval_processed_frames,
                       min_delay, max_delay, __LINE__);
  }

  // Same stats, since we are within the same execution cycle.
  VerifyPlayoutStats(playout_stats, script_state, total_processed_frames,
                     total_glitches,
                     interval_delay_sum / interval_processed_frames, min_delay,
                     max_delay, __LINE__);

  // Reset stats.
  playout_stats->resetLatency(script_state);

  min_delay = base::TimeDelta::Max();
  max_delay = base::TimeDelta();
  interval_processed_frames = 0;
  interval_delay_sum = base::TimeDelta();

  // Getting reset stats.
  VerifyPlayoutStats(playout_stats, script_state, total_processed_frames,
                     total_glitches, last_delay, last_delay, last_delay,
                     __LINE__);

  // New execution cycle.
  ToEventLoop(script_state).PerformMicrotaskCheckpoint();

  // Stats are still the same, since there have been no rendering yet.
  VerifyPlayoutStats(playout_stats, script_state, total_processed_frames,
                     total_glitches, last_delay, last_delay, last_delay,
                     __LINE__);

  for (; i < 4; ++i) {
    // Do some rendering after reset.
    renderer->Render(frames_to_process[i], playout_delay[i], glitch_info[i]);

    total_glitches += glitch_info[i];
    last_delay = playout_delay[i];
    total_processed_frames += frames_to_process[i];
    interval_processed_frames += frames_to_process[i];
    interval_delay_sum += playout_delay[i] * frames_to_process[i];
    max_delay = std::max<base::TimeDelta>(max_delay, playout_delay[i]);
    min_delay = std::min<base::TimeDelta>(min_delay, playout_delay[i]);

    // New execution cycle.
    ToEventLoop(script_state).PerformMicrotaskCheckpoint();

    // Stats reflect the state after the last reset.
    VerifyPlayoutStats(playout_stats, script_state, total_processed_frames,
                       total_glitches,
                       interval_delay_sum / interval_processed_frames,
                       min_delay, max_delay, __LINE__);
  }

  // Cache the current state: we'll be doing rendering several times without
  // advancing to the next execution cycle.
  const media::AudioGlitchInfo observed_total_glitches = total_glitches;
  const int observed_total_processed_frames = total_processed_frames;
  const base::TimeDelta observed_average_delay =
      interval_delay_sum / interval_processed_frames;
  const base::TimeDelta observed_max_delay = max_delay;
  const base::TimeDelta observed_min_delay = min_delay;

  VerifyPlayoutStats(playout_stats, script_state,
                     observed_total_processed_frames, observed_total_glitches,
                     observed_average_delay, observed_min_delay,
                     observed_max_delay, __LINE__);

  // Starting the execution cycle.
  ToEventLoop(script_state).PerformMicrotaskCheckpoint();

  // Still same stats: there has been no new rendering.
  VerifyPlayoutStats(playout_stats, script_state,
                     observed_total_processed_frames, observed_total_glitches,
                     observed_average_delay, observed_min_delay,
                     observed_max_delay, __LINE__);

  for (; i < 8; ++i) {
    // Render.
    renderer->Render(frames_to_process[i], playout_delay[i], glitch_info[i]);

    // Still same stats: we are in the same execution cycle.
    VerifyPlayoutStats(playout_stats, script_state,
                       observed_total_processed_frames, observed_total_glitches,
                       observed_average_delay, observed_min_delay,
                       observed_max_delay, __LINE__);

    total_glitches += glitch_info[i];
    last_delay = playout_delay[i];
    total_processed_frames += frames_to_process[i];
    interval_processed_frames += frames_to_process[i];
    interval_delay_sum += playout_delay[i] * frames_to_process[i];
    max_delay = std::max<base::TimeDelta>(max_delay, playout_delay[i]);
    min_delay = std::min<base::TimeDelta>(min_delay, playout_delay[i]);
  }

  // New execution cycle.
  ToEventLoop(script_state).PerformMicrotaskCheckpoint();

  // Stats are updated with all the new info.
  VerifyPlayoutStats(playout_stats, script_state, total_processed_frames,
                     total_glitches,
                     interval_delay_sum / interval_processed_frames, min_delay,
                     max_delay, __LINE__);

  // Reset stats.
  playout_stats->resetLatency(script_state);

  // Cache the current state: we'll be doing rendering several times without
  // advancing to the next execution cycle.
  const media::AudioGlitchInfo reset_total_glitches = total_glitches;
  const int reset_total_processed_frames = total_processed_frames;
  const base::TimeDelta reset_average_delay = last_delay;
  const base::TimeDelta reset_max_delay = last_delay;
  const base::TimeDelta reset_min_delay = last_delay;

  // Still same stats: we are in the same execution cycle.
  VerifyPlayoutStats(playout_stats, script_state, reset_total_processed_frames,
                     reset_total_glitches, reset_average_delay, reset_min_delay,
                     reset_max_delay, __LINE__);

  min_delay = base::TimeDelta::Max();
  max_delay = base::TimeDelta();
  interval_processed_frames = 0;
  interval_delay_sum = base::TimeDelta();

  // Render while in the same execution cycle.
  for (; i < kNumberOfRenderEvents; ++i) {
    renderer->Render(frames_to_process[i], playout_delay[i], glitch_info[i]);

    // Still same stats we got after reset: we are in the same execution cycle.
    VerifyPlayoutStats(playout_stats, script_state,
                       reset_total_processed_frames, reset_total_glitches,
                       reset_average_delay, reset_min_delay, reset_max_delay,
                       __LINE__);

    total_glitches += glitch_info[i];
    last_delay = playout_delay[i];
    total_processed_frames += frames_to_process[i];
    interval_processed_frames += frames_to_process[i];
    interval_delay_sum += playout_delay[i] * frames_to_process[i];
    max_delay = std::max<base::TimeDelta>(max_delay, playout_delay[i]);
    min_delay = std::min<base::TimeDelta>(min_delay, playout_delay[i]);
  }

  // New execution cycle.
  ToEventLoop(script_state).PerformMicrotaskCheckpoint();

  // In the new execution cycle stats have all the info received after the last
  // reset.
  VerifyPlayoutStats(playout_stats, script_state, total_processed_frames,
                     total_glitches,
                     interval_delay_sum / interval_processed_frames, min_delay,
                     max_delay, __LINE__);
}

TEST_F(AudioContextTest, ChannelCountRunning) {
  // Changing the channel count on a running AudioContext should result in a
  // running context and running platform destination.
  test::ScopedMockedURLLoad scoped_mocked_url_load(
      KURL(kSecurityOrigin), test::CoreTestDataPath(kTestData));
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(kSecurityOrigin);
  LocalFrame* main_frame = web_view_impl->MainFrameImpl()->GetFrame();
  ScriptState* script_state = ToScriptStateForMainWorld(main_frame);
  ScriptState::Scope scope(script_state);
  ExecutionContext* execution_context = main_frame->DomWindow();
  SecurityContext& security_context = execution_context->GetSecurityContext();
  security_context.SetSecurityOriginForTesting(nullptr);
  security_context.SetSecurityOrigin(
      SecurityOrigin::CreateFromString(kSecurityOrigin));

  // Creating an AudioContext should result in the context running and the
  // destination playing.
  AudioContext* context = AudioContext::Create(
      execution_context, AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kRunning);
  EXPECT_TRUE(context->GetRealtimeAudioDestinationNode()
                  ->GetOwnHandler()
                  .get_platform_destination_is_playing_for_testing());

  // Changing the channel count should should result in the same running and
  // playing state.
  context->destination()->setChannelCount(
      context->destination()->maxChannelCount(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kRunning);
  EXPECT_TRUE(context->GetRealtimeAudioDestinationNode()
                  ->GetOwnHandler()
                  .get_platform_destination_is_playing_for_testing());
}

TEST_F(AudioContextTest, ChannelCountSuspended) {
  // Changing the channel count on suspended AudioContexts should not cause the
  // destination to start.
  test::ScopedMockedURLLoad scoped_mocked_url_load(
      KURL(kSecurityOrigin), test::CoreTestDataPath(kTestData));
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(kSecurityOrigin);
  LocalFrame* main_frame = web_view_impl->MainFrameImpl()->GetFrame();
  ScriptState* script_state = ToScriptStateForMainWorld(main_frame);
  ScriptState::Scope scope(script_state);
  ExecutionContext* execution_context = main_frame->DomWindow();
  SecurityContext& security_context = execution_context->GetSecurityContext();
  security_context.SetSecurityOriginForTesting(nullptr);
  security_context.SetSecurityOrigin(
      SecurityOrigin::CreateFromString(kSecurityOrigin));

  // Creating an AudioContext should result in the context running and the
  // destination playing.
  AudioContext* context = AudioContext::Create(
      execution_context, AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kRunning);
  EXPECT_TRUE(context->GetRealtimeAudioDestinationNode()
                  ->GetOwnHandler()
                  .get_platform_destination_is_playing_for_testing());

  // Suspending the AudioContext should result in the context being suspended
  // and the destination not playing.
  context->suspendContext(script_state, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kSuspended);
  EXPECT_FALSE(context->GetRealtimeAudioDestinationNode()
                   ->GetOwnHandler()
                   .get_platform_destination_is_playing_for_testing());

  // Changing the channel count on a suspended context should not change the
  // suspended or playing states.
  context->destination()->setChannelCount(
      context->destination()->maxChannelCount(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kSuspended);
  EXPECT_FALSE(context->GetRealtimeAudioDestinationNode()
                   ->GetOwnHandler()
                   .get_platform_destination_is_playing_for_testing());

  // Resuming the context should make everything start playing again.
  context->resumeContext(script_state, ASSERT_NO_EXCEPTION);
  ContextRenderer* renderer = MakeGarbageCollected<ContextRenderer>(context);
  renderer->Init();
  renderer->Render(128, base::Milliseconds(0), {});
  platform()->RunUntilIdle();
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kRunning);
  EXPECT_TRUE(context->GetRealtimeAudioDestinationNode()
                  ->GetOwnHandler()
                  .get_platform_destination_is_playing_for_testing());
}

TEST_F(AudioContextTest, SetSinkIdRunning) {
  // Calling setSinkId on a running AudioContext should result in a running
  // context and running platform destination.
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);
  ExecutionContext* execution_context = GetFrame().DomWindow();
  SecurityContext& security_context = execution_context->GetSecurityContext();
  security_context.SetSecurityOriginForTesting(nullptr);
  security_context.SetSecurityOrigin(
      SecurityOrigin::CreateFromString(kSecurityOrigin));

  // Creating an AudioContext should result in the context running and the
  // destination playing.
  AudioContext* context = AudioContext::Create(
      execution_context, AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kRunning);
  EXPECT_TRUE(context->GetRealtimeAudioDestinationNode()
                  ->GetOwnHandler()
                  .get_platform_destination_is_playing_for_testing());

  // Calling setSinkId with a valid device ID should result in the same running
  // and playing state.
  context->setSinkId(
      script_state,
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kFakeAudioOutput1),
      ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kRunning);
  EXPECT_TRUE(context->GetRealtimeAudioDestinationNode()
                  ->GetOwnHandler()
                  .get_platform_destination_is_playing_for_testing());
}

TEST_F(AudioContextTest, SetSinkIdSuspended) {
  // Calling setSinkId on suspended AudioContexts should not cause the
  // destination to start.
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);
  ExecutionContext* execution_context = GetFrame().DomWindow();
  SecurityContext& security_context = execution_context->GetSecurityContext();
  security_context.SetSecurityOriginForTesting(nullptr);
  security_context.SetSecurityOrigin(
      SecurityOrigin::CreateFromString(kSecurityOrigin));

  // Creating an AudioContext should result in the context running and the
  // destination playing.
  AudioContext* context = AudioContext::Create(
      execution_context, AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kRunning);
  EXPECT_TRUE(context->GetRealtimeAudioDestinationNode()
                  ->GetOwnHandler()
                  .get_platform_destination_is_playing_for_testing());

  // Suspending the AudioContext should result in the context being suspended
  // and the destination not playing.
  context->suspendContext(script_state, ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kSuspended);
  EXPECT_FALSE(context->GetRealtimeAudioDestinationNode()
                   ->GetOwnHandler()
                   .get_platform_destination_is_playing_for_testing());

  // Calling setSinkId with an invalid device ID on a suspended context should
  // not change the suspended or playing states.
  context->setSinkId(script_state,
                     MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(
                         kInvalidAudioOutput),
                     ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kSuspended);
  EXPECT_FALSE(context->GetRealtimeAudioDestinationNode()
                   ->GetOwnHandler()
                   .get_platform_destination_is_playing_for_testing());

  // Calling setSinkId with a valid device ID on a suspended context should not
  // change the suspended or playing states.
  context->setSinkId(
      script_state,
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kFakeAudioOutput1),
      ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kSuspended);
  EXPECT_FALSE(context->GetRealtimeAudioDestinationNode()
                   ->GetOwnHandler()
                   .get_platform_destination_is_playing_for_testing());

  // Resuming the context should make everything start playing again.
  context->resumeContext(script_state, ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  ContextRenderer* renderer = MakeGarbageCollected<ContextRenderer>(context);
  renderer->Init();
  renderer->Render(128, base::Milliseconds(0), {});
  platform()->RunUntilIdle();
  EXPECT_EQ(context->ContextState(), BaseAudioContext::kRunning);
  EXPECT_TRUE(context->GetRealtimeAudioDestinationNode()
                  ->GetOwnHandler()
                  .get_platform_destination_is_playing_for_testing());
}

TEST_F(AudioContextTest, AecConstructor) {
  // Constructing AudioContexts with different sinkId values should update the
  // acoustic echo cancellation output device.
  ExecutionContext* execution_context = GetFrame().DomWindow();
  SecurityContext& security_context = execution_context->GetSecurityContext();
  security_context.SetSecurityOriginForTesting(nullptr);
  security_context.SetSecurityOrigin(
      SecurityOrigin::CreateFromString(kSecurityOrigin));

  // Creating an AudioContext with no options should not change the AEC device.
  const String initial_aec_device = GetAecDevice(execution_context);
  AudioContextOptions* options_empty = AudioContextOptions::Create();
  AudioContext::Create(execution_context, options_empty, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(GetAecDevice(execution_context), initial_aec_device);

  // Creating an AudioContext with a null sink should not change the AEC device.
  AudioContextOptions* options_null = AudioContextOptions::Create();
  options_null->setSinkId(MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(
      MakeGarbageCollected<AudioSinkOptions>()));
  AudioContext::Create(execution_context, options_null, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(GetAecDevice(execution_context), initial_aec_device);

  // A specific valid ID should change the AEC device.
  AudioContextOptions* options_a = AudioContextOptions::Create();
  options_a->setSinkId(
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kFakeAudioOutput1));
  AudioContext::Create(execution_context, options_a, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput1);

  // A different specific valid ID on a different AudioContext should change the
  // AEC device again.
  AudioContextOptions* options_b = AudioContextOptions::Create();
  options_b->setSinkId(
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kFakeAudioOutput2));
  AudioContext::Create(execution_context, options_b, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput2);

  // Creating another AudioContext with no options should not change the AEC
  // device.
  AudioContext::Create(execution_context, options_empty, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput2);

  // An explicit default will set the AEC device to default.
  AudioContextOptions* options_explicit_default = AudioContextOptions::Create();
  options_explicit_default->setSinkId(
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kDefaultDeviceId));
  AudioContext::Create(execution_context, options_explicit_default,
                       ASSERT_NO_EXCEPTION);
  EXPECT_EQ(GetAecDevice(execution_context), kDefaultDeviceId);
}

TEST_F(AudioContextTest, AecSetSinkIdSuspended) {
  // Calling setSinkId on suspended AudioContexts should not update the acoustic
  // echo cancellation output device until the contexts are resumed.
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);
  ExecutionContext* execution_context = GetFrame().DomWindow();
  SecurityContext& security_context = execution_context->GetSecurityContext();
  security_context.SetSecurityOriginForTesting(nullptr);
  security_context.SetSecurityOrigin(
      SecurityOrigin::CreateFromString(kSecurityOrigin));

  // Creating AudioContexts with no options should not change the AEC device.
  const String initial_aec_device = GetAecDevice(execution_context);
  AudioContextOptions* options_empty = AudioContextOptions::Create();
  AudioContext* context_a = AudioContext::Create(
      execution_context, options_empty, ASSERT_NO_EXCEPTION);
  context_a->suspendContext(script_state, ASSERT_NO_EXCEPTION);
  AudioContext* context_b = AudioContext::Create(
      execution_context, options_empty, ASSERT_NO_EXCEPTION);
  context_b->suspendContext(script_state, ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), initial_aec_device);

  // Calling setSinkId with a valid device ID on a suspended context should not
  // change the AEC device.
  context_a->setSinkId(
      script_state,
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kFakeAudioOutput1),
      ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), initial_aec_device);

  // Calling setSinkId on a different suspended context with a valid device ID
  // should not change the AEC device.
  context_b->setSinkId(
      script_state,
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kFakeAudioOutput2),
      ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), initial_aec_device);

  // Resuming a suspended AudioContext changes the AEC device.
  context_b->resumeContext(script_state, ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput2);

  // Resuming the other suspended AudioContext should also change the AEC
  // device.
  context_a->resumeContext(script_state, ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput1);

  // Suspending the first audio context should not change the AEC reference
  // again.
  context_b->suspendContext(script_state, ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput1);

  // Resuming the first audio context should not change the AEC reference again.
  context_b->resumeContext(script_state, ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput1);
}

TEST_F(AudioContextTest, AecSetSinkIdMultiple) {
  // Calling setSinkId multiple times on the same AudioContext should update the
  // acoustic echo cancellation output device each time.
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);
  ExecutionContext* execution_context = GetFrame().DomWindow();
  SecurityContext& security_context = execution_context->GetSecurityContext();
  security_context.SetSecurityOriginForTesting(nullptr);
  security_context.SetSecurityOrigin(
      SecurityOrigin::CreateFromString(kSecurityOrigin));

  // Creating an AudioContext with no options should not change the AEC device.
  const String initial_aec_device = GetAecDevice(execution_context);
  AudioContextOptions* options_empty = AudioContextOptions::Create();
  AudioContext* context = AudioContext::Create(execution_context, options_empty,
                                               ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), initial_aec_device);

  // Calling setSinkId with a valid device ID should change the AEC device.
  context->setSinkId(
      script_state,
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kFakeAudioOutput1),
      ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput1);

  // Calling setSinkId with an invalid device ID should not change the AEC
  // device.
  context->setSinkId(script_state,
                     MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(
                         kInvalidAudioOutput),
                     ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput1);

  // Calling setSinkId with another valid device ID on the same context should
  // change the AEC device again.
  context->setSinkId(
      script_state,
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kFakeAudioOutput2),
      ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput2);
}

TEST_F(AudioContextTest, AecSetSinkIdAfterConstructor) {
  // Calling setSinkId after constructing an AudioContext with an explicit
  // device ID should update the acoustic echo cancellation output device each
  // time.
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);
  ExecutionContext* execution_context = GetFrame().DomWindow();
  SecurityContext& security_context = execution_context->GetSecurityContext();
  security_context.SetSecurityOriginForTesting(nullptr);
  security_context.SetSecurityOrigin(
      SecurityOrigin::CreateFromString(kSecurityOrigin));

  // Creating an AudioContext with a specific ID should change the AEC device.
  AudioContextOptions* options = AudioContextOptions::Create();
  options->setSinkId(
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kFakeAudioOutput1));
  AudioContext* context =
      AudioContext::Create(execution_context, options, ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput1);

  // Calling setSinkId with a valid device ID should change the AEC device.
  context->setSinkId(
      script_state,
      MakeGarbageCollected<V8UnionAudioSinkOptionsOrString>(kFakeAudioOutput2),
      ASSERT_NO_EXCEPTION);
  FlushMediaDevicesDispatcherHost();
  EXPECT_EQ(GetAecDevice(execution_context), kFakeAudioOutput2);
}

}  // namespace blink
