// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_recorder.h"

#include <stdint.h>

#include <string>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_sample_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#include "third_party/opus/src/include/opus.h"

using base::TimeTicks;
using base::test::RunOnceClosure;
using ::testing::_;

namespace {

const int kDefaultSampleRate = 48000;

// The following parameters replicate those in audio_track_recorder.cc, see this
// file for explanations.
const int kMediaStreamAudioTrackBufferDurationMs = 10;
const int kOpusBufferDurationMs = 60;
const int kRatioInputToOutputFrames =
    kOpusBufferDurationMs / kMediaStreamAudioTrackBufferDurationMs;

const int kFramesPerBuffer = kOpusBufferDurationMs * kDefaultSampleRate / 1000;

}  // namespace

namespace blink {

struct ATRTestParams {
  const media::AudioParameters::Format input_format;
  const media::ChannelLayoutConfig channel_layout;
  const int sample_rate;
  const AudioTrackRecorder::CodecId codec;
  const AudioTrackRecorder::BitrateMode bitrateMode;
};

const ATRTestParams kATRTestParams[] = {
    // Equivalent to default settings:
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY, /* input format */
     media::ChannelLayoutConfig::Stereo(),          /* channel layout */
     kDefaultSampleRate,                            /* sample rate */
     AudioTrackRecorder::CodecId::kOpus,            /* codec for encoding */
     AudioTrackRecorder::BitrateMode::kVariable},   /* constant/variable rate */

    // Change to mono:
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::ChannelLayoutConfig::Mono(), kDefaultSampleRate,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},

    // Different sampling rate as well:
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::ChannelLayoutConfig::Mono(), 24000,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::ChannelLayoutConfig::Stereo(), 8000,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},

    // Using a non-default Opus sampling rate (48, 24, 16, 12, or 8 kHz).
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::ChannelLayoutConfig::Mono(), 22050,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::ChannelLayoutConfig::Stereo(), 44100,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::ChannelLayoutConfig::Stereo(), 96000,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::ChannelLayoutConfig::Mono(), kDefaultSampleRate,
     AudioTrackRecorder::CodecId::kPcm,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::ChannelLayoutConfig::Stereo(), kDefaultSampleRate,
     AudioTrackRecorder::CodecId::kPcm,
     AudioTrackRecorder::BitrateMode::kVariable},

    // Use Opus in constatnt bitrate mode:
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::ChannelLayoutConfig::Stereo(), kDefaultSampleRate,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kConstant},
};

int FramesPerBuffer(int sample_rate) {
  return kMediaStreamAudioTrackBufferDurationMs * sample_rate /
         base::Time::kMillisecondsPerSecond;
}

class AudioTrackRecorderTest : public testing::TestWithParam<ATRTestParams> {
 public:
  // Initialize |first_params_| based on test parameters, and |second_params_|
  // to always be the same thing.
  AudioTrackRecorderTest()
      : codec_(GetParam().codec),
        first_params_(GetParam().input_format,
                      GetParam().channel_layout,
                      GetParam().sample_rate,
                      FramesPerBuffer(GetParam().sample_rate)),
        second_params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                       media::ChannelLayoutConfig::Stereo(),
                       kDefaultSampleRate,
                       FramesPerBuffer(kDefaultSampleRate)),
        first_source_(first_params_.channels(),     /* # channels */
                      440,                          /* frequency */
                      first_params_.sample_rate()), /* sample rate */
        second_source_(second_params_.channels(),
                       440,
                       second_params_.sample_rate()),
        opus_decoder_(nullptr),
        first_source_cache_pos_(0) {
    ResetDecoder(first_params_);
    PrepareTrack();
    audio_track_recorder_ = std::make_unique<AudioTrackRecorder>(
        codec_, media_stream_component_,
        WTF::BindRepeating(&AudioTrackRecorderTest::OnEncodedAudio,
                           WTF::Unretained(this)),
        ConvertToBaseOnceCallback(CrossThreadBindOnce([] {})),
        0u /* bits_per_second */, GetParam().bitrateMode);
  }

  AudioTrackRecorderTest(const AudioTrackRecorderTest&) = delete;
  AudioTrackRecorderTest& operator=(const AudioTrackRecorderTest&) = delete;

  ~AudioTrackRecorderTest() {
    opus_decoder_destroy(opus_decoder_);
    opus_decoder_ = nullptr;
    media_stream_component_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
    audio_track_recorder_.reset();
    // Let the message loop run to finish destroying the recorder properly.
    base::RunLoop().RunUntilIdle();
  }

  void ResetDecoder(const media::AudioParameters& params) {
    if (opus_decoder_) {
      opus_decoder_destroy(opus_decoder_);
      opus_decoder_ = nullptr;
    }

    int error;
    opus_decoder_ =
        opus_decoder_create(kDefaultSampleRate, params.channels(), &error);
    EXPECT_TRUE(error == OPUS_OK && opus_decoder_);

    opus_buffer_.reset(new float[kFramesPerBuffer * params.channels()]);
  }

  std::unique_ptr<media::AudioBus> GetFirstSourceAudioBus() {
    std::unique_ptr<media::AudioBus> bus(media::AudioBus::Create(
        first_params_.channels(), first_params_.sample_rate() *
                                      kMediaStreamAudioTrackBufferDurationMs /
                                      base::Time::kMillisecondsPerSecond));
    first_source_.OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
                             bus.get());

    // Save the samples that we read into the first_source_cache_.
    std::unique_ptr<media::AudioBus> cache_bus(
        media::AudioBus::Create(bus->channels(), bus->frames()));
    bus->CopyTo(cache_bus.get());

    int current_size = first_source_cache_.size();
    first_source_cache_.resize(current_size +
                               cache_bus->frames() * cache_bus->channels());
    cache_bus->ToInterleaved<media::Float32SampleTypeTraits>(
        cache_bus->frames(), &first_source_cache_[current_size]);

    return bus;
  }
  std::unique_ptr<media::AudioBus> GetSecondSourceAudioBus() {
    std::unique_ptr<media::AudioBus> bus(media::AudioBus::Create(
        second_params_.channels(), second_params_.sample_rate() *
                                       kMediaStreamAudioTrackBufferDurationMs /
                                       base::Time::kMillisecondsPerSecond));
    second_source_.OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
                              bus.get());
    return bus;
  }

  MOCK_METHOD3(DoOnEncodedAudio,
               void(const media::AudioParameters& params,
                    std::string encoded_data,
                    base::TimeTicks timestamp));

  void OnEncodedAudio(const media::AudioParameters& params,
                      std::string encoded_data,
                      base::TimeTicks timestamp) {
    EXPECT_TRUE(!encoded_data.empty());

    if (codec_ == AudioTrackRecorder::CodecId::kOpus) {
      // Decode |encoded_data| and check we get the expected number of frames
      // per buffer.
      EXPECT_EQ(
          kDefaultSampleRate * kOpusBufferDurationMs / 1000,
          opus_decode_float(opus_decoder_,
                            reinterpret_cast<uint8_t*>(std::data(encoded_data)),
                            static_cast<wtf_size_t>(encoded_data.size()),
                            opus_buffer_.get(), kFramesPerBuffer, 0));
    } else if (codec_ == AudioTrackRecorder::CodecId::kPcm) {
      // Manually confirm that we're getting the same data out as what we
      // generated from the sine wave.
      for (size_t b = 0; b + 3 < encoded_data.size() &&
                         first_source_cache_pos_ < first_source_cache_.size();
           b += sizeof(first_source_cache_[0]), ++first_source_cache_pos_) {
        float sample;
        memcpy(&sample, &(encoded_data)[b], 4);
        ASSERT_FLOAT_EQ(sample, first_source_cache_[first_source_cache_pos_])
            << "(Sample " << first_source_cache_pos_ << ")";
      }
    }

    DoOnEncodedAudio(params, std::move(encoded_data), timestamp);
  }

  // AudioTrackRecorder and MediaStreamComponent for fooling it.
  std::unique_ptr<AudioTrackRecorder> audio_track_recorder_;
  Persistent<MediaStreamComponent> media_stream_component_;

  // The codec we'll use for compression the audio.
  const AudioTrackRecorder::CodecId codec_;

  // Two different sets of AudioParameters for testing re-init of ATR.
  const media::AudioParameters first_params_;
  const media::AudioParameters second_params_;

  // AudioSources for creating AudioBuses.
  media::SineWaveAudioSource first_source_;
  media::SineWaveAudioSource second_source_;

  // Decoder for verifying data was properly encoded.
  OpusDecoder* opus_decoder_;
  std::unique_ptr<float[]> opus_buffer_;

  // Save the data we generate from the first source so that we might compare it
  // later if we happen to be using the PCM encoder.
  Vector<float> first_source_cache_;
  wtf_size_t first_source_cache_pos_;

 private:
  // Prepares a blink track of a given MediaStreamType and attaches the native
  // track, which can be used to capture audio data and pass it to the producer.
  // Adapted from media::WebRTCLocalAudioSourceProviderTest.
  void PrepareTrack() {
    auto audio_source = std::make_unique<MediaStreamAudioSource>(
        scheduler::GetSingleThreadTaskRunnerForTesting(), true);
    auto* source = MakeGarbageCollected<MediaStreamSource>(
        String::FromUTF8("dummy_source_id"), MediaStreamSource::kTypeAudio,
        String::FromUTF8("dummy_source_name"), false /* remote */,
        std::move(audio_source));
    media_stream_component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
        String::FromUTF8("audio_track"), source,
        std::make_unique<MediaStreamAudioTrack>(/*is_local=*/true));
    CHECK(MediaStreamAudioSource::From(source)->ConnectToInitializedTrack(
        media_stream_component_));
  }
};

TEST_P(AudioTrackRecorderTest, OnDataOpus) {
  if (codec_ != AudioTrackRecorder::CodecId::kOpus)
    return;

  testing::InSequence s;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  // Give ATR initial audio parameters.
  audio_track_recorder_->OnSetFormat(first_params_);

  std::vector<std::size_t> encodedPacketSizes;
  // TODO(ajose): consider adding WillOnce(SaveArg...) and inspecting, as done
  // in VTR unittests. http://crbug.com/548856
  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _))
      .Times(1)
      .WillOnce([&encodedPacketSizes](const media::AudioParameters&,
                                      std::string encoded_data,
                                      base::TimeTicks) {
        encodedPacketSizes.push_back(encoded_data.size());
      });

  audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                base::TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i) {
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                  base::TimeTicks::Now());
  }

  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _))
      .Times(1)
      // Only reset the decoder once we've heard back:
      .WillOnce(testing::DoAll(
          RunOnceClosure(WTF::Bind(&AudioTrackRecorderTest::ResetDecoder,
                                   WTF::Unretained(this), second_params_)),
          [&encodedPacketSizes](const media::AudioParameters&,
                                std::string encoded_data, base::TimeTicks) {
            encodedPacketSizes.push_back(encoded_data.size());
          }));

  audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                base::TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i) {
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                  base::TimeTicks::Now());
  }

  // If the amount of samples/10ms buffer is not an integer (e.g. 22050Hz) we
  // need an extra OnData() to account for the round-off error.
  if (GetParam().sample_rate % 100) {
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                  base::TimeTicks::Now());
  }

  // Give ATR new audio parameters.
  audio_track_recorder_->OnSetFormat(second_params_);

  // Send audio with different params.
  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _))
      .Times(1)
      .WillOnce(testing::DoAll(
          RunOnceClosure(std::move(quit_closure)),
          [&encodedPacketSizes](const media::AudioParameters&,
                                std::string encoded_data, base::TimeTicks) {
            encodedPacketSizes.push_back(encoded_data.size());
          }));

  audio_track_recorder_->OnData(*GetSecondSourceAudioBus(),
                                base::TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i) {
    audio_track_recorder_->OnData(*GetSecondSourceAudioBus(),
                                  base::TimeTicks::Now());
  }

  // Check that in CBR mode, all the packets are the same size, to confirm it
  // actually made a CBR recording.
  if (GetParam().bitrateMode == AudioTrackRecorder::BitrateMode::kConstant) {
    if (!encodedPacketSizes.empty()) {
      EXPECT_THAT(encodedPacketSizes, testing::Each(encodedPacketSizes[0]));
    }
  }

  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_P(AudioTrackRecorderTest, OnDataPcm) {
  if (codec_ != AudioTrackRecorder::CodecId::kPcm)
    return;

  testing::InSequence s;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  audio_track_recorder_->OnSetFormat(first_params_);

  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _)).Times(5);
  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _))
      .WillOnce(RunOnceClosure(std::move(quit_closure)));

  audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                base::TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i) {
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                  base::TimeTicks::Now());
  }

  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_P(AudioTrackRecorderTest, PauseResume) {
  if (codec_ != AudioTrackRecorder::CodecId::kOpus)
    return;

  testing::InSequence s;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  // Give ATR initial audio parameters.
  audio_track_recorder_->OnSetFormat(first_params_);

  audio_track_recorder_->Pause();
  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _)).Times(0);
  audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                base::TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i) {
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                  base::TimeTicks::Now());
  }

  audio_track_recorder_->Resume();
  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _))
      .Times(1)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                base::TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i) {
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                  base::TimeTicks::Now());
  }

  if (GetParam().sample_rate % 100) {
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                  base::TimeTicks::Now());
  }

  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(this);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AudioTrackRecorderTest,
                         testing::ValuesIn(kATRTestParams));
}  // namespace blink
