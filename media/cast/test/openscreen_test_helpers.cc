// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/openscreen_test_helpers.h"

#include "crypto/random.h"
#include "media/cast/test/fake_openscreen_clock.h"
#include "media/cast/test/mock_openscreen_environment.h"
#include "third_party/openscreen/src/cast/streaming/sender_packet_router.h"

namespace media::cast {

constexpr uint32_t kFirstSsrc = 35535;
constexpr int kRtpTimebase = 9000;
constexpr auto kDefaultPlayoutDelay = std::chrono::milliseconds(400);

// Always have picture loss indication support set to true.
constexpr bool kIsPliEnabled = true;

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

OpenscreenTestSenders::OpenscreenTestSenders(
    const OpenscreenTestSenders::Config& config)
    : task_runner(config.sequenced_task_runner) {
  FakeOpenscreenClock::SetTickClock(config.clock);
  environment = std::make_unique<MockOpenscreenEnvironment>(
      &FakeOpenscreenClock::now, task_runner);
  sender_packet_router = std::make_unique<openscreen::cast::SenderPacketRouter>(
      *environment, 20, std::chrono::milliseconds(10));

  if (config.audio_rtp_type.has_value()) {
    std::unique_ptr<openscreen::cast::SessionConfig> audio_session_config;
    if (config.audio_config.has_value()) {
      audio_session_config = std::make_unique<openscreen::cast::SessionConfig>(
          ToOpenscreenSessionConfigForTesting(config.audio_config.value(),
                                              kIsPliEnabled));
    } else {
      std::array<uint8_t, 16> audio_aes_key;
      std::array<uint8_t, 16> audio_aes_iv_mask;
      crypto::RandBytes(audio_aes_key);
      crypto::RandBytes(audio_aes_iv_mask);
      audio_session_config = std::make_unique<openscreen::cast::SessionConfig>(
          kFirstSsrc, kFirstSsrc + 1, kRtpTimebase, 2 /* channels */,
          kDefaultPlayoutDelay, audio_aes_key, audio_aes_iv_mask,
          kIsPliEnabled);
    }
    audio_sender = std::make_unique<openscreen::cast::Sender>(
        *environment, *sender_packet_router, *audio_session_config,
        *config.audio_rtp_type);
  }

  if (config.video_rtp_type.has_value()) {
    std::unique_ptr<openscreen::cast::SessionConfig> video_session_config;
    if (config.video_config.has_value()) {
      video_session_config = std::make_unique<openscreen::cast::SessionConfig>(
          ToOpenscreenSessionConfigForTesting(config.video_config.value(),
                                              kIsPliEnabled));
    } else {
      std::array<uint8_t, 16> video_aes_key;
      std::array<uint8_t, 16> video_aes_iv_mask;
      crypto::RandBytes(video_aes_key);
      crypto::RandBytes(video_aes_iv_mask);
      video_session_config = std::make_unique<openscreen::cast::SessionConfig>(

          kFirstSsrc + 2, kFirstSsrc + 3, kRtpTimebase, 1 /* channels */,
          kDefaultPlayoutDelay, video_aes_key, video_aes_iv_mask,
          kIsPliEnabled);
    }
    video_sender = std::make_unique<openscreen::cast::Sender>(
        *environment, *sender_packet_router, *video_session_config,
        *config.video_rtp_type);
  }
}

OpenscreenTestSenders::~OpenscreenTestSenders() {
  FakeOpenscreenClock::ClearTickClock();
}

}  // namespace media::cast
