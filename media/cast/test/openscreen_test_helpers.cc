// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/openscreen_test_helpers.h"

#include "crypto/random.h"
#include "media/cast/test/fake_openscreen_clock.h"
#include "media/cast/test/mock_openscreen_environment.h"
#include "third_party/openscreen/src/cast/streaming/sender_packet_router.h"

namespace media::cast {

namespace {
constexpr uint32_t kFirstSsrc = 35535;
constexpr int kRtpTimebase = 9000;
constexpr auto kDefaultPlayoutDelay = std::chrono::milliseconds(400);

// Always have picture loss indication support set to true.
constexpr bool kIsPliEnabled = true;
}  // namespace

openscreen::cast::SessionConfig GetDefaultSessionConfigForTesting() {
  // Open Screen negotiates sessions using a cryptographically secure AES
  // key and IV mask.
  std::array<uint8_t, 16> aes_key;
  std::array<uint8_t, 16> aes_iv_mask;
  crypto::RandBytes(aes_key);
  crypto::RandBytes(aes_iv_mask);

  return openscreen::cast::SessionConfig(
      kFirstSsrc, kFirstSsrc + 1, kRtpTimebase, 2 /* channels */,
      kDefaultPlayoutDelay, aes_key, aes_iv_mask, kIsPliEnabled);
}

openscreen::cast::SessionConfig ToOpenscreenSessionConfigForTesting(
    const FrameSenderConfig& config,
    bool is_pli_enabled) {
  // Open Screen negotiates sessions using a cryptographically secure AES
  // key and IV mask. This functionality is not really exposed in Chrome.
  std::array<uint8_t, 16> aes_key;
  std::array<uint8_t, 16> aes_iv_mask;
  crypto::RandBytes(aes_key);
  crypto::RandBytes(aes_iv_mask);

  return openscreen::cast::SessionConfig(
      config.sender_ssrc, config.receiver_ssrc, config.rtp_timebase,
      config.channels,
      std::chrono::milliseconds(config.max_playout_delay.InMilliseconds()),
      aes_key, aes_iv_mask, is_pli_enabled);
}

OpenscreenTestSenders::Config::Config(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    const base::TickClock* clock,
    std::optional<openscreen::cast::RtpPayloadType> audio_rtp_type,
    std::optional<openscreen::cast::RtpPayloadType> video_rtp_type,
    std::optional<FrameSenderConfig> audio_config,
    std::optional<FrameSenderConfig> video_config)
    : sequenced_task_runner(std::move(sequenced_task_runner)),
      clock(clock),
      audio_rtp_type(std::move(audio_rtp_type)),
      video_rtp_type(std::move(video_rtp_type)),
      audio_config(std::move(audio_config)),
      video_config(std::move(video_config)) {}

OpenscreenTestSenders::Config::Config(Config&&) = default;
OpenscreenTestSenders::Config& OpenscreenTestSenders::Config::operator=(
    Config&&) = default;

OpenscreenTestSenders::Config::~Config() = default;

MockSender::MockSender() : MockSender(GetDefaultSessionConfigForTesting()) {}

MockSender::MockSender(openscreen::cast::SessionConfig config)
    : config_(std::move(config)) {
  ON_CALL(*this, config).WillByDefault(testing::ReturnRef(config_));
  ON_CALL(*this, SetObserver)
      .WillByDefault([this](openscreen::cast::Sender::Observer* observer) {
        set_observer(observer);
      });
  ON_CALL(*this, GetInFlightFrameCount).WillByDefault([this]() {
    return in_flight_frames_.size();
  });
  ON_CALL(*this, GetMaxInFlightMediaDuration)
      .WillByDefault(testing::Return(std::chrono::seconds(1)));
  ON_CALL(*this, GetInFlightMediaDuration)
      .WillByDefault(testing::Return(std::chrono::seconds(0)));
  ON_CALL(*this, GetCurrentRoundTripTime)
      .WillByDefault(testing::Return(std::chrono::milliseconds(10)));
  ON_CALL(*this, NeedsKeyFrame).WillByDefault(testing::Return(true));
  ON_CALL(*this, GetNextFrameId).WillByDefault([this]() {
    return in_flight_frames_.empty() ? openscreen::cast::FrameId::first()
                                     : *in_flight_frames_.rbegin() + 1;
  });
  ON_CALL(*this, EnqueueFrame)
      .WillByDefault([this](const openscreen::cast::EncodedFrame& frame) {
        in_flight_frames_.insert(frame.frame_id);
        if (environment_) {
          environment_->SendPacket(openscreen::ByteView(frame.data),
                                   openscreen::cast::PacketMetadata{});
        }
        if (observer_ && task_runner_) {
          task_runner_->PostTask(
              [weak_this = AsWeakPtr(), frame_id = frame.frame_id]() {
                if (weak_this && weak_this->observer_) {
                  weak_this->observer_->OnFrameCanceled(frame_id);
                }
              });
        }
        return openscreen::cast::Sender::EnqueueFrameResult::OK;
      });
  ON_CALL(*this, CancelInFlightData).WillByDefault([this]() {
    in_flight_frames_.clear();
  });
}

MockSender::~MockSender() = default;

MockReceiver::MockReceiver()
    : MockReceiver(GetDefaultSessionConfigForTesting()) {}

MockReceiver::MockReceiver(openscreen::cast::SessionConfig config)
    : config_(std::move(config)) {
  ON_CALL(*this, config).WillByDefault(testing::ReturnRef(config_));
}

MockReceiver::~MockReceiver() = default;

OpenscreenTestSenders::OpenscreenTestSenders(
    const OpenscreenTestSenders::Config& config)
    : task_runner(config.sequenced_task_runner) {
  FakeOpenscreenClock::SetTickClock(config.clock);
  environment = std::make_unique<MockOpenscreenEnvironment>(
      &FakeOpenscreenClock::now, task_runner);
  sender_packet_router = std::make_unique<openscreen::cast::SenderPacketRouter>(
      *environment, 20, std::chrono::milliseconds(10));

  if (config.audio_rtp_type.has_value()) {
    if (config.audio_config.has_value()) {
      audio_sender =
          std::make_unique<MockSender>(ToOpenscreenSessionConfigForTesting(
              config.audio_config.value(), kIsPliEnabled));
    } else {
      audio_sender = std::make_unique<MockSender>();
    }
    audio_sender->set_environment(environment.get());
    audio_sender->set_task_runner(&task_runner);
  }

  if (config.video_rtp_type.has_value()) {
    if (config.video_config.has_value()) {
      video_sender =
          std::make_unique<MockSender>(ToOpenscreenSessionConfigForTesting(
              config.video_config.value(), kIsPliEnabled));
    } else {
      video_sender =
          std::make_unique<MockSender>(openscreen::cast::SessionConfig(
              kFirstSsrc + 2, kFirstSsrc + 3, kRtpTimebase, 1 /* channels */,
              kDefaultPlayoutDelay,
              GetDefaultSessionConfigForTesting().aes_secret_key,
              GetDefaultSessionConfigForTesting().aes_iv_mask, kIsPliEnabled));
    }
    video_sender->set_environment(environment.get());
    video_sender->set_task_runner(&task_runner);
  }
}

OpenscreenTestSenders::~OpenscreenTestSenders() {
  FakeOpenscreenClock::ClearTickClock();
}

}  // namespace media::cast
