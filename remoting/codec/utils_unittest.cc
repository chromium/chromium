// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/utils.h"

#include <cstdint>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "remoting/base/cpu_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

namespace {

constexpr uint8_t kSentinel = 0xCD;

using LibyuvArgbToYuvFn = int (*)(const uint8_t* src_argb,
                                  int src_stride_argb,
                                  uint8_t* dst_y,
                                  int dst_stride_y,
                                  uint8_t* dst_u,
                                  int dst_stride_u,
                                  uint8_t* dst_v,
                                  int dst_stride_v,
                                  int width,
                                  int height);

struct RowAlignTestParam {
  const char* name;
  int align;
};

struct RegionAlignmentTestParam {
  const char* name;
  int align;
  bool is_i444;
  int uv_divisor;
  LibyuvArgbToYuvFn ref_convert_fn;
};

enum class RegionShape {
  kFullFrame,
  kMultiRect,
};

struct RegionParallelTestParam {
  const char* name;
  RegionShape region_shape;
  bool is_i444;
  int u_stride;
  int v_stride;
  int uv_divisor;
};

struct YuvSubsamplingTestParam {
  const char* name;
  bool is_i444;
  int uv_divisor;
};

struct FrameSizeParam {
  const char* name;
  int width;
  int height;
};

using LibyuvYuvToRgbFn = int (*)(const uint8_t* src_y,
                                 int src_stride_y,
                                 const uint8_t* src_u,
                                 int src_stride_u,
                                 const uint8_t* src_v,
                                 int src_stride_v,
                                 uint8_t* dst_rgb,
                                 int dst_stride_rgb,
                                 int width,
                                 int height);

struct YuvToRgbTestParam {
  const char* name;
  int width;
  int height;
  bool is_i444;
  int uv_divisor;
  int y_stride;
  int u_stride;
  int v_stride;
  webrtc::FourCC output_fourcc;
  LibyuvYuvToRgbFn ref_convert_fn;
};

// Populates an ARGB frame with deterministic, spatially varying pixel values.
void FillPatternFrame(webrtc::DesktopFrame& frame) {
  const int row_bytes =
      frame.size().width() * webrtc::DesktopFrame::kBytesPerPixel;
  std::vector<uint8_t> pixels(
      static_cast<size_t>(row_bytes * frame.size().height()));
  for (int y = 0; y < frame.size().height(); ++y) {
    for (int x = 0; x < frame.size().width(); ++x) {
      const size_t offset = static_cast<size_t>(
          y * row_bytes + x * webrtc::DesktopFrame::kBytesPerPixel);
      pixels[offset] = static_cast<uint8_t>((x * 7 + y * 13) & 0xFF);
      pixels[offset + 1] = static_cast<uint8_t>((x * 17 + y * 5 + 40) & 0xFF);
      pixels[offset + 2] = static_cast<uint8_t>((x * 3 + y * 23 + 80) & 0xFF);
      pixels[offset + 3] = 0xFF;
    }
  }
  frame.CopyPixelsFrom(pixels.data(), row_bytes,
                       webrtc::DesktopRect::MakeSize(frame.size()));
}

webrtc::DesktopRegion BuildParallelTestRegion(RegionShape shape,
                                              int width,
                                              int height) {
  if (shape == RegionShape::kFullFrame) {
    return webrtc::DesktopRegion(webrtc::DesktopRect::MakeWH(width, height));
  }
  CHECK_GE(height, 540);
  // Mixes a tall rect (split across dynamic strips) and short rects
  // (`height == 4`, kept intact without splitting) totaling >= 512k pixels.
  webrtc::DesktopRegion region;
  region.AddRect(webrtc::DesktopRect::MakeLTRB(0, 0, width, 514));
  region.AddRect(webrtc::DesktopRect::MakeLTRB(0, 520, width, 524));
  region.AddRect(webrtc::DesktopRect::MakeLTRB(0, 528, width, 532));
  region.AddRect(webrtc::DesktopRect::MakeLTRB(0, 536, width, 540));
  return region;
}

// Initializes a `TaskEnvironment` and pins every foreground `ThreadPool` worker
// on a `TestWaitableEvent` (`Wait()` on `TestWaitableEvent` omits
// `ScopedBlockingCall`, preventing `ThreadPool` from spawning replacement
// workers). Static `PostTask` + `BarrierClosure` fan-out would deadlock in this
// state because `release_workers_` is only signaled on destruction;
// `base::CreateJob(...).Join()` drains all strips inline on the calling thread
// instead.
class ScopedSaturateForegroundThreadPool {
 public:
  ScopedSaturateForegroundThreadPool() {
    base::RepeatingClosure occupied_barrier = base::BarrierClosure(
        base::test::TaskEnvironment::kNumForegroundThreadPoolThreads,
        base::BindOnce(&base::TestWaitableEvent::Signal,
                       base::Unretained(&workers_occupied_)));
    for (int i = 0;
         i < base::test::TaskEnvironment::kNumForegroundThreadPoolThreads;
         ++i) {
      base::ThreadPool::PostTask(
          FROM_HERE, {base::TaskPriority::USER_BLOCKING},
          base::BindOnce(
              [](base::RepeatingClosure on_occupied,
                 base::TestWaitableEvent* release) {
                on_occupied.Run();
                release->Wait();
              },
              occupied_barrier, base::Unretained(&release_workers_)));
    }
    workers_occupied_.Wait();
  }

  ScopedSaturateForegroundThreadPool(
      const ScopedSaturateForegroundThreadPool&) = delete;
  ScopedSaturateForegroundThreadPool& operator=(
      const ScopedSaturateForegroundThreadPool&) = delete;

  ~ScopedSaturateForegroundThreadPool() {
    release_workers_.Signal();
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

 private:
  // Declared before `task_environment_` so both events outlive
  // `~TaskEnvironment()`'s `ThreadPool` worker-thread join.
  base::TestWaitableEvent workers_occupied_;
  base::TestWaitableEvent release_workers_;
  base::test::TaskEnvironment task_environment_;
};

void ExpectDesktopFramesEqual(const webrtc::DesktopFrame& actual,
                              const webrtc::DesktopFrame& expected) {
  ASSERT_TRUE(actual.size().equals(expected.size()));
  for (int y = 0; y < actual.size().height(); ++y) {
    for (int x = 0; x < actual.size().width(); ++x) {
      const webrtc::DesktopVector pos(x, y);
      const uint32_t actual_px =
          *reinterpret_cast<const uint32_t*>(actual.GetFrameDataAtPos(pos));
      const uint32_t expected_px =
          *reinterpret_cast<const uint32_t*>(expected.GetFrameDataAtPos(pos));
      ASSERT_EQ(actual_px, expected_px)
          << "Mismatch at (" << x << ", " << y << ")";
    }
  }
}

}  // namespace

TEST(CodecUtilsTest, GetBoundingRect_EmptyRegion) {
  webrtc::DesktopRegion region;
  EXPECT_TRUE(GetBoundingRect(region).is_empty());
}

TEST(CodecUtilsTest, GetBoundingRect_MultipleRects) {
  webrtc::DesktopRegion region;
  region.AddRect(webrtc::DesktopRect::MakeLTRB(10, 20, 30, 40));
  region.AddRect(webrtc::DesktopRect::MakeLTRB(5, 50, 60, 80));
  EXPECT_TRUE(GetBoundingRect(region).equals(
      webrtc::DesktopRect::MakeLTRB(5, 20, 60, 80)));
}

TEST(CodecUtilsTest, ConvertDesktopFrameRectToYuv_EmptyRectIsNoOp) {
  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(16, 16));
  FillPatternFrame(frame);

  std::vector<uint8_t> y_plane(16 * 16, kSentinel);
  std::vector<uint8_t> u_plane(16 * 16, kSentinel);
  std::vector<uint8_t> v_plane(16 * 16, kSentinel);

  ConvertDesktopFrameRectToYuv(frame, webrtc::DesktopRect(), /*is_i444=*/false,
                               y_plane, 16, u_plane, 8, v_plane, 8);
  for (uint8_t b : y_plane) {
    EXPECT_EQ(b, kSentinel);
  }
}

TEST(CodecUtilsTest, ConvertDesktopFrameRectToYuv_I420SubRectAndStrides) {
  constexpr int kWidth = 32;
  constexpr int kHeight = 24;
  // Use distinct padded strides for Y, U, and V to verify independent stride
  // arithmetic.
  constexpr int kYStride = 40;
  constexpr int kUStride = 24;
  constexpr int kVStride = 28;

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(kWidth, kHeight));
  FillPatternFrame(frame);

  // Compute full-frame reference planes via libyuv.
  std::vector<uint8_t> ref_y(kYStride * kHeight, 0);
  std::vector<uint8_t> ref_u(kUStride * (kHeight / 2), 0);
  std::vector<uint8_t> ref_v(kVStride * (kHeight / 2), 0);
  libyuv::ARGBToI420(frame.data(), frame.stride(), ref_y.data(), kYStride,
                     ref_u.data(), kUStride, ref_v.data(), kVStride, kWidth,
                     kHeight);

  // Convert a non-zero offset sub-rectangle with even coordinates and odd
  // top/2 (e.g., top = 6 -> top/2 = 3) to exercise chroma row offset math.
  std::vector<uint8_t> actual_y(kYStride * kHeight, kSentinel);
  std::vector<uint8_t> actual_u(kUStride * (kHeight / 2), kSentinel);
  std::vector<uint8_t> actual_v(kVStride * (kHeight / 2), kSentinel);

  const webrtc::DesktopRect sub_rect =
      webrtc::DesktopRect::MakeLTRB(4, 6, 20, 18);
  ConvertDesktopFrameRectToYuv(frame, sub_rect, /*is_i444=*/false, actual_y,
                               kYStride, actual_u, kUStride, actual_v,
                               kVStride);

  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kYStride; ++x) {
      const size_t idx = static_cast<size_t>(y * kYStride + x);
      if (x >= sub_rect.left() && x < sub_rect.right() && y >= sub_rect.top() &&
          y < sub_rect.bottom()) {
        EXPECT_EQ(actual_y[idx], ref_y[idx])
            << "Y mismatch at (" << x << ", " << y << ")";
      } else {
        EXPECT_EQ(actual_y[idx], kSentinel)
            << "Y overwritten at (" << x << ", " << y << ")";
      }
    }
  }

  for (int uv_y = 0; uv_y < kHeight / 2; ++uv_y) {
    const bool in_y =
        uv_y >= sub_rect.top() / 2 && uv_y < sub_rect.bottom() / 2;
    for (int uv_x = 0; uv_x < kUStride; ++uv_x) {
      const size_t idx = static_cast<size_t>(uv_y * kUStride + uv_x);
      if (in_y && uv_x >= sub_rect.left() / 2 && uv_x < sub_rect.right() / 2) {
        EXPECT_EQ(actual_u[idx], ref_u[idx])
            << "U mismatch at (" << uv_x << ", " << uv_y << ")";
      } else {
        EXPECT_EQ(actual_u[idx], kSentinel)
            << "U overwritten at (" << uv_x << ", " << uv_y << ")";
      }
    }
    for (int uv_x = 0; uv_x < kVStride; ++uv_x) {
      const size_t idx = static_cast<size_t>(uv_y * kVStride + uv_x);
      if (in_y && uv_x >= sub_rect.left() / 2 && uv_x < sub_rect.right() / 2) {
        EXPECT_EQ(actual_v[idx], ref_v[idx])
            << "V mismatch at (" << uv_x << ", " << uv_y << ")";
      } else {
        EXPECT_EQ(actual_v[idx], kSentinel)
            << "V overwritten at (" << uv_x << ", " << uv_y << ")";
      }
    }
  }
}

TEST(CodecUtilsTest, ConvertDesktopFrameRectToYuv_I444SubRectAndStrides) {
  constexpr int kWidth = 24;
  constexpr int kHeight = 20;
  constexpr int kYStride = 32;
  constexpr int kUStride = 36;
  constexpr int kVStride = 40;

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(kWidth, kHeight));
  FillPatternFrame(frame);

  std::vector<uint8_t> ref_y(kYStride * kHeight, 0);
  std::vector<uint8_t> ref_u(kUStride * kHeight, 0);
  std::vector<uint8_t> ref_v(kVStride * kHeight, 0);
  libyuv::ARGBToI444(frame.data(), frame.stride(), ref_y.data(), kYStride,
                     ref_u.data(), kUStride, ref_v.data(), kVStride, kWidth,
                     kHeight);

  std::vector<uint8_t> actual_y(kYStride * kHeight, kSentinel);
  std::vector<uint8_t> actual_u(kUStride * kHeight, kSentinel);
  std::vector<uint8_t> actual_v(kVStride * kHeight, kSentinel);

  // I444 supports odd top-left coordinates as well.
  const webrtc::DesktopRect sub_rect =
      webrtc::DesktopRect::MakeLTRB(3, 5, 19, 17);
  ConvertDesktopFrameRectToYuv(frame, sub_rect, /*is_i444=*/true, actual_y,
                               kYStride, actual_u, kUStride, actual_v,
                               kVStride);

  for (int y = 0; y < kHeight; ++y) {
    const bool in_y = y >= sub_rect.top() && y < sub_rect.bottom();
    for (int x = 0; x < kWidth; ++x) {
      const bool in_rect = in_y && x >= sub_rect.left() && x < sub_rect.right();
      const size_t y_idx = static_cast<size_t>(y * kYStride + x);
      const size_t u_idx = static_cast<size_t>(y * kUStride + x);
      const size_t v_idx = static_cast<size_t>(y * kVStride + x);
      if (in_rect) {
        EXPECT_EQ(actual_y[y_idx], ref_y[y_idx]);
        EXPECT_EQ(actual_u[u_idx], ref_u[u_idx]);
        EXPECT_EQ(actual_v[v_idx], ref_v[v_idx]);
      } else {
        EXPECT_EQ(actual_y[y_idx], kSentinel);
        EXPECT_EQ(actual_u[u_idx], kSentinel);
        EXPECT_EQ(actual_v[v_idx], kSentinel);
      }
    }
  }
}

class GetRowAlignedRegionTest
    : public testing::TestWithParam<RowAlignTestParam> {};

TEST_P(GetRowAlignedRegionTest, CoalescesOverlappingWidenedRects) {
  const int align = GetParam().align;
  ScopedSimdMemoryAlignmentForTesting scoped_align(align);
  const int width = align * 2;

  // The two input rects (`x:[2, 6), y:[4, 12)` and `x:[10, 14), y:[6, 14)`)
  // are disjoint, but once `GetRowAlignedRect()` widens both X-ranges to
  // `[0, align)`, they overlap on rows `y:[6, 12)` and coalesce into a single
  // `MakeLTRB(0, 4, align, 14)` rectangle.
  webrtc::DesktopRegion region;
  region.AddRect(webrtc::DesktopRect::MakeLTRB(2, 4, 6, 12));
  region.AddRect(webrtc::DesktopRect::MakeLTRB(10, 6, 14, 14));

  const webrtc::DesktopRegion aligned_region =
      GetRowAlignedRegion(region, width);
  EXPECT_TRUE(aligned_region.Equals(
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeLTRB(0, 4, align, 14))));

  std::vector<webrtc::DesktopRect> rects;
  for (webrtc::DesktopRegion::Iterator r(aligned_region); !r.IsAtEnd();
       r.Advance()) {
    rects.push_back(r.rect());
  }
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_TRUE(rects[0].equals(webrtc::DesktopRect::MakeLTRB(0, 4, align, 14)));
}

TEST_P(GetRowAlignedRegionTest, CoalescesAbuttingClippedWidenedRects) {
  const int align = GetParam().align;
  ScopedSimdMemoryAlignmentForTesting scoped_align(align);

  // `MakeLTRB(2, 4, 6, 14)` widens to `[0, align)`, while
  // `MakeLTRB(align + 2, 4, align + 6, 14)` in the next SIMD block widens and
  // clips to `[align, align + 8)` on a frame of width `align + 8`, abutting
  // horizontally and coalescing into `MakeLTRB(0, 4, align + 8, 14)`.
  webrtc::DesktopRegion region;
  region.AddRect(webrtc::DesktopRect::MakeLTRB(2, 4, 6, 14));
  region.AddRect(webrtc::DesktopRect::MakeLTRB(align + 2, 4, align + 6, 14));

  EXPECT_TRUE(GetRowAlignedRegion(region, align + 8)
                  .Equals(webrtc::DesktopRegion(
                      webrtc::DesktopRect::MakeLTRB(0, 4, align + 8, 14))));
}

constexpr RowAlignTestParam kRowAlignParams[] = {
    {"Align16", 16},
    {"Align32", 32},
    {"Align64", 64},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    GetRowAlignedRegionTest,
    testing::ValuesIn(kRowAlignParams),
    [](const testing::TestParamInfo<RowAlignTestParam>& info) {
      return info.param.name;
    });

class ConvertDesktopFrameRegionToYuvAlignmentTest
    : public testing::TestWithParam<RegionAlignmentTestParam> {};

TEST_P(ConvertDesktopFrameRegionToYuvAlignmentTest, RowAlignmentAndClipping) {
  const RegionAlignmentTestParam& param = GetParam();
  ScopedSimdMemoryAlignmentForTesting scoped_align(param.align);
  const int width = param.align + 6;
  constexpr int kHeight = 16;
  const int y_stride = param.align * 2;
  const int uv_stride = y_stride / param.uv_divisor;
  const int uv_height = kHeight / param.uv_divisor;

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(width, kHeight));
  FillPatternFrame(frame);

  // Input rect `[align + 2, 4, width, 12]` is row-aligned by
  // `GetRowAlignedRect()` to `[align, 4, width, 12]` (right edge clipped to
  // `width`).
  const webrtc::DesktopRegion region(
      webrtc::DesktopRect::MakeLTRB(param.align + 2, 4, width, 12));
  const webrtc::DesktopRect expected_aligned =
      webrtc::DesktopRect::MakeLTRB(param.align, 4, width, 12);

  std::vector<uint8_t> ref_y(y_stride * kHeight, 0);
  std::vector<uint8_t> ref_u(uv_stride * uv_height, 0);
  std::vector<uint8_t> ref_v(uv_stride * uv_height, 0);
  param.ref_convert_fn(frame.data(), frame.stride(), ref_y.data(), y_stride,
                       ref_u.data(), uv_stride, ref_v.data(), uv_stride, width,
                       kHeight);

  std::vector<uint8_t> actual_y(y_stride * kHeight, kSentinel);
  std::vector<uint8_t> actual_u(uv_stride * uv_height, kSentinel);
  std::vector<uint8_t> actual_v(uv_stride * uv_height, kSentinel);
  ConvertDesktopFrameRegionToYuv(frame, region, param.is_i444, actual_y,
                                 y_stride, actual_u, uv_stride, actual_v,
                                 uv_stride);

  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < y_stride; ++x) {
      const size_t idx = static_cast<size_t>(y * y_stride + x);
      if (expected_aligned.Contains(webrtc::DesktopVector(x, y))) {
        EXPECT_EQ(actual_y[idx], ref_y[idx])
            << "Y mismatch at (" << x << ", " << y << ")";
      } else {
        EXPECT_EQ(actual_y[idx], kSentinel)
            << "Y overwritten at (" << x << ", " << y << ")";
      }
    }
  }

  for (int uv_y = 0; uv_y < uv_height; ++uv_y) {
    const bool in_y = uv_y >= expected_aligned.top() / param.uv_divisor &&
                      uv_y < expected_aligned.bottom() / param.uv_divisor;
    for (int uv_x = 0; uv_x < uv_stride; ++uv_x) {
      const size_t idx = static_cast<size_t>(uv_y * uv_stride + uv_x);
      if (in_y && uv_x >= expected_aligned.left() / param.uv_divisor &&
          uv_x < expected_aligned.right() / param.uv_divisor) {
        EXPECT_EQ(actual_u[idx], ref_u[idx])
            << "U mismatch at (" << uv_x << ", " << uv_y << ")";
        EXPECT_EQ(actual_v[idx], ref_v[idx])
            << "V mismatch at (" << uv_x << ", " << uv_y << ")";
      } else {
        EXPECT_EQ(actual_u[idx], kSentinel)
            << "U overwritten at (" << uv_x << ", " << uv_y << ")";
        EXPECT_EQ(actual_v[idx], kSentinel)
            << "V overwritten at (" << uv_x << ", " << uv_y << ")";
      }
    }
  }
}

TEST_P(ConvertDesktopFrameRegionToYuvAlignmentTest,
       OverlappingAlignedRectsProduceCorrectOutput) {
  const RegionAlignmentTestParam& param = GetParam();
  ScopedSimdMemoryAlignmentForTesting scoped_align(param.align);
  const int width = param.align * 2;
  constexpr int kHeight = 16;
  const int y_stride = width;
  const int uv_stride = width / param.uv_divisor;
  const int uv_height = kHeight / param.uv_divisor;

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(width, kHeight));
  FillPatternFrame(frame);

  // `MakeLTRB(2, 4, 6, 12)` and `MakeLTRB(10, 6, 14, 14)` are initially
  // disjoint and overlap on `x:[0, align), y:[6, 12)` once
  // `GetRowAlignedRect()` widens both X-ranges to `[0, align)`.
  webrtc::DesktopRegion region;
  region.AddRect(webrtc::DesktopRect::MakeLTRB(2, 4, 6, 12));
  region.AddRect(webrtc::DesktopRect::MakeLTRB(10, 6, 14, 14));

  std::vector<uint8_t> actual_y(y_stride * kHeight, kSentinel);
  std::vector<uint8_t> actual_u(uv_stride * uv_height, kSentinel);
  std::vector<uint8_t> actual_v(uv_stride * uv_height, kSentinel);
  ConvertDesktopFrameRegionToYuv(frame, region, param.is_i444, actual_y,
                                 y_stride, actual_u, uv_stride, actual_v,
                                 uv_stride);

  std::vector<uint8_t> expected_y(y_stride * kHeight, kSentinel);
  std::vector<uint8_t> expected_u(uv_stride * uv_height, kSentinel);
  std::vector<uint8_t> expected_v(uv_stride * uv_height, kSentinel);
  ConvertDesktopFrameRectToYuv(
      frame, webrtc::DesktopRect::MakeLTRB(0, 4, param.align, 14),
      param.is_i444, expected_y, y_stride, expected_u, uv_stride, expected_v,
      uv_stride);

  EXPECT_EQ(actual_y, expected_y);
  EXPECT_EQ(actual_u, expected_u);
  EXPECT_EQ(actual_v, expected_v);
}

constexpr RegionAlignmentTestParam kRegionAlignmentParams[] = {
    {"Align16_I420", 16, /*is_i444=*/false, /*uv_divisor=*/2,
     &libyuv::ARGBToI420},
    {"Align32_I420", 32, /*is_i444=*/false, /*uv_divisor=*/2,
     &libyuv::ARGBToI420},
    {"Align64_I420", 64, /*is_i444=*/false, /*uv_divisor=*/2,
     &libyuv::ARGBToI420},
    {"Align16_I444", 16, /*is_i444=*/true, /*uv_divisor=*/1,
     &libyuv::ARGBToI444},
    {"Align32_I444", 32, /*is_i444=*/true, /*uv_divisor=*/1,
     &libyuv::ARGBToI444},
    {"Align64_I444", 64, /*is_i444=*/true, /*uv_divisor=*/1,
     &libyuv::ARGBToI444},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ConvertDesktopFrameRegionToYuvAlignmentTest,
    testing::ValuesIn(kRegionAlignmentParams),
    [](const testing::TestParamInfo<RegionAlignmentTestParam>& info) {
      return info.param.name;
    });

class ConvertDesktopFrameRegionToYuvParallelTest
    : public testing::TestWithParam<RegionParallelTestParam> {};

TEST_P(ConvertDesktopFrameRegionToYuvParallelTest,
       ParallelJobMatchesSinglePass) {
  base::test::TaskEnvironment task_environment;
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);
  const RegionParallelTestParam& param = GetParam();

  // 1024 * 542 = 555,008 pixels (> `kParallelYuvConversionPixelThreshold`).
  // `542` is not divisible by 16, exercising `RoundToTwosMultiple` across
  // uneven horizontal strip heights so I420 chroma rows remain aligned.
  constexpr int kWidth = 1024;
  constexpr int kHeight = 542;
  constexpr int kYStride = 1088;
  const int uv_height = kHeight / param.uv_divisor;

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(kWidth, kHeight));
  FillPatternFrame(frame);

  const webrtc::DesktopRegion region =
      BuildParallelTestRegion(param.region_shape, kWidth, kHeight);
  const webrtc::DesktopRegion aligned_region =
      GetRowAlignedRegion(region, kWidth);

  std::vector<uint8_t> expected_y(kYStride * kHeight, kSentinel);
  std::vector<uint8_t> expected_u(param.u_stride * uv_height, kSentinel);
  std::vector<uint8_t> expected_v(param.v_stride * uv_height, kSentinel);
  for (webrtc::DesktopRegion::Iterator r(aligned_region); !r.IsAtEnd();
       r.Advance()) {
    ConvertDesktopFrameRectToYuv(frame, r.rect(), param.is_i444, expected_y,
                                 kYStride, expected_u, param.u_stride,
                                 expected_v, param.v_stride);
  }

  std::vector<uint8_t> actual_y(kYStride * kHeight, kSentinel);
  std::vector<uint8_t> actual_u(param.u_stride * uv_height, kSentinel);
  std::vector<uint8_t> actual_v(param.v_stride * uv_height, kSentinel);
  ConvertDesktopFrameRegionToYuv(frame, region, param.is_i444, actual_y,
                                 kYStride, actual_u, param.u_stride, actual_v,
                                 param.v_stride);

  EXPECT_EQ(actual_y, expected_y);
  EXPECT_EQ(actual_u, expected_u);
  EXPECT_EQ(actual_v, expected_v);
}

constexpr RegionParallelTestParam kRegionParallelParams[] = {
    {"FullFrame_I420", RegionShape::kFullFrame, /*is_i444=*/false,
     /*u_stride=*/576, /*v_stride=*/608, /*uv_divisor=*/2},
    {"FullFrame_I444", RegionShape::kFullFrame, /*is_i444=*/true,
     /*u_stride=*/1104, /*v_stride=*/1120, /*uv_divisor=*/1},
    {"MultiRect_I420", RegionShape::kMultiRect, /*is_i444=*/false,
     /*u_stride=*/576, /*v_stride=*/608, /*uv_divisor=*/2},
    {"MultiRect_I444", RegionShape::kMultiRect, /*is_i444=*/true,
     /*u_stride=*/1104, /*v_stride=*/1120, /*uv_divisor=*/1},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ConvertDesktopFrameRegionToYuvParallelTest,
    testing::ValuesIn(kRegionParallelParams),
    [](const testing::TestParamInfo<RegionParallelTestParam>& info) {
      return info.param.name;
    });

class ConvertDesktopFrameRegionToYuvStarvedPoolTest
    : public testing::TestWithParam<YuvSubsamplingTestParam> {};

TEST_P(ConvertDesktopFrameRegionToYuvStarvedPoolTest,
       CompletesInlineWhenThreadPoolStarved) {
  ScopedSaturateForegroundThreadPool saturated_pool;
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);
  const YuvSubsamplingTestParam& param = GetParam();

  constexpr int kWidth = 1024;
  constexpr int kHeight = 512;  // 524,288 pixels >= 512,000.
  const int uv_stride = kWidth / param.uv_divisor;
  const int uv_height = kHeight / param.uv_divisor;

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(kWidth, kHeight));
  FillPatternFrame(frame);
  const webrtc::DesktopRegion region(
      webrtc::DesktopRect::MakeWH(kWidth, kHeight));

  std::vector<uint8_t> actual_y(kWidth * kHeight, kSentinel);
  std::vector<uint8_t> actual_u(uv_stride * uv_height, kSentinel);
  std::vector<uint8_t> actual_v(uv_stride * uv_height, kSentinel);
  ConvertDesktopFrameRegionToYuv(frame, region, param.is_i444, actual_y, kWidth,
                                 actual_u, uv_stride, actual_v, uv_stride);

  std::vector<uint8_t> expected_y(kWidth * kHeight, kSentinel);
  std::vector<uint8_t> expected_u(uv_stride * uv_height, kSentinel);
  std::vector<uint8_t> expected_v(uv_stride * uv_height, kSentinel);
  ConvertDesktopFrameRectToYuv(
      frame, webrtc::DesktopRect::MakeWH(kWidth, kHeight), param.is_i444,
      expected_y, kWidth, expected_u, uv_stride, expected_v, uv_stride);

  EXPECT_EQ(actual_y, expected_y);
  EXPECT_EQ(actual_u, expected_u);
  EXPECT_EQ(actual_v, expected_v);
}

constexpr YuvSubsamplingTestParam kYuvSubsamplingParams[] = {
    {"I420", /*is_i444=*/false, /*uv_divisor=*/2},
    {"I444", /*is_i444=*/true, /*uv_divisor=*/1},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ConvertDesktopFrameRegionToYuvStarvedPoolTest,
    testing::ValuesIn(kYuvSubsamplingParams),
    [](const testing::TestParamInfo<YuvSubsamplingTestParam>& info) {
      return info.param.name;
    });

TEST(CodecUtilsTest,
     ConvertDesktopFrameRegionToYuv_NoThreadPoolRunsSinglePassAboveThreshold) {
  // Intentionally omit `TaskEnvironment` (`ThreadPoolInstance::Get()` is null)
  // to verify callers without an initialized `ThreadPool` fall back cleanly to
  // single-pass conversion above `kParallelYuvConversionPixelThreshold`.
  ASSERT_FALSE(base::ThreadPoolInstance::Get());
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);
  constexpr int kWidth = 1024;
  constexpr int kHeight = 512;  // 524,288 pixels >= 512,000.

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(kWidth, kHeight));
  FillPatternFrame(frame);
  const webrtc::DesktopRegion region(
      webrtc::DesktopRect::MakeWH(kWidth, kHeight));

  std::vector<uint8_t> actual_y(kWidth * kHeight, kSentinel);
  std::vector<uint8_t> actual_u((kWidth / 2) * (kHeight / 2), kSentinel);
  std::vector<uint8_t> actual_v((kWidth / 2) * (kHeight / 2), kSentinel);
  ConvertDesktopFrameRegionToYuv(frame, region, /*is_i444=*/false, actual_y,
                                 kWidth, actual_u, kWidth / 2, actual_v,
                                 kWidth / 2);

  std::vector<uint8_t> ref_y(kWidth * kHeight, 0);
  std::vector<uint8_t> ref_u((kWidth / 2) * (kHeight / 2), 0);
  std::vector<uint8_t> ref_v((kWidth / 2) * (kHeight / 2), 0);
  libyuv::ARGBToI420(frame.data(), frame.stride(), ref_y.data(), kWidth,
                     ref_u.data(), kWidth / 2, ref_v.data(), kWidth / 2, kWidth,
                     kHeight);
  EXPECT_EQ(actual_y, ref_y);
  EXPECT_EQ(actual_u, ref_u);
  EXPECT_EQ(actual_v, ref_v);
}

TEST(CodecUtilsTest, ConvertDesktopFrameToNv12_EmptyFrameIsNoOp) {
  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(0, 0));
  std::vector<uint8_t> y_plane(16, kSentinel);
  std::vector<uint8_t> uv_plane(16, kSentinel);

  ConvertDesktopFrameToNv12(frame, y_plane, 16, uv_plane, 16);
  EXPECT_EQ(y_plane, std::vector<uint8_t>(16, kSentinel));
  EXPECT_EQ(uv_plane, std::vector<uint8_t>(16, kSentinel));
}

class ConvertDesktopFrameToNv12BelowThresholdTest
    : public testing::TestWithParam<FrameSizeParam> {};

TEST_P(ConvertDesktopFrameToNv12BelowThresholdTest,
       MatchesSinglePassWithPaddedStridesAndTightSpans) {
  const FrameSizeParam& param = GetParam();
  const int width = param.width;
  const int height = param.height;
  const int uv_rows = (height + 1) / 2;
  const int uv_row_bytes = ((width + 1) / 2) * 2;
  // Use distinct padded Y/UV strides and tight visible-plane span bounds
  // (matching `media::VideoFrame::GetWritableVisiblePlaneData()`, which
  // excludes trailing stride padding on the final row).
  constexpr int kYStride = 48;
  constexpr int kUvStride = 56;

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(width, height));
  FillPatternFrame(frame);

  const size_t tight_y_size =
      static_cast<size_t>(kYStride) * (height - 1) + width;
  const size_t tight_uv_size =
      static_cast<size_t>(kUvStride) * (uv_rows - 1) + uv_row_bytes;

  std::vector<uint8_t> ref_y(tight_y_size, kSentinel);
  std::vector<uint8_t> ref_uv(tight_uv_size, kSentinel);
  libyuv::ARGBToNV12(frame.data(), frame.stride(), ref_y.data(), kYStride,
                     ref_uv.data(), kUvStride, width, height);

  std::vector<uint8_t> actual_y(tight_y_size, kSentinel);
  std::vector<uint8_t> actual_uv(tight_uv_size, kSentinel);
  ConvertDesktopFrameToNv12(frame, actual_y, kYStride, actual_uv, kUvStride);

  EXPECT_EQ(actual_y, ref_y);
  EXPECT_EQ(actual_uv, ref_uv);
}

constexpr FrameSizeParam kNv12BelowThresholdParams[] = {
    {"EvenBounds_32x24", 32, 24},
    {"OddBounds_33x25", 33, 25},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ConvertDesktopFrameToNv12BelowThresholdTest,
    testing::ValuesIn(kNv12BelowThresholdParams),
    [](const testing::TestParamInfo<FrameSizeParam>& info) {
      return info.param.name;
    });

class ConvertDesktopFrameToNv12ParallelTest
    : public testing::TestWithParam<FrameSizeParam> {};

TEST_P(ConvertDesktopFrameToNv12ParallelTest, ParallelJobMatchesSinglePass) {
  base::test::TaskEnvironment task_environment;
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);
  const FrameSizeParam& param = GetParam();
  const int width = param.width;
  const int height = param.height;
  const int uv_rows = (height + 1) / 2;
  const int uv_row_bytes = ((width + 1) / 2) * 2;
  constexpr int kYStride = 1088;
  constexpr int kUvStride = 1120;

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(width, height));
  FillPatternFrame(frame);

  const size_t tight_y_size =
      static_cast<size_t>(kYStride) * (height - 1) + width;
  const size_t tight_uv_size =
      static_cast<size_t>(kUvStride) * (uv_rows - 1) + uv_row_bytes;

  std::vector<uint8_t> expected_y(tight_y_size, kSentinel);
  std::vector<uint8_t> expected_uv(tight_uv_size, kSentinel);
  libyuv::ARGBToNV12(frame.data(), frame.stride(), expected_y.data(), kYStride,
                     expected_uv.data(), kUvStride, width, height);

  std::vector<uint8_t> actual_y(tight_y_size, kSentinel);
  std::vector<uint8_t> actual_uv(tight_uv_size, kSentinel);
  ConvertDesktopFrameToNv12(frame, actual_y, kYStride, actual_uv, kUvStride);

  EXPECT_EQ(actual_y, expected_y);
  EXPECT_EQ(actual_uv, expected_uv);
}

constexpr FrameSizeParam kNv12ParallelParams[] = {
    {"EvenBounds_1024x542", 1024, 542},
    {"OddBounds_1025x543", 1025, 543},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ConvertDesktopFrameToNv12ParallelTest,
    testing::ValuesIn(kNv12ParallelParams),
    [](const testing::TestParamInfo<FrameSizeParam>& info) {
      return info.param.name;
    });

TEST(CodecUtilsTest,
     ConvertDesktopFrameToNv12_CompletesInlineWhenThreadPoolStarved) {
  ScopedSaturateForegroundThreadPool saturated_pool;
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);

  constexpr int kWidth = 1024;
  constexpr int kHeight = 512;  // 524,288 pixels >= 512,000.
  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(kWidth, kHeight));
  FillPatternFrame(frame);

  std::vector<uint8_t> expected_y(kWidth * kHeight, kSentinel);
  std::vector<uint8_t> expected_uv(kWidth * (kHeight / 2), kSentinel);
  libyuv::ARGBToNV12(frame.data(), frame.stride(), expected_y.data(), kWidth,
                     expected_uv.data(), kWidth, kWidth, kHeight);

  std::vector<uint8_t> actual_y(kWidth * kHeight, kSentinel);
  std::vector<uint8_t> actual_uv(kWidth * (kHeight / 2), kSentinel);
  ConvertDesktopFrameToNv12(frame, actual_y, kWidth, actual_uv, kWidth);

  EXPECT_EQ(actual_y, expected_y);
  EXPECT_EQ(actual_uv, expected_uv);
}

TEST(CodecUtilsTest,
     ConvertDesktopFrameToNv12_NoThreadPoolRunsSinglePassAboveThreshold) {
  // Intentionally omit `TaskEnvironment` (`ThreadPoolInstance::Get()` is null)
  // to verify callers without an initialized `ThreadPool` fall back cleanly to
  // single-pass conversion above `kParallelYuvConversionPixelThreshold`.
  ASSERT_FALSE(base::ThreadPoolInstance::Get());
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);
  constexpr int kWidth = 1024;
  constexpr int kHeight = 512;  // 524,288 pixels >= 512,000.
  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(kWidth, kHeight));
  FillPatternFrame(frame);

  std::vector<uint8_t> actual_y(kWidth * kHeight, kSentinel);
  std::vector<uint8_t> actual_uv(kWidth * (kHeight / 2), kSentinel);
  ConvertDesktopFrameToNv12(frame, actual_y, kWidth, actual_uv, kWidth);

  std::vector<uint8_t> ref_y(kWidth * kHeight, 0);
  std::vector<uint8_t> ref_uv(kWidth * (kHeight / 2), 0);
  libyuv::ARGBToNV12(frame.data(), frame.stride(), ref_y.data(), kWidth,
                     ref_uv.data(), kWidth, kWidth, kHeight);
  EXPECT_EQ(actual_y, ref_y);
  EXPECT_EQ(actual_uv, ref_uv);
}

TEST(CodecUtilsTest, ConvertYuvToDesktopFrame_EmptyFrameIsNoOp) {
  webrtc::BasicDesktopFrame rgb_frame(webrtc::DesktopSize(0, 0));
  std::vector<uint8_t> plane(16, kSentinel);
  ConvertYuvToDesktopFrame(plane, 16, plane, 16, plane, 16, /*is_i444=*/false,
                           webrtc::FOURCC_ARGB, rgb_frame);
}

class ConvertYuvToDesktopFrameBelowThresholdTest
    : public testing::TestWithParam<YuvToRgbTestParam> {};

TEST_P(ConvertYuvToDesktopFrameBelowThresholdTest,
       MatchesSinglePassWithPaddedStridesAndTightSpans) {
  const YuvToRgbTestParam& param = GetParam();
  const int uv_width = (param.width + param.uv_divisor - 1) / param.uv_divisor;
  const int uv_height =
      (param.height + param.uv_divisor - 1) / param.uv_divisor;

  std::vector<uint8_t> y_plane(
      static_cast<size_t>(param.y_stride) * (param.height - 1) + param.width);
  std::vector<uint8_t> u_plane(
      static_cast<size_t>(param.u_stride) * (uv_height - 1) + uv_width);
  std::vector<uint8_t> v_plane(
      static_cast<size_t>(param.v_stride) * (uv_height - 1) + uv_width);
  for (size_t i = 0; i < y_plane.size(); ++i) {
    y_plane[i] = static_cast<uint8_t>(16 + (i * 7) % 210);
  }
  for (size_t i = 0; i < u_plane.size(); ++i) {
    u_plane[i] = static_cast<uint8_t>(24 + (i * 11) % 200);
  }
  for (size_t i = 0; i < v_plane.size(); ++i) {
    v_plane[i] = static_cast<uint8_t>(24 + (i * 13) % 200);
  }

  const webrtc::DesktopSize size(param.width, param.height);
  webrtc::BasicDesktopFrame expected_rgb(size);
  param.ref_convert_fn(y_plane.data(), param.y_stride, u_plane.data(),
                       param.u_stride, v_plane.data(), param.v_stride,
                       expected_rgb.data(), expected_rgb.stride(), param.width,
                       param.height);

  webrtc::BasicDesktopFrame actual_rgb(size);
  ConvertYuvToDesktopFrame(y_plane, param.y_stride, u_plane, param.u_stride,
                           v_plane, param.v_stride, param.is_i444,
                           param.output_fourcc, actual_rgb);

  ExpectDesktopFramesEqual(actual_rgb, expected_rgb);
}

constexpr YuvToRgbTestParam kYuvToRgbBelowThresholdParams[] = {
    {"EvenBounds_32x24_I420_ARGB", 32, 24, /*is_i444=*/false, /*uv_divisor=*/2,
     48, 40, 44, webrtc::FOURCC_ARGB, &libyuv::I420ToARGB},
    {"EvenBounds_32x24_I420_ABGR", 32, 24, /*is_i444=*/false, /*uv_divisor=*/2,
     48, 40, 44, webrtc::FOURCC_ABGR, &libyuv::I420ToABGR},
    {"EvenBounds_32x24_I444_ARGB", 32, 24, /*is_i444=*/true, /*uv_divisor=*/1,
     48, 40, 44, webrtc::FOURCC_ARGB, &libyuv::I444ToARGB},
    {"EvenBounds_32x24_I444_ABGR", 32, 24, /*is_i444=*/true, /*uv_divisor=*/1,
     48, 40, 44, webrtc::FOURCC_ABGR, &libyuv::I444ToABGR},
    {"OddBounds_33x25_I420_ARGB", 33, 25, /*is_i444=*/false, /*uv_divisor=*/2,
     48, 40, 44, webrtc::FOURCC_ARGB, &libyuv::I420ToARGB},
    {"OddBounds_33x25_I420_ABGR", 33, 25, /*is_i444=*/false, /*uv_divisor=*/2,
     48, 40, 44, webrtc::FOURCC_ABGR, &libyuv::I420ToABGR},
    {"OddBounds_33x25_I444_ARGB", 33, 25, /*is_i444=*/true, /*uv_divisor=*/1,
     48, 40, 44, webrtc::FOURCC_ARGB, &libyuv::I444ToARGB},
    {"OddBounds_33x25_I444_ABGR", 33, 25, /*is_i444=*/true, /*uv_divisor=*/1,
     48, 40, 44, webrtc::FOURCC_ABGR, &libyuv::I444ToABGR},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ConvertYuvToDesktopFrameBelowThresholdTest,
    testing::ValuesIn(kYuvToRgbBelowThresholdParams),
    [](const testing::TestParamInfo<YuvToRgbTestParam>& info) {
      return info.param.name;
    });

class ConvertYuvToDesktopFrameParallelTest
    : public testing::TestWithParam<YuvToRgbTestParam> {};

TEST_P(ConvertYuvToDesktopFrameParallelTest, ParallelJobMatchesSinglePass) {
  base::test::TaskEnvironment task_environment;
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);
  const YuvToRgbTestParam& param = GetParam();
  const int uv_width = (param.width + param.uv_divisor - 1) / param.uv_divisor;
  const int uv_height =
      (param.height + param.uv_divisor - 1) / param.uv_divisor;

  std::vector<uint8_t> y_plane(
      static_cast<size_t>(param.y_stride) * (param.height - 1) + param.width);
  std::vector<uint8_t> u_plane(
      static_cast<size_t>(param.u_stride) * (uv_height - 1) + uv_width);
  std::vector<uint8_t> v_plane(
      static_cast<size_t>(param.v_stride) * (uv_height - 1) + uv_width);
  for (size_t i = 0; i < y_plane.size(); ++i) {
    y_plane[i] = static_cast<uint8_t>(16 + (i * 5) % 210);
  }
  for (size_t i = 0; i < u_plane.size(); ++i) {
    u_plane[i] = static_cast<uint8_t>(24 + (i * 9) % 200);
  }
  for (size_t i = 0; i < v_plane.size(); ++i) {
    v_plane[i] = static_cast<uint8_t>(24 + (i * 15) % 200);
  }

  const webrtc::DesktopSize size(param.width, param.height);
  webrtc::BasicDesktopFrame expected_rgb(size);
  param.ref_convert_fn(y_plane.data(), param.y_stride, u_plane.data(),
                       param.u_stride, v_plane.data(), param.v_stride,
                       expected_rgb.data(), expected_rgb.stride(), param.width,
                       param.height);

  webrtc::BasicDesktopFrame actual_rgb(size);
  ConvertYuvToDesktopFrame(y_plane, param.y_stride, u_plane, param.u_stride,
                           v_plane, param.v_stride, param.is_i444,
                           param.output_fourcc, actual_rgb);

  ExpectDesktopFramesEqual(actual_rgb, expected_rgb);
}

constexpr YuvToRgbTestParam kYuvToRgbParallelParams[] = {
    {"EvenBounds_1024x542_I420_ARGB", 1024, 542, /*is_i444=*/false,
     /*uv_divisor=*/2, 1088, 576, 608, webrtc::FOURCC_ARGB,
     &libyuv::I420ToARGB},
    {"EvenBounds_1024x542_I420_ABGR", 1024, 542, /*is_i444=*/false,
     /*uv_divisor=*/2, 1088, 576, 608, webrtc::FOURCC_ABGR,
     &libyuv::I420ToABGR},
    {"EvenBounds_1024x542_I444_ARGB", 1024, 542, /*is_i444=*/true,
     /*uv_divisor=*/1, 1088, 1104, 1120, webrtc::FOURCC_ARGB,
     &libyuv::I444ToARGB},
    {"EvenBounds_1024x542_I444_ABGR", 1024, 542, /*is_i444=*/true,
     /*uv_divisor=*/1, 1088, 1104, 1120, webrtc::FOURCC_ABGR,
     &libyuv::I444ToABGR},
    {"OddBounds_1025x543_I420_ARGB", 1025, 543, /*is_i444=*/false,
     /*uv_divisor=*/2, 1088, 576, 608, webrtc::FOURCC_ARGB,
     &libyuv::I420ToARGB},
    {"OddBounds_1025x543_I420_ABGR", 1025, 543, /*is_i444=*/false,
     /*uv_divisor=*/2, 1088, 576, 608, webrtc::FOURCC_ABGR,
     &libyuv::I420ToABGR},
    {"OddBounds_1025x543_I444_ARGB", 1025, 543, /*is_i444=*/true,
     /*uv_divisor=*/1, 1088, 1104, 1120, webrtc::FOURCC_ARGB,
     &libyuv::I444ToARGB},
    {"OddBounds_1025x543_I444_ABGR", 1025, 543, /*is_i444=*/true,
     /*uv_divisor=*/1, 1088, 1104, 1120, webrtc::FOURCC_ABGR,
     &libyuv::I444ToABGR},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ConvertYuvToDesktopFrameParallelTest,
    testing::ValuesIn(kYuvToRgbParallelParams),
    [](const testing::TestParamInfo<YuvToRgbTestParam>& info) {
      return info.param.name;
    });

TEST(CodecUtilsTest,
     ConvertYuvToDesktopFrame_CompletesInlineWhenThreadPoolStarved) {
  ScopedSaturateForegroundThreadPool saturated_pool;
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);

  constexpr int kWidth = 1024;
  constexpr int kHeight = 512;
  std::vector<uint8_t> y_plane(kWidth * kHeight, 128);
  std::vector<uint8_t> u_plane((kWidth / 2) * (kHeight / 2), 90);
  std::vector<uint8_t> v_plane((kWidth / 2) * (kHeight / 2), 180);

  webrtc::BasicDesktopFrame expected_rgb(webrtc::DesktopSize(kWidth, kHeight));
  libyuv::I420ToARGB(y_plane.data(), kWidth, u_plane.data(), kWidth / 2,
                     v_plane.data(), kWidth / 2, expected_rgb.data(),
                     expected_rgb.stride(), kWidth, kHeight);

  webrtc::BasicDesktopFrame actual_rgb(webrtc::DesktopSize(kWidth, kHeight));
  ConvertYuvToDesktopFrame(y_plane, kWidth, u_plane, kWidth / 2, v_plane,
                           kWidth / 2, /*is_i444=*/false, webrtc::FOURCC_ARGB,
                           actual_rgb);

  ExpectDesktopFramesEqual(actual_rgb, expected_rgb);
}

TEST(CodecUtilsTest,
     ConvertYuvToDesktopFrame_NoThreadPoolRunsSinglePassAboveThreshold) {
  ASSERT_FALSE(base::ThreadPoolInstance::Get());
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);
  constexpr int kWidth = 1024;
  constexpr int kHeight = 512;
  std::vector<uint8_t> y_plane(kWidth * kHeight, 128);
  std::vector<uint8_t> u_plane((kWidth / 2) * (kHeight / 2), 90);
  std::vector<uint8_t> v_plane((kWidth / 2) * (kHeight / 2), 180);

  webrtc::BasicDesktopFrame expected_rgb(webrtc::DesktopSize(kWidth, kHeight));
  libyuv::I420ToARGB(y_plane.data(), kWidth, u_plane.data(), kWidth / 2,
                     v_plane.data(), kWidth / 2, expected_rgb.data(),
                     expected_rgb.stride(), kWidth, kHeight);

  webrtc::BasicDesktopFrame actual_rgb(webrtc::DesktopSize(kWidth, kHeight));
  ConvertYuvToDesktopFrame(y_plane, kWidth, u_plane, kWidth / 2, v_plane,
                           kWidth / 2, /*is_i444=*/false, webrtc::FOURCC_ARGB,
                           actual_rgb);

  ExpectDesktopFramesEqual(actual_rgb, expected_rgb);
}

}  // namespace remoting
