// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"

#include <memory>

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
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
      : audio_device_(
            new rtc::RefCountedObject<blink::WebRtcAudioDeviceImpl>()),
        audio_transport_(new MockAudioTransport()) {
    audio_device_module()->Init();
    audio_device_module()->RegisterAudioCallback(audio_transport_.get());
  }

  ~WebRtcAudioDeviceImplTest() override { audio_device_module()->Terminate(); }

 protected:
  webrtc::AudioDeviceModule* audio_device_module() {
    return static_cast<webrtc::AudioDeviceModule*>(audio_device_.get());
  }

  test::TaskEnvironment task_environment_;
  scoped_refptr<blink::WebRtcAudioDeviceImpl> audio_device_;
  std::unique_ptr<MockAudioTransport> audio_transport_;
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
    EXPECT_EQ(stats.total_samples_duration_s,
              (media::AudioTimestampHelper::FramesToTime(audio_bus->frames(),
                                                         sample_rate) *
               i)
                  .InSecondsF());
    audio_device_->RenderData(audio_bus.get(), sample_rate, audio_delay,
                              &current_time, glitch_info);
  }
}

}  // namespace blink
