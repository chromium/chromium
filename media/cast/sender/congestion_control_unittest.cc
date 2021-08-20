// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/cast/constants.h"
#include "media/cast/sender/congestion_control.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int kMaxBitrateConfigured = 5000000;
static const int kMinBitrateConfigured = 500000;
static const int64_t kFrameDelayMs = 33;
static const double kMaxFrameRate = 1000.0 / kFrameDelayMs;
static const int64_t kStartMillisecond = INT64_C(12345678900000);
static const double kTargetEmptyBufferFraction = 0.9;

class CongestionControlTest : public ::testing::Test {
 protected:
  CongestionControlTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&testing_clock_)) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    congestion_control_.reset(NewAdaptiveCongestionControl(
        &testing_clock_, kMaxBitrateConfigured, kMinBitrateConfigured,
        kMaxFrameRate));
    const int max_unacked_frames = 10;
    const base::TimeDelta target_playout_delay =
        (max_unacked_frames - 1) * base::TimeDelta::FromSeconds(1) /
        kMaxFrameRate;
    congestion_control_->UpdateTargetPlayoutDelay(target_playout_delay);
  }

  void AckFrame(FrameId frame_id) {
    congestion_control_->AckFrame(frame_id, testing_clock_.NowTicks());
  }

  void Run(int num_frames,
           size_t frame_size,
           base::TimeDelta rtt,
           base::TimeDelta frame_delay,
           base::TimeDelta ack_time) {
    const FrameId end = FrameId::first() + num_frames;
    for (frame_id_ = FrameId::first(); frame_id_ < end; frame_id_++) {
      congestion_control_->UpdateRtt(rtt);
      congestion_control_->SendFrameToTransport(
          frame_id_, frame_size, testing_clock_.NowTicks());
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&CongestionControlTest::AckFrame,
                         base::Unretained(this), frame_id_),
          ack_time);
      task_runner_->Sleep(frame_delay);
    }
  }

  base::SimpleTestTickClock testing_clock_;
  std::unique_ptr<CongestionControl> congestion_control_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  FrameId frame_id_;

  DISALLOW_COPY_AND_ASSIGN(CongestionControlTest);
};

// Tests that AdaptiveCongestionControl returns reasonable bitrates based on
// estimations of network bandwidth and how much is in-flight (i.e, using the
// "target buffer fill" model).
TEST_F(CongestionControlTest, SimpleRun) {
  uint32_t frame_size = 10000 * 8;
  Run(500,
      frame_size,
      base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMilliseconds(kFrameDelayMs),
      base::TimeDelta::FromMilliseconds(45));
  // Empty the buffer.
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(100));

  uint32_t safe_bitrate = frame_size * 1000 / kFrameDelayMs;
  uint32_t bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(
      safe_bitrate / kTargetEmptyBufferFraction, bitrate, safe_bitrate * 0.05);

  bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(200),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(safe_bitrate / kTargetEmptyBufferFraction * 2 / 3,
              bitrate,
              safe_bitrate * 0.05);

  bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(safe_bitrate / kTargetEmptyBufferFraction * 1 / 3,
              bitrate,
              safe_bitrate * 0.05);

  // Add a large (100ms) frame.
  congestion_control_->SendFrameToTransport(
      frame_id_++, safe_bitrate * 100 / 1000, testing_clock_.NowTicks());

  // Results should show that we have ~200ms to send.
  bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(safe_bitrate / kTargetEmptyBufferFraction * 2 / 3,
              bitrate,
              safe_bitrate * 0.05);

  // Add another large (100ms) frame.
  congestion_control_->SendFrameToTransport(
      frame_id_++, safe_bitrate * 100 / 1000, testing_clock_.NowTicks());

  // Results should show that we have ~100ms to send.
  bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(safe_bitrate / kTargetEmptyBufferFraction * 1 / 3,
              bitrate,
              safe_bitrate * 0.05);

  // Ack the last frame.
  std::vector<FrameId> received_frames;
  received_frames.push_back(frame_id_ - 1);
  congestion_control_->AckLaterFrames(received_frames,
                                      testing_clock_.NowTicks());

  // Results should show that we have ~200ms to send.
  bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(safe_bitrate / kTargetEmptyBufferFraction * 2 / 3, bitrate,
              safe_bitrate * 0.05);
}

// Regression test for http://crbug.com/685392: This confirms that enough
// history is maintained in AdaptiveCongestionControl to avoid invalid
// indexing offsets. This test is successful if it does not crash the process.
TEST_F(CongestionControlTest, RetainsSufficientHistory) {
  constexpr base::TimeDelta kFakePlayoutDelay =
      base::TimeDelta::FromMilliseconds(400);

  // Sanity-check: With no data, GetBitrate() returns an in-range value.
  const int bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + kFakePlayoutDelay, kFakePlayoutDelay);
  ASSERT_GE(bitrate, kMinBitrateConfigured);
  ASSERT_LE(bitrate, kMaxBitrateConfigured);

  // Notify AdaptiveCongestionControl of a large number (the maximum possible)
  // of frames being enqueued for transport, but not yet ACKed. Confirm
  // GetBitrate() returns an in-range value at each step.
  FrameId frame_id = FrameId::first();
  for (int i = 0; i < kMaxUnackedFrames; ++i) {
    congestion_control_->SendFrameToTransport(frame_id, 16384,
                                              testing_clock_.NowTicks());

    const int bitrate = congestion_control_->GetBitrate(
        testing_clock_.NowTicks() + kFakePlayoutDelay, kFakePlayoutDelay);
    ASSERT_GE(bitrate, kMinBitrateConfigured);
    ASSERT_LE(bitrate, kMaxBitrateConfigured);

    task_runner_->Sleep(base::TimeDelta::FromMilliseconds(kFrameDelayMs));
    ++frame_id;
  }

  // Notify AdaptiveCongestionControl that each frame is ACK'ed, again checking
  // that GetBitrate() returns an in-range value at each step.
  frame_id = FrameId::first();
  for (int i = 0; i < kMaxUnackedFrames; ++i) {
    congestion_control_->AckFrame(frame_id, testing_clock_.NowTicks());

    const int bitrate = congestion_control_->GetBitrate(
        testing_clock_.NowTicks() + kFakePlayoutDelay, kFakePlayoutDelay);
    ASSERT_GE(bitrate, kMinBitrateConfigured);
    ASSERT_LE(bitrate, kMaxBitrateConfigured);

    task_runner_->Sleep(base::TimeDelta::FromMilliseconds(kFrameDelayMs));
    ++frame_id;
  }
}

}  // namespace cast
}  // namespace media
