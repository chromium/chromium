// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/time/time.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/audio/audio_pcm_encoder.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/opus/src/include/opus.h"

namespace media {

namespace {

constexpr int kAudioSampleRate = 48000;

constexpr base::TimeDelta kBufferDuration =
    base::TimeDelta::FromMilliseconds(10);

// This is the preferred opus buffer duration (60 ms), which corresponds to a
// value of 2880 frames per buffer (|kOpusFramesPerBuffer|).
constexpr base::TimeDelta kOpusBufferDuration =
    base::TimeDelta::FromMilliseconds(60);
constexpr int kOpusFramesPerBuffer = kOpusBufferDuration.InMicroseconds() *
                                     kAudioSampleRate /
                                     base::Time::kMicrosecondsPerSecond;

struct TestAudioParams {
  const media::AudioParameters::Format format;
  const media::ChannelLayout channel_layout;
  const int sample_rate;
};

constexpr TestAudioParams kTestAudioParams[] = {
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::CHANNEL_LAYOUT_STEREO, kAudioSampleRate},
    // Change to mono:
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
     kAudioSampleRate},
    // Different sampling rate as well:
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
     24000},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::CHANNEL_LAYOUT_STEREO, 8000},
    // Using a non-default Opus sampling rate (48, 24, 16, 12, or 8 kHz).
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
     22050},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::CHANNEL_LAYOUT_STEREO, 44100},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::CHANNEL_LAYOUT_STEREO, 96000},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
     kAudioSampleRate},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::CHANNEL_LAYOUT_STEREO, kAudioSampleRate},
};

}  // namespace

class AudioEncodersTest : public ::testing::TestWithParam<TestAudioParams> {
 public:
  AudioEncodersTest()
      : input_params_(GetParam().format,
                      GetParam().channel_layout,
                      GetParam().sample_rate,
                      GetParam().sample_rate / 100),
        audio_source_(input_params_.channels(),
                      /*freq=*/440,
                      input_params_.sample_rate()) {}
  AudioEncodersTest(const AudioEncodersTest&) = delete;
  AudioEncodersTest& operator=(const AudioEncodersTest&) = delete;
  ~AudioEncodersTest() override = default;

  const AudioParameters& input_params() const { return input_params_; }
  const AudioEncoder* encoder() const { return encoder_.get(); }
  int encode_callback_count() const { return encode_callback_count_; }

  void SetEncoder(std::unique_ptr<AudioEncoder> encoder) {
    encoder_ = std::move(encoder);
    encode_callback_count_ = 0;
  }

  // Produces an audio data that corresponds to a |kBufferDuration| and the
  // sample rate of the current |input_params_|. The produced data is send to
  // |encoder_| to be encoded, and the number of frames generated is returned.
  int ProduceAudioAndEncode() {
    DCHECK(encoder_);
    const int num_frames =
        input_params_.sample_rate() * kBufferDuration.InSecondsF();
    current_audio_bus_ =
        media::AudioBus::Create(input_params_.channels(), num_frames);
    const auto capture_time = base::TimeTicks::Now();
    audio_source_.OnMoreData(base::TimeDelta(), capture_time, 0,
                             current_audio_bus_.get());
    encoder_->EncodeAudio(*current_audio_bus_, capture_time);
    return num_frames;
  }

  // Used to verify we get no errors.
  void OnErrorCallback(Status error) { FAIL() << error.message(); }

  // Used as the callback of the PCM encoder.
  void VerifyPcmEncoding(EncodedAudioBuffer output) {
    DCHECK(current_audio_bus_);
    ++encode_callback_count_;
    // Verify that PCM doesn't change the input; i.e. it's just a pass through.
    size_t uncompressed_size = current_audio_bus_->frames() *
                               current_audio_bus_->channels() * sizeof(float);
    ASSERT_EQ(uncompressed_size, output.encoded_data_size);
    std::unique_ptr<uint8_t[]> uncompressed_audio_data(
        new uint8_t[uncompressed_size]);
    current_audio_bus_->ToInterleaved<Float32SampleTypeTraits>(
        current_audio_bus_->frames(),
        reinterpret_cast<float*>(uncompressed_audio_data.get()));
    EXPECT_EQ(std::memcmp(uncompressed_audio_data.get(),
                          output.encoded_data.get(), uncompressed_size),
              0);
  }

  // Used as the callback of the Opus encoder.
  void VerifyOpusEncoding(OpusDecoder* opus_decoder,
                          EncodedAudioBuffer output) {
    DCHECK(current_audio_bus_);
    DCHECK(opus_decoder);

    ++encode_callback_count_;
    // Use the provied |opus_decoder| to decode the |encoded_data| and check we
    // get the expected number of frames per buffer.
    std::vector<float> buffer(kOpusFramesPerBuffer * output.params.channels());
    EXPECT_EQ(kOpusFramesPerBuffer,
              opus_decode_float(opus_decoder, output.encoded_data.get(),
                                output.encoded_data_size, buffer.data(),
                                kOpusFramesPerBuffer, 0));
  }

 private:
  // The input params as initialized from the test's parameter.
  const AudioParameters input_params_;

  // The audio source used to fill in the data of the |current_audio_bus_|.
  media::SineWaveAudioSource audio_source_;

  // The encoder the test is verifying.
  std::unique_ptr<AudioEncoder> encoder_;

  // The audio bus that was most recently generated and sent to the |encoder_|
  // by ProduceAudioAndEncode().
  std::unique_ptr<media::AudioBus> current_audio_bus_;

  // The number of encoder callbacks received.
  int encode_callback_count_ = 0;
};

TEST_P(AudioEncodersTest, PcmEncoder) {
  SetEncoder(std::make_unique<AudioPcmEncoder>(
      input_params(),
      base::BindRepeating(&AudioEncodersTest::VerifyPcmEncoding,
                          base::Unretained(this)),
      base::BindRepeating(&AudioEncodersTest::OnErrorCallback,
                          base::Unretained(this))));

  constexpr int kCount = 6;
  for (int i = 0; i < kCount; ++i)
    ProduceAudioAndEncode();

  EXPECT_EQ(kCount, encode_callback_count());
}

TEST_P(AudioEncodersTest, OpusEncoder) {
  int error;
  OpusDecoder* opus_decoder =
      opus_decoder_create(kAudioSampleRate, input_params().channels(), &error);
  ASSERT_TRUE(error == OPUS_OK && opus_decoder);

  SetEncoder(std::make_unique<AudioOpusEncoder>(
      input_params(),
      base::BindRepeating(&AudioEncodersTest::VerifyOpusEncoding,
                          base::Unretained(this), opus_decoder),
      base::BindRepeating(&AudioEncodersTest::OnErrorCallback,
                          base::Unretained(this)),
      /*opus_bitrate=*/0));

  // The opus encoder encodes in multiple of 60 ms. Wait for the total number of
  // frames that will be generated in 60 ms at the input sampling rate.
  const int frames_in_60_ms =
      kOpusBufferDuration.InSecondsF() * input_params().sample_rate();
  int total_frames = 0;
  while (total_frames < frames_in_60_ms)
    total_frames += ProduceAudioAndEncode();

  EXPECT_EQ(1, encode_callback_count());

  opus_decoder_destroy(opus_decoder);
  opus_decoder = nullptr;
}

INSTANTIATE_TEST_SUITE_P(All,
                         AudioEncodersTest,
                         testing::ValuesIn(kTestAudioParams));

}  // namespace media
