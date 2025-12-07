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

void SaveOperationalStatus(OperationalStatus* out_status,
                           OperationalStatus in_status) {
  DVLOG(1) << "OperationalStatus transitioning from " << *out_status << " to "
           << in_status;
  *out_status = in_status;
}

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

    OperationalStatus operational_status = STATUS_UNINITIALIZED;
    audio_sender_ = std::make_unique<AudioSender>(
        cast_environment(), audio_config_,
        base::BindOnce(&SaveOperationalStatus, &operational_status),
        std::move(test_senders_->audio_sender));
    RunUntilIdle();
    CHECK_EQ(STATUS_INITIALIZED, operational_status);
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

  EXPECT_CALL(*test_senders_->environment, SendPacket(_, _)).Times(3);

  audio_sender_->InsertAudio(std::move(bus), NowTicks());
  RunUntilIdle();
  EXPECT_EQ(2, openscreen_audio_sender_->GetInFlightFrameCount());
}

}  // namespace media::cast
