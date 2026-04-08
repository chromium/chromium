// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/audio_sender.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "media/base/audio_codecs.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/media.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/constants.h"
#include "media/cast/test/fake_openscreen_clock.h"
#include "media/cast/test/mock_openscreen_environment.h"
#include "media/cast/test/openscreen_test_helpers.h"
#include "media/cast/test/test_with_cast_environment.h"
#include "media/cast/test/utility/audio_utility.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"

using testing::_;

namespace media::cast {

namespace {

}  // namespace

class AudioSenderTest : public TestWithCastEnvironment {
 protected:
  AudioSenderTest() {
    InitializeMediaLibrary();
    AdvanceClock(base::TimeTicks::Now() - base::TimeTicks());

    audio_config_.sender_ssrc = 35535;
    audio_config_.receiver_ssrc = 35536;
    audio_config_.audio_codec_params =
        AudioCodecParams{.codec = AudioCodec::kOpus};
    audio_config_.use_hardware_encoder = false;
    audio_config_.rtp_timebase = kDefaultAudioSamplingRate;
    audio_config_.channels = 2;
    audio_config_.max_bitrate = kDefaultAudioEncoderBitrate;

    test_senders_ =
        std::make_unique<OpenscreenTestSenders>(OpenscreenTestSenders::Config(
            task_environment().GetMainThreadTaskRunner(), GetMockTickClock(),
            openscreen::cast::RtpPayloadType::kAudioOpus, std::nullopt,
            audio_config_));
    openscreen_audio_sender_ = test_senders_->audio_sender.get();

    base::test::TestFuture<OperationalStatus> operational_status;
    audio_sender_ = std::make_unique<AudioSender>(
        cast_environment(), audio_config_, operational_status.GetCallback(),
        std::move(test_senders_->audio_sender));
    CHECK_EQ(STATUS_INITIALIZED, operational_status.Get());
  }

  ~AudioSenderTest() override = default;

  std::unique_ptr<OpenscreenTestSenders> test_senders_;
  FrameSenderConfig audio_config_;
  std::unique_ptr<AudioSender> audio_sender_;
  // Unowned pointer to the openscreen::cast::Sender.
  raw_ptr<openscreen::cast::Sender> openscreen_audio_sender_;
};

TEST_F(AudioSenderTest, Encode20ms) {
  const base::TimeDelta kDuration = base::Milliseconds(20);
  std::unique_ptr<AudioBus> bus(
      TestAudioBusFactory(audio_config_.channels, audio_config_.rtp_timebase,
                          TestAudioBusFactory::kMiddleANoteFreq, 0.5f)
          .NextAudioBus(kDuration));

  base::test::TestFuture<void> packets_future;
  int packets_sent = 0;
  EXPECT_CALL(*test_senders_->environment, SendPacket(_, _))
      .Times(2)
      .WillRepeatedly([&](openscreen::ByteView packet,
                          openscreen::cast::PacketMetadata metadata) {
        if (++packets_sent == 2) {
          packets_future.SetValue();
        }
      });

  audio_sender_->InsertAudio(std::move(bus), NowTicks());
  ASSERT_TRUE(packets_future.Wait());
  EXPECT_EQ(2u, openscreen_audio_sender_->GetInFlightFrameCount());
}

TEST_F(AudioSenderTest, GettersReturnValidValues) {
  EXPECT_GE(audio_sender_->GetEncoderBitrate(), 0);
  EXPECT_GE(audio_sender_->GetFramesInserted(), 0);
  EXPECT_GE(audio_sender_->GetFramesDropped(), 0);

  const base::TimeDelta kDuration = base::Milliseconds(20);
  std::unique_ptr<AudioBus> bus(
      TestAudioBusFactory(audio_config_.channels, audio_config_.rtp_timebase,
                          TestAudioBusFactory::kMiddleANoteFreq, 0.5f)
          .NextAudioBus(kDuration));

  base::test::TestFuture<void> packet_future;
  int packets_sent = 0;
  EXPECT_CALL(*test_senders_->environment, SendPacket(_, _))
      .Times(2)
      .WillRepeatedly([&](openscreen::ByteView packet,
                          openscreen::cast::PacketMetadata metadata) {
        if (++packets_sent == 2) {
          packet_future.SetValue();
        }
      });

  audio_sender_->InsertAudio(std::move(bus), NowTicks());
  ASSERT_TRUE(packet_future.Wait());

  EXPECT_EQ(audio_sender_->GetFramesInserted(), 1);
}

}  // namespace media::cast
