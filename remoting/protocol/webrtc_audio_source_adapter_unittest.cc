// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/protocol/webrtc_audio_source_adapter.h"

#include <numeric>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/fake_audio_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/rtc_base/ref_count.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace remoting::protocol {

namespace {

const int kSampleRate = 48000;
const int kBytesPerSample = 2;
const int kChannels = 2;
constexpr auto kFrameDuration = base::Milliseconds(10);

class FakeAudioSink : public webrtc::AudioTrackSinkInterface {
 public:
  FakeAudioSink() = default;
  ~FakeAudioSink() override = default;

  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_samples) override {
    EXPECT_EQ(kSampleRate, sample_rate);
    EXPECT_EQ(kBytesPerSample * 8, bits_per_sample);
    EXPECT_EQ(kChannels, static_cast<int>(number_of_channels));
    EXPECT_EQ((kSampleRate * kFrameDuration).InSeconds(),
              static_cast<int>(number_of_samples));
    const int16_t* samples = reinterpret_cast<const int16_t*>(audio_data);
    samples_.insert(samples_.end(), samples,
                    samples + number_of_samples * kChannels);
  }

  const std::vector<int16_t>& samples() { return samples_; }

 private:
  std::vector<int16_t> samples_;
};

}  // namespace

class WebrtcAudioSourceAdapterTest : public testing::Test {
 public:
  void SetUp() override {
    audio_source_adapter_ = new rtc::RefCountedObject<WebrtcAudioSourceAdapter>(
        task_environment_.GetMainThreadTaskRunner());
    audio_source_ = new FakeAudioSource();
    audio_source_adapter_->Start(base::WrapUnique(audio_source_.get()));
    audio_source_adapter_->AddSink(&sink_);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    audio_source_adapter_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<FakeAudioSource, AcrossTasksDanglingUntriaged> audio_source_;
  scoped_refptr<WebrtcAudioSourceAdapter> audio_source_adapter_;
  FakeAudioSink sink_;
};

TEST_F(WebrtcAudioSourceAdapterTest, PartialFrames) {
  int16_t sample_value = 1;
  std::vector<int> frame_sizes_ms = {10, 12, 18, 2, 5, 7, 55, 13, 8};
  for (int frame_size_ms : frame_sizes_ms) {
    int num_samples = frame_size_ms * kSampleRate / 1000;
    std::vector<int16_t> data(num_samples * kChannels);
    for (int i = 0; i < num_samples; ++i) {
      data[i * kChannels] = sample_value;
      data[i * kChannels + 1] = -sample_value;
      ++sample_value;
    }

    std::unique_ptr<AudioPacket> packet(new AudioPacket());
    packet->add_data(reinterpret_cast<char*>(&(data[0])),
                     num_samples * kChannels * sizeof(int16_t));
    packet->set_encoding(AudioPacket::ENCODING_RAW);
    packet->set_sampling_rate(AudioPacket::SAMPLING_RATE_48000);
    packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
    packet->set_channels(AudioPacket::CHANNELS_STEREO);
    audio_source_->callback().Run(std::move(packet));
  }

  int total_length_ms =
      std::accumulate(frame_sizes_ms.begin(), frame_sizes_ms.end(), 0,
                      [](int sum, int x) { return sum + x; });
  const std::vector<int16_t>& received = sink_.samples();
  int total_samples = total_length_ms * kSampleRate / 1000;
  ASSERT_EQ(total_samples * kChannels, static_cast<int>(received.size()));
  sample_value = 1;
  for (int i = 0; i < total_samples; ++i) {
    ASSERT_EQ(sample_value, received[i * kChannels]) << i;
    ASSERT_EQ(-sample_value, received[i * kChannels + 1]);
    ++sample_value;
  }
}

}  // namespace remoting::protocol
