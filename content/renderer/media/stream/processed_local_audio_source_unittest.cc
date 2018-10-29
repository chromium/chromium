// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/renderer/media/audio/mock_audio_device_factory.h"
#include "content/renderer/media/stream/media_stream_audio_processor_options.h"
#include "content/renderer/media/stream/media_stream_audio_track.h"
#include "content/renderer/media/stream/processed_local_audio_source.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/web/web_heap.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::WithArg;

namespace content {

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

class MockMediaStreamAudioSink : public MediaStreamAudioSink {
 public:
  MockMediaStreamAudioSink() {}
  ~MockMediaStreamAudioSink() override {}

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

class ProcessedLocalAudioSourceTest : public testing::Test {
 protected:
  ProcessedLocalAudioSourceTest() {}

  ~ProcessedLocalAudioSourceTest() override {}

  void SetUp() override {
    blink_audio_source_.Initialize(blink::WebString::FromUTF8("audio_label"),
                                   blink::WebMediaStreamSource::kTypeAudio,
                                   blink::WebString::FromUTF8("audio_track"),
                                   false /* remote */);
    blink_audio_track_.Initialize(blink_audio_source_.Id(),
                                  blink_audio_source_);
  }

  void TearDown() override {
    blink_audio_track_.Reset();
    blink_audio_source_.Reset();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  void CreateProcessedLocalAudioSource(
      const AudioProcessingProperties& properties) {
    ProcessedLocalAudioSource* const source = new ProcessedLocalAudioSource(
        -1 /* consumer_render_frame_id is N/A for non-browser tests */,
        MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, "mock_audio_device_id",
                          "Mock audio device", kSampleRate, kChannelLayout,
                          kRequestedBufferSize),
        false /* hotword_enabled */, false /* disable_local_echo */, properties,
        base::Bind(&ProcessedLocalAudioSourceTest::OnAudioSourceStarted,
                   base::Unretained(this)),
        &mock_dependency_factory_);
    source->SetAllowInvalidRenderFrameIdForTesting(true);
    blink_audio_source_.SetExtraData(source);  // Takes ownership.
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

  MockAudioDeviceFactory* mock_audio_device_factory() {
    return &mock_audio_device_factory_;
  }

  media::AudioCapturerSource::CaptureCallback* capture_source_callback() const {
    return static_cast<media::AudioCapturerSource::CaptureCallback*>(
        ProcessedLocalAudioSource::From(audio_source()));
  }

  MediaStreamAudioSource* audio_source() const {
    return MediaStreamAudioSource::From(blink_audio_source_);
  }

  const blink::WebMediaStreamTrack& blink_audio_track() {
    return blink_audio_track_;
  }

  void OnAudioSourceStarted(MediaStreamSource* source,
                            MediaStreamRequestResult result,
                            const blink::WebString& result_name) {}

 private:
  base::test::ScopedTaskEnvironment
      task_environment_;  // Needed for MSAudioProcessor.
  MockAudioDeviceFactory mock_audio_device_factory_;
  MockPeerConnectionDependencyFactory mock_dependency_factory_;
  blink::WebMediaStreamSource blink_audio_source_;
  blink::WebMediaStreamTrack blink_audio_track_;
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
  CreateProcessedLocalAudioSource(properties);

  // Connect the track, and expect the MockCapturerSource to be initialized and
  // started by ProcessedLocalAudioSource.
  EXPECT_CALL(*mock_audio_device_factory()->mock_capturer_source(),
              Initialize(_, capture_source_callback()))
      .WillOnce(WithArg<0>(Invoke(this, &ThisTest::CheckSourceFormatMatches)));
  EXPECT_CALL(*mock_audio_device_factory()->mock_capturer_source(),
              SetAutomaticGainControl(true));
  EXPECT_CALL(*mock_audio_device_factory()->mock_capturer_source(), Start())
      .WillOnce(Invoke(
          capture_source_callback(),
          &media::AudioCapturerSource::CaptureCallback::OnCaptureStarted));
  ASSERT_TRUE(audio_source()->ConnectToTrack(blink_audio_track()));
  CheckOutputFormatMatches(audio_source()->GetAudioParameters());

  // Connect a sink to the track.
  std::unique_ptr<MockMediaStreamAudioSink> sink(
      new MockMediaStreamAudioSink());
  EXPECT_CALL(*sink, FormatIsSet(_))
      .WillOnce(Invoke(this, &ThisTest::CheckOutputFormatMatches));
  MediaStreamAudioTrack::From(blink_audio_track())->AddSink(sink.get());

  // Feed audio data into the ProcessedLocalAudioSource and expect it to reach
  // the sink.
  int delay_ms = 65;
  bool key_pressed = true;
  double volume = 0.9;
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(2, kExpectedSourceBufferSize);
  audio_bus->Zero();
  EXPECT_CALL(*sink, OnDataCallback()).Times(AtLeast(1));
  capture_source_callback()->Capture(audio_bus.get(), delay_ms, volume,
                                     key_pressed);

  // Expect the ProcessedLocalAudioSource to auto-stop the MockCapturerSource
  // when the track is stopped.
  EXPECT_CALL(*mock_audio_device_factory()->mock_capturer_source(), Stop());
  MediaStreamAudioTrack::From(blink_audio_track())->Stop();
}


}  // namespace content
