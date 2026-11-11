// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_software_rate_controller.h"

#include <array>
#include <memory>
#include <utility>

#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/h264_rate_control_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

constexpr base::TimeDelta kFrameDuration = base::Seconds(1) / 30;

VideoEncodeAccelerator::Config GetConfig(uint32_t bitrate_bps) {
  VideoEncodeAccelerator::Config config;
  config.input_visible_size = gfx::Size(1280, 720);
  config.bitrate = Bitrate::ConstantBitrate(bitrate_bps);
  config.framerate = 30;
  config.content_type = VideoEncodeAccelerator::Config::ContentType::kDisplay;
  return config;
}

VideoBitrateAllocation GetAllocation(uint32_t bitrate_bps) {
  VideoBitrateAllocation allocation(Bitrate::Mode::kConstant);
  allocation.SetBitrate(0, 0, bitrate_bps);
  return allocation;
}

std::unique_ptr<D3D12SoftwareRateController> CreateRateController(
    uint32_t bitrate_bps,
    uint32_t max_qp = 51) {
  auto rate_controller_or_error = D3D12SoftwareRateController::Create(
      GetConfig(bitrate_bps), GetAllocation(bitrate_bps),
      /*num_temporal_layers=*/1, /*min_qp=*/16, max_qp);
  EXPECT_TRUE(rate_controller_or_error.has_value());
  return rate_controller_or_error.has_value()
             ? std::move(rate_controller_or_error).value()
             : nullptr;
}

// Encodes `num_frames` frames of `frame_size` bytes spaced `interval` apart,
// starting with a key frame. Returns the QP of the last frame.
uint32_t EncodeFrames(D3D12SoftwareRateController& rate_controller,
                      int num_frames,
                      size_t frame_size,
                      base::TimeDelta interval) {
  uint32_t qp = 0;
  for (int i = 0; i < num_frames; ++i) {
    const bool is_keyframe = i == 0;
    qp = rate_controller.ComputeQp(i * interval, is_keyframe,
                                   /*temporal_idx=*/0);
    rate_controller.FinishFrame(frame_size, is_keyframe, /*temporal_idx=*/0);
  }
  return qp;
}

}  // namespace

TEST(D3D12SoftwareRateControllerTest, HRDBufferSize) {
  // Low bitrates use the floor.
  EXPECT_EQ(GetSoftwareRateControllerHRDBufferSize(0u), 40000u);
  EXPECT_EQ(GetSoftwareRateControllerHRDBufferSize(300000u), 40000u);
  // Otherwise the buffer holds one second of the bitrate.
  EXPECT_EQ(GetSoftwareRateControllerHRDBufferSize(10000000u), 1250000u);
  EXPECT_EQ(GetSoftwareRateControllerHRDBufferSize(50000000u), 6250000u);
}

TEST(D3D12SoftwareRateControllerTest, IsSupported) {
  EXPECT_FALSE(D3D12SoftwareRateController::IsSupported(
      Bitrate::Mode::kConstant, /*num_temporal_layers=*/0));
  EXPECT_TRUE(D3D12SoftwareRateController::IsSupported(
      Bitrate::Mode::kConstant, /*num_temporal_layers=*/1));
  EXPECT_TRUE(D3D12SoftwareRateController::IsSupported(
      Bitrate::Mode::kVariable, /*num_temporal_layers=*/1));
  EXPECT_FALSE(D3D12SoftwareRateController::IsSupported(
      Bitrate::Mode::kExternal, /*num_temporal_layers=*/1));
  EXPECT_FALSE(D3D12SoftwareRateController::IsSupported(
      Bitrate::Mode::kConstant,
      h264_rate_control_util::kMaxNumTemporalLayers + 1));
}

TEST(D3D12SoftwareRateControllerTest, CreateFailsWithZeroBitrate) {
  auto rate_controller_or_error = D3D12SoftwareRateController::Create(
      GetConfig(1000000u), VideoBitrateAllocation(Bitrate::Mode::kConstant),
      /*num_temporal_layers=*/1, /*min_qp=*/16, /*max_qp=*/51);
  ASSERT_FALSE(rate_controller_or_error.has_value());
  EXPECT_EQ(rate_controller_or_error.error().code(),
            EncoderStatus::Codes::kEncoderUnsupportedConfig);
}

TEST(D3D12SoftwareRateControllerTest, UpdateRateControlRejectsInvalidValues) {
  auto rate_controller = CreateRateController(1000000u);
  ASSERT_TRUE(rate_controller);
  EXPECT_FALSE(rate_controller->UpdateRateControl(
      VideoBitrateAllocation(Bitrate::Mode::kConstant), 30));
  EXPECT_FALSE(rate_controller->UpdateRateControl(
      VideoBitrateAllocation(Bitrate::Mode::kExternal), 30));
  EXPECT_FALSE(rate_controller->UpdateRateControl(GetAllocation(1000000u), 0));
  EXPECT_TRUE(rate_controller->UpdateRateControl(GetAllocation(2000000u), 30));
  EXPECT_TRUE(rate_controller->UpdateRateControl(GetAllocation(2000000u), 60));
}

TEST(D3D12SoftwareRateControllerTest,
     UpdateRateControlPreservesStateAcrossFramerateChange) {
  // Warm up two identical controllers on tiny frames so QP drops near `min_qp`.
  // Updating the nominal frame rate on one of them must keep its learned
  // estimator and HRD state rather than resetting to a fresh controller.
  constexpr uint32_t kBitrate = 5000000u;
  constexpr size_t kSmallFrameSize = 1000;
  auto updated_controller = CreateRateController(kBitrate);
  auto steady_controller = CreateRateController(kBitrate);
  auto fresh_controller = CreateRateController(kBitrate);
  ASSERT_TRUE(updated_controller);
  ASSERT_TRUE(steady_controller);
  ASSERT_TRUE(fresh_controller);

  EncodeFrames(*updated_controller, 20, kSmallFrameSize, kFrameDuration);
  EncodeFrames(*steady_controller, 20, kSmallFrameSize, kFrameDuration);

  ASSERT_TRUE(updated_controller->UpdateRateControl(GetAllocation(kBitrate),
                                                    /*framerate=*/24));

  const base::TimeDelta next_ts = 20 * kFrameDuration;
  const uint32_t updated_qp =
      updated_controller->ComputeQp(next_ts, /*is_keyframe=*/false,
                                    /*temporal_idx=*/0);
  const uint32_t steady_qp =
      steady_controller->ComputeQp(next_ts, /*is_keyframe=*/false,
                                   /*temporal_idx=*/0);
  const uint32_t fresh_first_p_qp = [&] {
    fresh_controller->ComputeQp(base::TimeDelta(), /*is_keyframe=*/true,
                                /*temporal_idx=*/0);
    fresh_controller->FinishFrame(kSmallFrameSize, /*is_keyframe=*/true,
                                  /*temporal_idx=*/0);
    return fresh_controller->ComputeQp(kFrameDuration, /*is_keyframe=*/false,
                                       /*temporal_idx=*/0);
  }();

  EXPECT_EQ(updated_qp, steady_qp);
  EXPECT_LT(updated_qp, fresh_first_p_qp);
}

TEST(D3D12SoftwareRateControllerTest,
     LongLowFpsGopKeepsConsistentOvershootResponse) {
  // At 1 fps a 3000-frame GOP spans 3000 seconds. Past 100 seconds a duration
  // derived from `gop_length / 30` went negative and clamped the overshoot
  // correction window from 800 ms down to 200 ms for the rest of the GOP.
  auto encode_idle_then_burst = [](int idle_seconds) {
    constexpr uint32_t kBitrate = 1000000u;
    auto controller = CreateRateController(kBitrate);
    EXPECT_TRUE(controller);
    for (int s = 0; s < idle_seconds; ++s) {
      const bool is_keyframe = s == 0;
      controller->ComputeQp(base::Seconds(s), is_keyframe, /*temporal_idx=*/0);
      controller->FinishFrame(15000, is_keyframe, /*temporal_idx=*/0);
    }
    // A sudden scroll burst fills past `buffer_target_high` (20% of the 125 KB
    // HRD buffer = 25 KB) without overflowing the whole buffer.
    uint32_t qp = 0;
    for (int i = 1; i <= 3; ++i) {
      const base::TimeDelta ts =
          base::Seconds(idle_seconds - 1) + i * kFrameDuration;
      qp = controller->ComputeQp(ts, /*is_keyframe=*/false, /*temporal_idx=*/0);
      controller->FinishFrame(25000, /*is_keyframe=*/false, /*temporal_idx=*/0);
    }
    return qp;
  };

  EXPECT_EQ(encode_idle_then_burst(/*idle_seconds=*/50),
            encode_idle_then_burst(/*idle_seconds=*/150));
}

TEST(D3D12SoftwareRateControllerTest, QpStaysWithinMaxQp) {
  // The frames are far larger than the bitrate allows.
  constexpr uint32_t kMaxQp = 35;
  auto rate_controller = CreateRateController(100000u, kMaxQp);
  ASSERT_TRUE(rate_controller);
  for (int i = 0; i < 30; ++i) {
    const bool is_keyframe = i == 0;
    EXPECT_LE(rate_controller->ComputeQp(i * kFrameDuration, is_keyframe,
                                         /*temporal_idx=*/0),
              kMaxQp);
    rate_controller->FinishFrame(200000, is_keyframe, /*temporal_idx=*/0);
  }
}

TEST(D3D12SoftwareRateControllerTest, LowerQpWhenFramesArriveLessOften) {
  // The same frames get a 30 times larger bit budget at 1 fps than at 30 fps.
  constexpr size_t kFrameSize = 20000;
  auto rate_controller_at_30fps = CreateRateController(1000000u);
  auto rate_controller_at_1fps = CreateRateController(1000000u);
  ASSERT_TRUE(rate_controller_at_30fps);
  ASSERT_TRUE(rate_controller_at_1fps);
  EXPECT_LT(
      EncodeFrames(*rate_controller_at_1fps, 30, kFrameSize, base::Seconds(1)),
      EncodeFrames(*rate_controller_at_30fps, 30, kFrameSize, kFrameDuration));
}

TEST(D3D12SoftwareRateControllerTest, FramesWithinBitrateKeepLowerQp) {
  // Scrolling a text page produces inter frames of tens of KB. At 20 Mbps and
  // 30 fps, 60 KB frames use about 70% of the bitrate, so they must end up
  // with a lower QP than 120 KB frames, which exceed it. A small HRD buffer
  // would overflow on every frame of both sizes and push both to the max QP.
  constexpr uint32_t kBitrate = 20000000u;
  auto rate_controller_within_bitrate = CreateRateController(kBitrate);
  auto rate_controller_over_bitrate = CreateRateController(kBitrate);
  ASSERT_TRUE(rate_controller_within_bitrate);
  ASSERT_TRUE(rate_controller_over_bitrate);
  EXPECT_LT(EncodeFrames(*rate_controller_within_bitrate, 60,
                         /*frame_size=*/60000, kFrameDuration),
            EncodeFrames(*rate_controller_over_bitrate, 60,
                         /*frame_size=*/120000, kFrameDuration));
}

TEST(D3D12SoftwareRateControllerTest, ExtremeTimestampsDoNotCrash) {
  auto rate_controller = CreateRateController(1000000u, /*max_qp=*/35);
  ASSERT_TRUE(rate_controller);

  constexpr base::TimeDelta kWebCodecsMax = base::Microseconds((1LL << 53) - 1);
  constexpr base::TimeDelta kWebCodecsMin = -kWebCodecsMax;

  constexpr auto kEdgeTimestamps = std::to_array<base::TimeDelta>({
      base::TimeDelta::Min(),
      base::TimeDelta::Max(),
      base::TimeDelta::Max(),
      base::TimeDelta::Min(),
      kWebCodecsMax,
      kWebCodecsMin,
  });
  for (size_t i = 0; i < kEdgeTimestamps.size(); ++i) {
    const bool is_keyframe = i == 0;
    const uint32_t qp = rate_controller->ComputeQp(
        kEdgeTimestamps[i], is_keyframe, /*temporal_idx=*/0);
    EXPECT_LE(qp, 35u);
    rate_controller->FinishFrame(10000, is_keyframe, /*temporal_idx=*/0);
  }

  // Alternating +/-WebCodecs limits across thousands of frames previously
  // saturated the mapped timeline to TimeDelta::Max() and crashed in
  // ClampedTimestampDiff().
  for (int i = 0; i < 2000; ++i) {
    const base::TimeDelta ts = (i % 2 == 0) ? kWebCodecsMax : kWebCodecsMin;
    const uint32_t qp = rate_controller->ComputeQp(ts, /*is_keyframe=*/false,
                                                   /*temporal_idx=*/0);
    EXPECT_LE(qp, 35u);
    rate_controller->FinishFrame(10000, /*is_keyframe=*/false,
                                 /*temporal_idx=*/0);
  }
}

TEST(RateControllerTimestampMapperTest, FollowsFrameTimestamps) {
  RateControllerTimestampMapper mapper;
  // The timeline starts at zero, whatever the first input timestamp is.
  EXPECT_EQ(mapper.Map(base::Seconds(10), kFrameDuration), base::TimeDelta());
  // Frames that arrive at 1 fps stay 1 second apart.
  EXPECT_EQ(mapper.Map(base::Seconds(11), kFrameDuration), base::Seconds(1));
  EXPECT_EQ(mapper.Map(base::Seconds(12), kFrameDuration), base::Seconds(2));
  EXPECT_EQ(mapper.Map(base::Milliseconds(12010), kFrameDuration),
            base::Milliseconds(2010));
}

TEST(RateControllerTimestampMapperTest, KeepsTimelineIncreasing) {
  RateControllerTimestampMapper mapper;
  EXPECT_EQ(mapper.Map(base::Seconds(0), kFrameDuration), base::TimeDelta());
  EXPECT_EQ(mapper.Map(base::Seconds(1), kFrameDuration), base::Seconds(1));
  // A repeated timestamp moves forward by one frame.
  EXPECT_EQ(mapper.Map(base::Seconds(1), kFrameDuration),
            base::Seconds(1) + kFrameDuration);
  // A small backward step also moves forward by one frame.
  EXPECT_EQ(mapper.Map(base::Milliseconds(900), kFrameDuration),
            base::Seconds(1) + 2 * kFrameDuration);
  // Later frames follow the input again.
  EXPECT_EQ(mapper.Map(base::Seconds(2), kFrameDuration), base::Seconds(2));
}

TEST(RateControllerTimestampMapperTest, RebasesAfterTimestampReset) {
  RateControllerTimestampMapper mapper;
  EXPECT_EQ(mapper.Map(base::Seconds(0), kFrameDuration), base::TimeDelta());
  EXPECT_EQ(mapper.Map(base::Seconds(5), kFrameDuration), base::Seconds(5));
  // The source restarts from zero. The timeline continues one frame later and
  // keeps the spacing of the new input timestamps.
  EXPECT_EQ(mapper.Map(base::Seconds(0), kFrameDuration),
            base::Seconds(5) + kFrameDuration);
  EXPECT_EQ(mapper.Map(base::Seconds(1), kFrameDuration),
            base::Seconds(6) + kFrameDuration);
}

TEST(RateControllerTimestampMapperTest, ClampsMinMaxAndBoundsAlternatingJumps) {
  RateControllerTimestampMapper mapper;
  EXPECT_EQ(mapper.Map(base::TimeDelta::Min(), kFrameDuration),
            base::TimeDelta());
  // A jump to TimeDelta::Max() is bounded to 5 minutes.
  EXPECT_EQ(mapper.Map(base::TimeDelta::Max(), kFrameDuration),
            base::Minutes(5));
  // Repeating TimeDelta::Max() advances by one nominal frame without saturating
  // or crashing.
  EXPECT_EQ(mapper.Map(base::TimeDelta::Max(), kFrameDuration),
            base::Minutes(5) + kFrameDuration);
  // Jumping back to TimeDelta::Min() rebases one frame ahead.
  EXPECT_EQ(mapper.Map(base::TimeDelta::Min(), kFrameDuration),
            base::Minutes(5) + 2 * kFrameDuration);

  // Alternate across the full WebCodecs timestamp range (+/-(2^53 - 1) us).
  constexpr base::TimeDelta kWebCodecsMax = base::Microseconds((1LL << 53) - 1);
  constexpr base::TimeDelta kWebCodecsMin = -kWebCodecsMax;
  base::TimeDelta prev = base::Minutes(5) + 2 * kFrameDuration;
  for (int i = 0; i < 2000; ++i) {
    const base::TimeDelta input = (i % 2 == 0) ? kWebCodecsMax : kWebCodecsMin;
    const base::TimeDelta mapped = mapper.Map(input, kFrameDuration);
    EXPECT_FALSE(mapped.is_inf());
    EXPECT_GT(mapped, prev);
    EXPECT_LE(mapped - prev, base::Minutes(5));
    prev = mapped;
  }
}

}  // namespace media
