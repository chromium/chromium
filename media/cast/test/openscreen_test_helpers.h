// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_OPENSCREEN_TEST_HELPERS_H_
#define MEDIA_CAST_TEST_OPENSCREEN_TEST_HELPERS_H_

#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "components/openscreen_platform/task_runner.h"
#include "media/cast/cast_config.h"
#include "media/cast/test/mock_openscreen_environment.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"
#include "third_party/openscreen/src/cast/streaming/sender_packet_router.h"
#include "third_party/openscreen/src/platform/api/time.h"
#include "third_party/openscreen/src/platform/base/trivial_clock_traits.h"

namespace media::cast {

// Used to construct a valid set of openscreen::cast::Senders for use in tests.
// Takes care of some of the complex Open Screen configuration.
struct OpenscreenTestSenders {
  struct Config {
    Config(scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
           const base::TickClock* clock,
           std::optional<openscreen::cast::RtpPayloadType> audio_rtp_type =
               std::nullopt,
           std::optional<openscreen::cast::RtpPayloadType> video_rtp_type =
               std::nullopt,
           std::optional<FrameSenderConfig> audio_config = std::nullopt,
           std::optional<FrameSenderConfig> video_config = std::nullopt);
    Config(const Config&) = delete;
    Config(Config&&);
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&);
    ~Config();

    // The task runner to be used with the Open Screen environment.
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner;

    // Used for setting up the fake Open Screen clock.
    raw_ptr<const base::TickClock> clock;

    // If set, will construct a valid audio sender.
    std::optional<openscreen::cast::RtpPayloadType> audio_rtp_type;

    // If set, will construct a valid video sender.
    std::optional<openscreen::cast::RtpPayloadType> video_rtp_type;

    // If set, will use this config to help set up the audio sender.
    std::optional<FrameSenderConfig> audio_config;

    // If set, will use this config to help set up the video sender.
    std::optional<FrameSenderConfig> video_config;
  };

  explicit OpenscreenTestSenders(const Config& config);
  OpenscreenTestSenders(const OpenscreenTestSenders&) = delete;
  OpenscreenTestSenders(OpenscreenTestSenders&&) = delete;
  OpenscreenTestSenders& operator=(const OpenscreenTestSenders&) = delete;
  OpenscreenTestSenders& operator=(OpenscreenTestSenders&&) = delete;
  ~OpenscreenTestSenders();

  openscreen_platform::TaskRunner task_runner;
  std::unique_ptr<MockOpenscreenEnvironment> environment;
  std::unique_ptr<openscreen::cast::SenderPacketRouter> sender_packet_router;
  std::unique_ptr<openscreen::cast::Sender> audio_sender;
  std::unique_ptr<openscreen::cast::Sender> video_sender;
};

}  // namespace media::cast

#endif  // MEDIA_CAST_TEST_OPENSCREEN_TEST_HELPERS_H_
