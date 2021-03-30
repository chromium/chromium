// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/opus/src/include/opus.h"

namespace media {

namespace {

constexpr int kAudioSampleRate = 48000;

// This is the preferred opus buffer duration (60 ms), which corresponds to a
// value of 2880 frames per buffer (|kOpusFramesPerBuffer|).
constexpr base::TimeDelta kOpusBufferDuration =
    base::TimeDelta::FromMilliseconds(60);
constexpr int kOpusFramesPerBuffer = kOpusBufferDuration.InMicroseconds() *
                                     kAudioSampleRate /
                                     base::Time::kMicrosecondsPerSecond;

struct TestAudioParams {
  const int channels;
  const int sample_rate;
};

constexpr TestAudioParams kTestAudioParams[] = {
    {2, kAudioSampleRate},
    // Change to mono:
    {1, kAudioSampleRate},
    // Different sampling rate as well:
    {1, 24000},
    {2, 8000},
    // Using a non-default Opus sampling rate (48, 24, 16, 12, or 8 kHz).
    {1, 22050},
    {2, 44100},
    {2, 96000},
    {1, kAudioSampleRate},
    {2, kAudioSampleRate},
};

}  // namespace

class AudioEncodersTest : public ::testing::TestWithParam<TestAudioParams> {
 public:
  AudioEncodersTest()
      : audio_source_(GetParam().channels,
                      /*freq=*/440,
                      GetParam().sample_rate) {
    options_.sample_rate = GetParam().sample_rate;
    options_.channels = GetParam().channels;
  }
  AudioEncodersTest(const AudioEncodersTest&) = delete;
  AudioEncodersTest& operator=(const AudioEncodersTest&) = delete;
  ~AudioEncodersTest() override = default;

  using MaybeDesc = base::Optional<AudioEncoder::CodecDescription>;

  AudioEncoder* encoder() const { return encoder_.get(); }

  void SetupEncoder(AudioEncoder::OutputCB output_cb) {
    encoder_ = std::make_unique<AudioOpusEncoder>();

    bool called_done = false;
    AudioEncoder::StatusCB done_cb =
        base::BindLambdaForTesting([&](Status error) {
          if (!error.is_ok())
            FAIL() << error.message();
          called_done = true;
        });

    encoder_->Initialize(options_, std::move(output_cb), std::move(done_cb));

    RunLoop();
    EXPECT_TRUE(called_done);
  }

  // Produces an audio data that corresponds to a |buffer_duration_| and the
  // sample rate of the current |options_|. The produced data is send to
  // |encoder_| to be encoded, and the number of frames generated is returned.
  int ProduceAudioAndEncode(
      base::TimeTicks timestamp = base::TimeTicks::Now()) {
    DCHECK(encoder_);
    const int num_frames = options_.sample_rate * buffer_duration_.InSecondsF();
    base::TimeTicks capture_time = timestamp + buffer_duration_;
    auto audio_bus = AudioBus::Create(options_.channels, num_frames);
    audio_source_.OnMoreData(base::TimeDelta(), capture_time, 0,
                             audio_bus.get());

    bool called_done = false;
    auto done_cb = base::BindLambdaForTesting([&](Status error) {
      if (!error.is_ok())
        FAIL() << error.message();
      called_done = true;
    });

    encoder_->Encode(std::move(audio_bus), capture_time, std::move(done_cb));
    RunLoop();
    EXPECT_TRUE(called_done);
    return num_frames;
  }

  void RunLoop() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;

  // The input params as initialized from the test's parameter.
  AudioEncoder::Options options_;

  // The audio source used to fill in the data of the |current_audio_bus_|.
  SineWaveAudioSource audio_source_;

  // The encoder the test is verifying.
  std::unique_ptr<AudioEncoder> encoder_;

  // The audio bus that was most recently generated and sent to the |encoder_|
  // by ProduceAudioAndEncode().
  std::unique_ptr<AudioBus> current_audio_bus_;

  base::TimeDelta buffer_duration_ = base::TimeDelta::FromMilliseconds(10);
};

TEST_P(AudioEncodersTest, OpusTimestamps) {
  constexpr int kCount = 12;
  for (base::TimeDelta duration :
       {kOpusBufferDuration * 10, kOpusBufferDuration,
        kOpusBufferDuration * 2 / 3}) {
    buffer_duration_ = duration;
    size_t expected_outputs = (buffer_duration_ * kCount) / kOpusBufferDuration;
    base::TimeTicks current_timestamp;
    std::vector<base::TimeTicks> timestamps;

    auto output_cb =
        base::BindLambdaForTesting([&](EncodedAudioBuffer output, MaybeDesc) {
          timestamps.push_back(output.timestamp);
        });

    SetupEncoder(std::move(output_cb));

    for (int i = 0; i < kCount; ++i) {
      ProduceAudioAndEncode(current_timestamp);
      current_timestamp += buffer_duration_;
    }

    bool flush_done = false;
    auto done_cb = base::BindLambdaForTesting([&](Status error) {
      if (!error.is_ok())
        FAIL() << error.message();
      flush_done = true;
    });
    encoder()->Flush(std::move(done_cb));
    RunLoop();
    EXPECT_TRUE(flush_done);
    EXPECT_EQ(expected_outputs, timestamps.size());

    current_timestamp = base::TimeTicks();
    for (auto& ts : timestamps) {
      EXPECT_EQ(current_timestamp, ts);
      current_timestamp += kOpusBufferDuration;
    }
  }
}

TEST_P(AudioEncodersTest, OpusExtraData) {
  std::vector<uint8_t> extra;
  auto output_cb = base::BindLambdaForTesting(
      [&](EncodedAudioBuffer output, MaybeDesc desc) {
        DCHECK(desc.has_value());
        extra = desc.value();
      });

  SetupEncoder(std::move(output_cb));
  buffer_duration_ = kOpusBufferDuration;
  ProduceAudioAndEncode();
  RunLoop();

  ASSERT_GT(extra.size(), 0u);
  EXPECT_EQ(extra[0], 'O');
  EXPECT_EQ(extra[1], 'p');
  EXPECT_EQ(extra[2], 'u');
  EXPECT_EQ(extra[3], 's');

  uint16_t* sample_rate_ptr = reinterpret_cast<uint16_t*>(extra.data() + 12);
  if (options_.sample_rate < std::numeric_limits<uint16_t>::max())
    EXPECT_EQ(*sample_rate_ptr, options_.sample_rate);
  else
    EXPECT_EQ(*sample_rate_ptr, 48000);

  uint8_t* channels_ptr = reinterpret_cast<uint8_t*>(extra.data() + 9);
  EXPECT_EQ(*channels_ptr, options_.channels);

  uint16_t* skip_ptr = reinterpret_cast<uint16_t*>(extra.data() + 10);
  EXPECT_GT(*skip_ptr, 0);
}

// Check how Opus encoder reacts to breaks in continuity of incoming sound.
// Capture times are expected to be exactly buffer durations apart,
// but the encoder should be ready to handle situations when it's not the case.
TEST_P(AudioEncodersTest, OpusTimeContinuityBreak) {
  base::TimeTicks current_timestamp;
  base::TimeDelta small_gap = base::TimeDelta::FromMicroseconds(500);
  base::TimeDelta large_gap = base::TimeDelta::FromMicroseconds(1500);
  std::vector<base::TimeTicks> timestamps;

  auto output_cb =
      base::BindLambdaForTesting([&](EncodedAudioBuffer output, MaybeDesc) {
        timestamps.push_back(output.timestamp);
      });

  SetupEncoder(std::move(output_cb));

  // Encode first normal buffer and immediately get an output for it.
  buffer_duration_ = kOpusBufferDuration;
  auto ts0 = current_timestamp;
  ProduceAudioAndEncode(current_timestamp);
  current_timestamp += buffer_duration_;
  EXPECT_EQ(1u, timestamps.size());
  EXPECT_EQ(ts0, timestamps[0]);

  // Add another buffer which is too small and will be buffered
  buffer_duration_ = kOpusBufferDuration / 2;
  auto ts1 = current_timestamp;
  ProduceAudioAndEncode(current_timestamp);
  current_timestamp += buffer_duration_;
  EXPECT_EQ(1u, timestamps.size());

  // Add another large buffer after a large gap, 2 outputs are expected
  // because large gap should trigger a flush.
  current_timestamp += large_gap;
  buffer_duration_ = kOpusBufferDuration;
  auto ts2 = current_timestamp;
  ProduceAudioAndEncode(current_timestamp);
  current_timestamp += buffer_duration_;
  EXPECT_EQ(3u, timestamps.size());
  EXPECT_EQ(ts1, timestamps[1]);
  EXPECT_EQ(ts2, timestamps[2]);

  // Add another buffer which is too small and will be buffered
  buffer_duration_ = kOpusBufferDuration / 2;
  auto ts3 = current_timestamp;
  ProduceAudioAndEncode(current_timestamp);
  current_timestamp += buffer_duration_;
  EXPECT_EQ(3u, timestamps.size());

  // Add a small gap and a large buffer, only one output is expected because
  // small gap doesn't trigger a flush.
  // Small gap itself is not counted in output timestamps.
  auto ts4 = current_timestamp + kOpusBufferDuration / 2;
  current_timestamp += small_gap;
  buffer_duration_ = kOpusBufferDuration;
  ProduceAudioAndEncode(current_timestamp);
  EXPECT_EQ(4u, timestamps.size());
  EXPECT_EQ(ts3, timestamps[3]);

  encoder()->Flush(base::BindOnce([](Status error) {
    if (!error.is_ok())
      FAIL() << error.message();
  }));
  RunLoop();
  EXPECT_EQ(5u, timestamps.size());
  EXPECT_EQ(ts4, timestamps[4]);
}

TEST_P(AudioEncodersTest, FullCycleEncodeDecode) {
  int error;
  int encode_callback_count = 0;
  std::vector<float> buffer(kOpusFramesPerBuffer * options_.channels);
  OpusDecoder* opus_decoder =
      opus_decoder_create(kAudioSampleRate, options_.channels, &error);
  ASSERT_TRUE(error == OPUS_OK && opus_decoder);
  int total_frames = 0;

  auto verify_opus_encoding = [&](EncodedAudioBuffer output, MaybeDesc) {
    ++encode_callback_count;

    // Use the libopus decoder to decode the |encoded_data| and check we
    // get the expected number of frames per buffer.
    EXPECT_EQ(kOpusFramesPerBuffer,
              opus_decode_float(opus_decoder, output.encoded_data.get(),
                                output.encoded_data_size, buffer.data(),
                                kOpusFramesPerBuffer, 0));
  };

  SetupEncoder(base::BindLambdaForTesting(verify_opus_encoding));

  // The opus encoder encodes in multiple of 60 ms. Wait for the total number of
  // frames that will be generated in 60 ms at the input sampling rate.
  const int frames_in_60_ms =
      kOpusBufferDuration.InSecondsF() * options_.sample_rate;

  base::TimeTicks time;
  while (total_frames < frames_in_60_ms) {
    total_frames += ProduceAudioAndEncode(time);
    time += buffer_duration_;
  }

  EXPECT_EQ(1, encode_callback_count);

  // If there are remaining frames in the opus encoder FIFO, we need to flush
  // them before we destroy the encoder. Flushing should trigger the encode
  // callback and we should be able to decode the resulting encoded frames.
  if (total_frames > frames_in_60_ms) {
    encoder()->Flush(base::BindOnce([](Status error) {
      if (!error.is_ok())
        FAIL() << error.message();
    }));
    RunLoop();
    EXPECT_EQ(2, encode_callback_count);
  }

  opus_decoder_destroy(opus_decoder);
  opus_decoder = nullptr;
}

INSTANTIATE_TEST_SUITE_P(All,
                         AudioEncodersTest,
                         testing::ValuesIn(kTestAudioParams));

}  // namespace media
