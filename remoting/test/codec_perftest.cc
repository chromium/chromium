// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "remoting/codec/video_encoder_vpx.h"
#include "remoting/proto/video.pb.h"
#include "remoting/test/cyclic_frame_generator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace test {

constexpr auto kIntervalBetweenFrames = base::Seconds(1) / 30;

struct CodecParams {
  CodecParams(bool use_vp9, bool lossless_color)
      : use_vp9(use_vp9), lossless_color(lossless_color) {}

  bool use_vp9;
  bool lossless_color;
};

class CodecPerfTest : public testing::Test,
                      public testing::WithParamInterface<CodecParams> {
 public:
  void SetUp() override {
    if (GetParam().use_vp9) {
      encoder_ = VideoEncoderVpx::CreateForVP9();
      encoder_->SetLosslessColor(GetParam().lossless_color);
    } else {
      encoder_ = VideoEncoderVpx::CreateForVP8();
    }
    encoder_->SetTickClockForTests(&clock_);

    frame_generator_ = CyclicFrameGenerator::Create();
    frame_generator_->SetTickClock(&clock_);
  }

 protected:
  base::SimpleTestTickClock clock_;
  scoped_refptr<CyclicFrameGenerator> frame_generator_;
  std::unique_ptr<VideoEncoderVpx> encoder_;
};

INSTANTIATE_TEST_SUITE_P(VP8,
                         CodecPerfTest,
                         ::testing::Values(CodecParams(false, false)));
INSTANTIATE_TEST_SUITE_P(VP9,
                         CodecPerfTest,
                         ::testing::Values(CodecParams(true, false)));
INSTANTIATE_TEST_SUITE_P(VP9LosslessColor,
                         CodecPerfTest,
                         ::testing::Values(CodecParams(true, true)));

TEST_P(CodecPerfTest, EncodeLatency) {
  const int kTotalFrames = 300;
  base::TimeDelta total_latency;

  base::TimeDelta total_latency_big_frames;
  int big_frame_count = 0;
  base::TimeDelta total_latency_small_frames;
  int small_frame_count = 0;
  base::TimeDelta total_latency_empty_frames;
  int empty_frame_count = 0;

  int total_bytes = 0;

  for (int i = 0; i < kTotalFrames; ++i) {
    std::unique_ptr<webrtc::DesktopFrame> frame =
        frame_generator_->GenerateFrame(nullptr);
    base::TimeTicks started = base::TimeTicks::Now();

    std::unique_ptr<VideoPacket> packet = encoder_->Encode(*frame);

    base::TimeTicks ended = base::TimeTicks::Now();
    base::TimeDelta latency = ended - started;

    total_latency += latency;
    if (packet) {
      total_bytes += packet->data().size();
    }

    switch (frame_generator_->last_frame_type()) {
      case CyclicFrameGenerator::ChangeType::NO_CHANGES:
        total_latency_empty_frames += latency;
        ++empty_frame_count;
        break;
      case CyclicFrameGenerator::ChangeType::FULL:
        total_latency_big_frames += latency;
        ++big_frame_count;
        break;
      case CyclicFrameGenerator::ChangeType::CURSOR:
        total_latency_small_frames += latency;
        ++small_frame_count;
        break;
    }

    clock_.Advance(kIntervalBetweenFrames);
  }

  VLOG(0) << "Total time: " << total_latency.InMillisecondsF();
  VLOG(0) << "Average encode latency: "
          << (total_latency / kTotalFrames).InMillisecondsF();

  CHECK(big_frame_count);
  VLOG(0) << "Average encode latency for big frames: "
          << (total_latency_big_frames / big_frame_count).InMillisecondsF();

  if (small_frame_count) {
    VLOG(0)
        << "Average encode latency for small frames: "
        << (total_latency_small_frames / small_frame_count).InMillisecondsF();
  }

  if (empty_frame_count) {
    VLOG(0)
        << "Average encode latency for empty frames: "
        << (total_latency_empty_frames / empty_frame_count).InMillisecondsF();
  }

  VLOG(0) << "Encoded bytes: " << total_bytes;
}

TEST_P(CodecPerfTest, MaxFramerate) {
  const int kTotalFrames = 100;
  base::TimeDelta total_latency;

  // Update the whole screen on every frame.
  frame_generator_->set_frame_cycle_period(kIntervalBetweenFrames);

  for (int i = 0; i < kTotalFrames; ++i) {
    std::unique_ptr<webrtc::DesktopFrame> frame =
        frame_generator_->GenerateFrame(nullptr);
    base::TimeTicks started = base::TimeTicks::Now();

    std::unique_ptr<VideoPacket> packet = encoder_->Encode(*frame);

    base::TimeTicks ended = base::TimeTicks::Now();
    base::TimeDelta latency = ended - started;

    total_latency += latency;

    clock_.Advance(kIntervalBetweenFrames);
  }

  VLOG(0) << "Max framerate: " << kTotalFrames / total_latency.InSecondsF();
}

}  // namespace test
}  // namespace remoting
