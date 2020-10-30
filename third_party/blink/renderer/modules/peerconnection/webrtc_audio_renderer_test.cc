// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_renderer.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/audio/audio_source_parameters.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_renderer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_source.h"
#include "third_party/webrtc/api/media_stream_interface.h"

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;

namespace blink {

namespace {

const int kHardwareSampleRate = 44100;
const int kHardwareBufferSize = 512;
const char kDefaultOutputDeviceId[] = "";
const char kOtherOutputDeviceId[] = "other-output-device";
const char kInvalidOutputDeviceId[] = "invalid-device";

class MockAudioRendererSource : public blink::WebRtcAudioRendererSource {
 public:
  MockAudioRendererSource() {}
  ~MockAudioRendererSource() override {}
  MOCK_METHOD4(RenderData,
               void(media::AudioBus* audio_bus,
                    int sample_rate,
                    int audio_delay_milliseconds,
                    base::TimeDelta* current_time));
  MOCK_METHOD1(RemoveAudioRenderer, void(blink::WebRtcAudioRenderer* renderer));
  MOCK_METHOD0(AudioRendererThreadStopped, void());
  MOCK_METHOD1(SetOutputDeviceForAec, void(const String&));
  MOCK_CONST_METHOD0(GetAudioProcessingId, base::UnguessableToken());
};

// Mock blink::Platform implementation needed for creating
// media::AudioRendererSink instances.
//
// TODO(crbug.com/704136): Remove this class once this test is Onion souped
// (which is blocked on Onion souping AudioDeviceFactory).
//
// TODO(crbug.com/704136): When this test gets Onion soup'ed, consider
// factorying this class out of it into its own reusable helper file.
// The class could inherit from TestingPlatformSupport and use
// ScopedTestingPlatformSupport.
class AudioDeviceFactoryTestingPlatformSupport : public blink::Platform {
 public:
  scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      blink::WebLocalFrame* web_frame,
      const media::AudioSinkParameters& params) override {
    MockNewAudioRendererSink(source_type, web_frame, params);

    mock_sink_ = new media::MockAudioRendererSink(
        params.device_id,
        params.device_id == kInvalidOutputDeviceId
            ? media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL
            : media::OUTPUT_DEVICE_STATUS_OK,
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               media::CHANNEL_LAYOUT_STEREO,
                               kHardwareSampleRate, kHardwareBufferSize));

    if (params.device_id != kInvalidOutputDeviceId) {
      EXPECT_CALL(*mock_sink_.get(), Start());
      EXPECT_CALL(*mock_sink_.get(), Play());
    } else {
      EXPECT_CALL(*mock_sink_.get(), Stop());
    }

    return mock_sink_;
  }

  MOCK_METHOD3(MockNewAudioRendererSink,
               void(blink::WebAudioDeviceSourceType,
                    blink::WebLocalFrame*,
                    const media::AudioSinkParameters&));

  media::MockAudioRendererSink* mock_sink() { return mock_sink_.get(); }

 private:
  scoped_refptr<media::MockAudioRendererSink> mock_sink_;
};

}  // namespace

// Flaky on TSAN. See https://crbug.com/1127211
#if defined(THREAD_SANITIZER)
#define MAYBE_WebRtcAudioRendererTest DISABLED_WebRtcAudioRendererTest
#else
#define MAYBE_WebRtcAudioRendererTest WebRtcAudioRendererTest
#endif
class MAYBE_WebRtcAudioRendererTest : public testing::Test {
 public:
  MOCK_METHOD1(MockSwitchDeviceCallback, void(media::OutputDeviceStatus));
  void SwitchDeviceCallback(base::RunLoop* loop,
                            media::OutputDeviceStatus result) {
    MockSwitchDeviceCallback(result);
    loop->Quit();
  }

 protected:
  MAYBE_WebRtcAudioRendererTest()
      : source_(new MockAudioRendererSource())
// Tests crash on Android if these are defined. https://crbug.com/1119689
#if !defined(OS_ANDROID)
        ,
        agent_group_scheduler_(
            blink::scheduler::WebThreadScheduler::MainThreadScheduler()
                ->CreateAgentGroupScheduler()),
        web_view_(blink::WebView::Create(/*client=*/nullptr,
                                         /*is_hidden=*/false,
                                         /*is_inside_portal=*/false,
                                         /*compositing_enabled=*/false,
                                         /*opener=*/nullptr,
                                         mojo::NullAssociatedReceiver(),
                                         *agent_group_scheduler_)),
        web_local_frame_(blink::WebLocalFrame::CreateMainFrame(
            web_view_,
            &web_local_frame_client_,
            nullptr,
            base::UnguessableToken::Create(),
            /*policy_container=*/nullptr))
#endif
  {
    MediaStreamSourceVector dummy_components;
    stream_descriptor_ = MakeGarbageCollected<MediaStreamDescriptor>(
        String::FromUTF8("new stream"), dummy_components, dummy_components);
    EXPECT_CALL(*source_.get(), GetAudioProcessingId())
        .WillRepeatedly(Return(*kAudioProcessingId));
  }

  void SetupRenderer(const String& device_id) {
    renderer_ = new blink::WebRtcAudioRenderer(
        scheduler::GetSingleThreadTaskRunnerForTesting(), stream_descriptor_,
        web_local_frame_, base::UnguessableToken::Create(), device_id,
        base::RepeatingCallback<void()>());

    media::AudioSinkParameters params;
    EXPECT_CALL(
        *audio_device_factory_platform_,
        MockNewAudioRendererSink(blink::WebAudioDeviceSourceType::kWebRtc,
                                 web_local_frame_, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(DoAll(SaveArg<2>(&params), InvokeWithoutArgs([&]() {
                                EXPECT_EQ(params.device_id, device_id.Utf8());
                              })));

    EXPECT_CALL(*source_.get(), SetOutputDeviceForAec(device_id));
    EXPECT_TRUE(renderer_->Initialize(source_.get()));

    renderer_proxy_ =
        renderer_->CreateSharedAudioRendererProxy(stream_descriptor_);
  }
  MOCK_METHOD2(CreateAudioCapturerSource,
               scoped_refptr<media::AudioCapturerSource>(
                   int,
                   const media::AudioSourceParameters&));
  MOCK_METHOD3(
      CreateFinalAudioRendererSink,
      scoped_refptr<media::AudioRendererSink>(int,
                                              const media::AudioSinkParameters&,
                                              base::TimeDelta));
  MOCK_METHOD3(CreateSwitchableAudioRendererSink,
               scoped_refptr<media::SwitchableAudioRendererSink>(
                   blink::WebAudioDeviceSourceType,
                   int,
                   const media::AudioSinkParameters&));
  MOCK_METHOD5(MockCreateAudioRendererSink,
               void(blink::WebAudioDeviceSourceType,
                    int,
                    const base::UnguessableToken&,
                    const std::string&,
                    const base::Optional<base::UnguessableToken>&));

  media::MockAudioRendererSink* mock_sink() {
    return audio_device_factory_platform_->mock_sink();
  }

  void TearDown() override {
    renderer_proxy_ = nullptr;
    renderer_ = nullptr;
    stream_descriptor_ = nullptr;
    source_.reset();
    agent_group_scheduler_ = nullptr;
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  blink::ScopedTestingPlatformSupport<AudioDeviceFactoryTestingPlatformSupport>
      audio_device_factory_platform_;
  const base::Optional<base::UnguessableToken> kAudioProcessingId =
      base::UnguessableToken::Create();
  std::unique_ptr<MockAudioRendererSource> source_;
  Persistent<MediaStreamDescriptor> stream_descriptor_;
  std::unique_ptr<blink::scheduler::WebAgentGroupScheduler>
      agent_group_scheduler_;
  WebView* web_view_ = nullptr;
  WebLocalFrameClient web_local_frame_client_;
  WebLocalFrame* web_local_frame_ = nullptr;
  scoped_refptr<blink::WebRtcAudioRenderer> renderer_;
  scoped_refptr<blink::WebMediaStreamAudioRenderer> renderer_proxy_;
};

// Verify that the renderer will be stopped if the only proxy is stopped.
TEST_F(MAYBE_WebRtcAudioRendererTest, StopRenderer) {
  SetupRenderer(kDefaultOutputDeviceId);
  renderer_proxy_->Start();

  // |renderer_| has only one proxy, stopping the proxy should stop the sink of
  // |renderer_|.
  EXPECT_CALL(*mock_sink(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

// Verify that the renderer will not be stopped unless the last proxy is
// stopped.
TEST_F(MAYBE_WebRtcAudioRendererTest, MultipleRenderers) {
  SetupRenderer(kDefaultOutputDeviceId);
  renderer_proxy_->Start();

  // Create a vector of renderer proxies from the |renderer_|.
  std::vector<scoped_refptr<blink::WebMediaStreamAudioRenderer>>
      renderer_proxies_;
  static const int kNumberOfRendererProxy = 5;
  for (int i = 0; i < kNumberOfRendererProxy; ++i) {
    scoped_refptr<blink::WebMediaStreamAudioRenderer> renderer_proxy(
        renderer_->CreateSharedAudioRendererProxy(stream_descriptor_));
    renderer_proxy->Start();
    renderer_proxies_.push_back(renderer_proxy);
  }

  // Stop the |renderer_proxy_| should not stop the sink since it is used by
  // other proxies.
  EXPECT_CALL(*mock_sink(), Stop()).Times(0);
  renderer_proxy_->Stop();

  for (int i = 0; i < kNumberOfRendererProxy; ++i) {
    if (i != kNumberOfRendererProxy - 1) {
      EXPECT_CALL(*mock_sink(), Stop()).Times(0);
    } else {
      // When the last proxy is stopped, the sink will stop.
      EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
      EXPECT_CALL(*mock_sink(), Stop());
    }
    renderer_proxies_[i]->Stop();
  }
}

// Verify that the sink of the renderer is using the expected sample rate and
// buffer size.
TEST_F(MAYBE_WebRtcAudioRendererTest, VerifySinkParameters) {
  SetupRenderer(kDefaultOutputDeviceId);
  renderer_proxy_->Start();
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_FUCHSIA)
  static const int kExpectedBufferSize = kHardwareSampleRate / 100;
#elif defined(OS_ANDROID)
  static const int kExpectedBufferSize = 2 * kHardwareSampleRate / 100;
#elif defined(OS_WIN)
  static const int kExpectedBufferSize = kHardwareBufferSize;
#else
#error Unknown platform.
#endif
  EXPECT_EQ(kExpectedBufferSize, renderer_->frames_per_buffer());
  EXPECT_EQ(kHardwareSampleRate, renderer_->sample_rate());
  EXPECT_EQ(2, renderer_->channels());

  EXPECT_CALL(*mock_sink(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

TEST_F(MAYBE_WebRtcAudioRendererTest, NonDefaultDevice) {
  SetupRenderer(kDefaultOutputDeviceId);
  EXPECT_EQ(kDefaultOutputDeviceId,
            mock_sink()->GetOutputDeviceInfo().device_id());
  renderer_proxy_->Start();

  EXPECT_CALL(*mock_sink(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();

  SetupRenderer(kOtherOutputDeviceId);
  EXPECT_EQ(kOtherOutputDeviceId,
            mock_sink()->GetOutputDeviceInfo().device_id());
  renderer_proxy_->Start();

  EXPECT_CALL(*mock_sink(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

TEST_F(MAYBE_WebRtcAudioRendererTest, SwitchOutputDevice) {
  SetupRenderer(kDefaultOutputDeviceId);
  EXPECT_EQ(kDefaultOutputDeviceId,
            mock_sink()->GetOutputDeviceInfo().device_id());
  renderer_proxy_->Start();

  EXPECT_CALL(*mock_sink(), Stop());

  media::AudioSinkParameters params;
  EXPECT_CALL(
      *audio_device_factory_platform_,
      MockNewAudioRendererSink(blink::WebAudioDeviceSourceType::kWebRtc, _, _))
      .WillOnce(SaveArg<2>(&params));
  EXPECT_CALL(*source_.get(), AudioRendererThreadStopped());
  EXPECT_CALL(*source_.get(),
              SetOutputDeviceForAec(String::FromUTF8(kOtherOutputDeviceId)));
  EXPECT_CALL(*this, MockSwitchDeviceCallback(media::OUTPUT_DEVICE_STATUS_OK));
  base::RunLoop loop;
  renderer_proxy_->SwitchOutputDevice(
      kOtherOutputDeviceId,
      base::BindOnce(&MAYBE_WebRtcAudioRendererTest::SwitchDeviceCallback,
                     base::Unretained(this), &loop));
  loop.Run();
  EXPECT_EQ(kOtherOutputDeviceId,
            mock_sink()->GetOutputDeviceInfo().device_id());

  // blink::Platform::NewAudioRendererSink should have been called by now.
  EXPECT_EQ(params.device_id, kOtherOutputDeviceId);
  EXPECT_EQ(params.processing_id, kAudioProcessingId);

  EXPECT_CALL(*mock_sink(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

TEST_F(MAYBE_WebRtcAudioRendererTest, SwitchOutputDeviceInvalidDevice) {
  SetupRenderer(kDefaultOutputDeviceId);
  EXPECT_EQ(kDefaultOutputDeviceId,
            mock_sink()->GetOutputDeviceInfo().device_id());
  auto* original_sink = mock_sink();
  renderer_proxy_->Start();

  media::AudioSinkParameters params;
  EXPECT_CALL(
      *audio_device_factory_platform_,
      MockNewAudioRendererSink(blink::WebAudioDeviceSourceType::kWebRtc, _, _))
      .WillOnce(SaveArg<2>(&params));
  EXPECT_CALL(*this, MockSwitchDeviceCallback(
                         media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL));
  base::RunLoop loop;
  renderer_proxy_->SwitchOutputDevice(
      kInvalidOutputDeviceId,
      base::BindOnce(&MAYBE_WebRtcAudioRendererTest::SwitchDeviceCallback,
                     base::Unretained(this), &loop));
  loop.Run();
  EXPECT_EQ(kDefaultOutputDeviceId,
            original_sink->GetOutputDeviceInfo().device_id());

  // blink::Platform::NewAudioRendererSink should have been called by now.
  EXPECT_EQ(params.device_id, kInvalidOutputDeviceId);
  EXPECT_EQ(params.processing_id, kAudioProcessingId);

  EXPECT_CALL(*original_sink, Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

TEST_F(MAYBE_WebRtcAudioRendererTest, InitializeWithInvalidDevice) {
  renderer_ = new blink::WebRtcAudioRenderer(
      scheduler::GetSingleThreadTaskRunnerForTesting(), stream_descriptor_,
      nullptr /*blink::WebLocalFrame*/, base::UnguessableToken::Create(),
      kInvalidOutputDeviceId, base::RepeatingCallback<void()>());

  media::AudioSinkParameters params;
  EXPECT_CALL(
      *audio_device_factory_platform_,
      MockNewAudioRendererSink(blink::WebAudioDeviceSourceType::kWebRtc, _, _))
      .WillOnce(SaveArg<2>(&params));

  EXPECT_FALSE(renderer_->Initialize(source_.get()));

  // blink::Platform::NewAudioRendererSink should have been called by now.
  EXPECT_EQ(params.device_id, kInvalidOutputDeviceId);
  EXPECT_EQ(params.processing_id, kAudioProcessingId);

  renderer_proxy_ =
      renderer_->CreateSharedAudioRendererProxy(stream_descriptor_);

  EXPECT_EQ(kInvalidOutputDeviceId,
            mock_sink()->GetOutputDeviceInfo().device_id());
}

TEST_F(MAYBE_WebRtcAudioRendererTest, SwitchOutputDeviceStoppedSource) {
  SetupRenderer(kDefaultOutputDeviceId);
  auto* original_sink = mock_sink();
  renderer_proxy_->Start();

  EXPECT_CALL(*original_sink, Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  EXPECT_CALL(*this, MockSwitchDeviceCallback(
                         media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL));
  base::RunLoop loop;
  renderer_proxy_->Stop();
  renderer_proxy_->SwitchOutputDevice(
      kInvalidOutputDeviceId,
      base::BindOnce(&MAYBE_WebRtcAudioRendererTest::SwitchDeviceCallback,
                     base::Unretained(this), &loop));
  loop.Run();
}

}  // namespace blink
