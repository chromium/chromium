// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/testing_platform_support_with_mock_audio_capture_source.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::WithArg;

namespace blink {

namespace {

// Audio parameters for the VerifyAudioFlowWithoutAudioProcessing test.
constexpr int kSampleRate = 48000;
constexpr media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
constexpr int kRequestedBufferSize = 512;

// On Android, ProcessedLocalAudioSource forces a 20ms buffer size from the
// input device.
#if defined(OS_ANDROID)
constexpr int kExpectedSourceBufferSize = kSampleRate / 50;
#else
constexpr int kExpectedSourceBufferSize = kRequestedBufferSize;
#endif

// On both platforms, even though audio processing is turned off, the
// MediaStreamAudioProcessor will force the use of 10ms buffer sizes on the
// output end of its FIFO.
constexpr int kExpectedOutputBufferSize = kSampleRate / 100;

class FormatCheckingMockAudioSink : public WebMediaStreamAudioSink {
 public:
  FormatCheckingMockAudioSink() = default;
  ~FormatCheckingMockAudioSink() override = default;

  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks estimated_capture_time) override {
    EXPECT_EQ(audio_bus.channels(), params_.channels());
    EXPECT_EQ(audio_bus.frames(), params_.frames_per_buffer());
    EXPECT_FALSE(estimated_capture_time.is_null());
    OnDataCallback();
  }
  MOCK_METHOD0(OnDataCallback, void());

  void OnSetFormat(const media::AudioParameters& params) override {
    params_ = params;
    FormatIsSet(params_);
  }
  MOCK_METHOD1(FormatIsSet, void(const media::AudioParameters& params));

 private:
  media::AudioParameters params_;
};

}  // namespace

class ProcessedLocalAudioSourceTest : public SimTest {
 protected:
  ProcessedLocalAudioSourceTest() = default;

  ~ProcessedLocalAudioSourceTest() override = default;

  void SetUp() override {
    SimTest::SetUp();
    audio_source_ = MakeGarbageCollected<MediaStreamSource>(
        String::FromUTF8("audio_label"), MediaStreamSource::kTypeAudio,
        String::FromUTF8("audio_track"), false /* remote */);
    audio_component_ = MakeGarbageCollected<MediaStreamComponent>(
        audio_source_->Id(), audio_source_);
  }

  void TearDown() override {
    SimTest::TearDown();
    audio_source_ = nullptr;
    audio_component_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  void CreateProcessedLocalAudioSource(
      const AudioProcessingProperties& properties,
      int num_requested_channels) {
    std::unique_ptr<blink::ProcessedLocalAudioSource> source =
        std::make_unique<blink::ProcessedLocalAudioSource>(
            *MainFrame().GetFrame(),
            MediaStreamDevice(mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                              "mock_audio_device_id", "Mock audio device",
                              kSampleRate, kChannelLayout,
                              kRequestedBufferSize),
            false /* disable_local_echo */, properties, num_requested_channels,
            base::DoNothing(),
            scheduler::GetSingleThreadTaskRunnerForTesting());
    source->SetAllowInvalidRenderFrameIdForTesting(true);
    audio_source_->SetPlatformSource(std::move(source));
  }

  void CheckSourceFormatMatches(const media::AudioParameters& params) {
    EXPECT_EQ(kSampleRate, params.sample_rate());
    EXPECT_EQ(kChannelLayout, params.channel_layout());
    EXPECT_EQ(kExpectedSourceBufferSize, params.frames_per_buffer());
  }

  void CheckOutputFormatMatches(const media::AudioParameters& params) {
    EXPECT_EQ(kSampleRate, params.sample_rate());
    EXPECT_EQ(kChannelLayout, params.channel_layout());
    EXPECT_EQ(kExpectedOutputBufferSize, params.frames_per_buffer());
  }

  media::AudioCapturerSource::CaptureCallback* capture_source_callback() const {
    return static_cast<media::AudioCapturerSource::CaptureCallback*>(
        ProcessedLocalAudioSource::From(audio_source()));
  }

  MediaStreamAudioSource* audio_source() const {
    return MediaStreamAudioSource::From(audio_source_.Get());
  }

  MediaStreamComponent* audio_track() { return audio_component_; }

  MockAudioCapturerSource* mock_audio_capturer_source() {
    return webrtc_audio_device_platform_support_->mock_audio_capturer_source();
  }

 private:
  ScopedTestingPlatformSupport<AudioCapturerSourceTestingPlatformSupport>
      webrtc_audio_device_platform_support_;
  Persistent<MediaStreamSource> audio_source_;
  Persistent<MediaStreamComponent> audio_component_;
};

// Tests a basic end-to-end start-up, track+sink connections, audio flow, and
// shut-down. The unit tests in media_stream_audio_unittest.cc provide more
// comprehensive testing of the object graph connections and multi-threading
// concerns.
TEST_F(ProcessedLocalAudioSourceTest, VerifyAudioFlowWithoutAudioProcessing) {
  using ThisTest =
      ProcessedLocalAudioSourceTest_VerifyAudioFlowWithoutAudioProcessing_Test;

  // Turn off the default constraints so the sink will get audio in chunks of
  // the native buffer size.
  AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  CreateProcessedLocalAudioSource(properties, 1 /* num_requested_channels */);

  // Connect the track, and expect the MockAudioCapturerSource to be initialized
  // and started by ProcessedLocalAudioSource.
  EXPECT_CALL(*mock_audio_capturer_source(),
              Initialize(_, capture_source_callback()))
      .WillOnce(WithArg<0>(Invoke(this, &ThisTest::CheckSourceFormatMatches)));
  EXPECT_CALL(*mock_audio_capturer_source(), SetAutomaticGainControl(true));
  EXPECT_CALL(*mock_audio_capturer_source(), Start())
      .WillOnce(Invoke(
          capture_source_callback(),
          &media::AudioCapturerSource::CaptureCallback::OnCaptureStarted));
  ASSERT_TRUE(audio_source()->ConnectToTrack(audio_track()));
  CheckOutputFormatMatches(audio_source()->GetAudioParameters());

  // Connect a sink to the track.
  auto sink = std::make_unique<FormatCheckingMockAudioSink>();
  EXPECT_CALL(*sink, FormatIsSet(_))
      .WillOnce(Invoke(this, &ThisTest::CheckOutputFormatMatches));
  MediaStreamAudioTrack::From(audio_track())->AddSink(sink.get());

  // Feed audio data into the ProcessedLocalAudioSource and expect it to reach
  // the sink.
  int delay_ms = 65;
  bool key_pressed = true;
  double volume = 0.9;
  const base::TimeTicks capture_time =
      base::TimeTicks::Now() + base::Milliseconds(delay_ms);
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(2, kExpectedSourceBufferSize);
  audio_bus->Zero();
  EXPECT_CALL(*sink, OnDataCallback()).Times(AtLeast(1));
  capture_source_callback()->Capture(audio_bus.get(), capture_time, volume,
                                     key_pressed);

  // Expect the ProcessedLocalAudioSource to auto-stop the MockCapturerSource
  // when the track is stopped.
  EXPECT_CALL(*mock_audio_capturer_source(), Stop());
  MediaStreamAudioTrack::From(audio_track())->Stop();
}

}  // namespace blink
