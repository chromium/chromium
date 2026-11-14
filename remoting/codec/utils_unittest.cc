// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/utils.h"

#include <cstdint>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "remoting/base/cpu_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

namespace {

constexpr uint8_t kSentinel = 0xCD;

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

}  // namespace

TEST(CodecUtilsTest, GetBoundingRect) {
  webrtc::DesktopRegion region;
  EXPECT_TRUE(GetBoundingRect(region).is_empty());

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

TEST(CodecUtilsTest, ConvertDesktopFrameRegionToYuv_RowAlignmentAndClipping) {
  // Exercise 16-, 32-, and 64-pixel SIMD row alignments deterministically on
  // all bots (including non-AVX-512 CQ hosts) across both I420 and I444, using
  // a frame width that is not a multiple of `align` to verify right-edge
  // clipping.
  for (const int align : {16, 32, 64}) {
    ScopedSimdMemoryAlignmentForTesting scoped_align(align);
    const int width = align + 6;
    constexpr int kHeight = 16;
    const int y_stride = align * 2;

    webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(width, kHeight));
    FillPatternFrame(frame);

    // Input rect [align + 2, 4, width, 12] is row-aligned by
    // GetRowAlignedRect() to [align, 4, width, 12] (right edge clipped to
    // `width`).
    const webrtc::DesktopRegion region(
        webrtc::DesktopRect::MakeLTRB(align + 2, 4, width, 12));
    const webrtc::DesktopRect expected_aligned =
        webrtc::DesktopRect::MakeLTRB(align, 4, width, 12);

    for (const bool is_i444 : {false, true}) {
      SCOPED_TRACE(testing::Message()
                   << "align=" << align << " is_i444=" << is_i444);
      const int uv_stride = is_i444 ? y_stride : align;
      const int uv_height = is_i444 ? kHeight : kHeight / 2;
      const int uv_width = is_i444 ? width : width / 2;

      std::vector<uint8_t> ref_y(y_stride * kHeight, 0);
      std::vector<uint8_t> ref_u(uv_stride * uv_height, 0);
      std::vector<uint8_t> ref_v(uv_stride * uv_height, 0);
      if (is_i444) {
        libyuv::ARGBToI444(frame.data(), frame.stride(), ref_y.data(), y_stride,
                           ref_u.data(), uv_stride, ref_v.data(), uv_stride,
                           width, kHeight);
      } else {
        libyuv::ARGBToI420(frame.data(), frame.stride(), ref_y.data(), y_stride,
                           ref_u.data(), uv_stride, ref_v.data(), uv_stride,
                           width, kHeight);
      }

      std::vector<uint8_t> actual_y(y_stride * kHeight, kSentinel);
      std::vector<uint8_t> actual_u(uv_stride * uv_height, kSentinel);
      std::vector<uint8_t> actual_v(uv_stride * uv_height, kSentinel);
      ConvertDesktopFrameRegionToYuv(frame, region, is_i444, actual_y, y_stride,
                                     actual_u, uv_stride, actual_v, uv_stride);

      for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < width; ++x) {
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

      const int uv_scale = is_i444 ? 1 : 2;
      for (int uv_y = 0; uv_y < uv_height; ++uv_y) {
        const bool in_y = uv_y >= expected_aligned.top() / uv_scale &&
                          uv_y < expected_aligned.bottom() / uv_scale;
        for (int uv_x = 0; uv_x < uv_width; ++uv_x) {
          const size_t idx = static_cast<size_t>(uv_y * uv_stride + uv_x);
          if (in_y && uv_x >= expected_aligned.left() / uv_scale &&
              uv_x < expected_aligned.right() / uv_scale) {
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
  }
}

TEST(CodecUtilsTest, GetRowAlignedRegion_CoalescesOverlappingWidenedRects) {
  for (const int align : {16, 32, 64}) {
    SCOPED_TRACE(align);
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
    EXPECT_TRUE(
        rects[0].equals(webrtc::DesktopRect::MakeLTRB(0, 4, align, 14)));

    // Adding `MakeLTRB(align + 2, 4, align + 6, 14)` in the next SIMD block
    // widens to `MakeLTRB(align, 4, align + 8, 14)` when clipped to
    // `align + 8`, abutting `MakeLTRB(0, 4, align, 14)` horizontally and
    // coalescing into `MakeLTRB(0, 4, align + 8, 14)`.
    region.AddRect(webrtc::DesktopRect::MakeLTRB(align + 2, 4, align + 6, 14));
    EXPECT_TRUE(GetRowAlignedRegion(region, align + 8)
                    .Equals(webrtc::DesktopRegion(
                        webrtc::DesktopRect::MakeLTRB(0, 4, align + 8, 14))));
  }
}

TEST(
    CodecUtilsTest,
    ConvertDesktopFrameRegionToYuv_OverlappingAlignedRectsProduceCorrectOutput) {
  for (const int align : {16, 32, 64}) {
    ScopedSimdMemoryAlignmentForTesting scoped_align(align);
    const int width = align * 2;
    constexpr int kHeight = 16;
    const int y_stride = width;

    webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(width, kHeight));
    FillPatternFrame(frame);

    // `MakeLTRB(2, 4, 6, 12)` and `MakeLTRB(10, 6, 14, 14)` are initially
    // disjoint and overlap on `x:[0, align), y:[6, 12)` once
    // `GetRowAlignedRect()` widens both X-ranges to `[0, align)`.
    webrtc::DesktopRegion region;
    region.AddRect(webrtc::DesktopRect::MakeLTRB(2, 4, 6, 12));
    region.AddRect(webrtc::DesktopRect::MakeLTRB(10, 6, 14, 14));

    for (const bool is_i444 : {false, true}) {
      SCOPED_TRACE(testing::Message()
                   << "align=" << align << " is_i444=" << is_i444);
      const int uv_stride = is_i444 ? width : width / 2;
      const int uv_height = is_i444 ? kHeight : kHeight / 2;

      std::vector<uint8_t> actual_y(y_stride * kHeight, kSentinel);
      std::vector<uint8_t> actual_u(uv_stride * uv_height, kSentinel);
      std::vector<uint8_t> actual_v(uv_stride * uv_height, kSentinel);
      ConvertDesktopFrameRegionToYuv(frame, region, is_i444, actual_y, y_stride,
                                     actual_u, uv_stride, actual_v, uv_stride);

      std::vector<uint8_t> expected_y(y_stride * kHeight, kSentinel);
      std::vector<uint8_t> expected_u(uv_stride * uv_height, kSentinel);
      std::vector<uint8_t> expected_v(uv_stride * uv_height, kSentinel);
      ConvertDesktopFrameRectToYuv(
          frame, webrtc::DesktopRect::MakeLTRB(0, 4, align, 14), is_i444,
          expected_y, y_stride, expected_u, uv_stride, expected_v, uv_stride);

      EXPECT_EQ(actual_y, expected_y);
      EXPECT_EQ(actual_u, expected_u);
      EXPECT_EQ(actual_v, expected_v);
    }
  }
}

TEST(CodecUtilsTest,
     ConvertDesktopFrameRegionToYuv_ParallelJobMatchesSinglePass) {
  base::test::TaskEnvironment task_environment;
  // 1024 * 542 = 555,008 pixels (> `kParallelYuvConversionPixelThreshold`).
  // `542` is not divisible by 16, exercising `RoundToTwosMultiple` across
  // uneven horizontal strip heights so I420 chroma rows remain aligned.
  constexpr int kWidth = 1024;
  constexpr int kHeight = 542;
  constexpr int kYStride = 1088;

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(kWidth, kHeight));
  FillPatternFrame(frame);

  // Exercise both a full-frame rect and a multi-rect region mixing a tall rect
  // (split across dynamic strips) and short rects (`height == 4`, kept intact
  // without splitting) that totals >= 512k pixels.
  webrtc::DesktopRegion multi_rect_region;
  multi_rect_region.AddRect(webrtc::DesktopRect::MakeLTRB(0, 0, kWidth, 514));
  multi_rect_region.AddRect(webrtc::DesktopRect::MakeLTRB(0, 520, kWidth, 524));
  multi_rect_region.AddRect(webrtc::DesktopRect::MakeLTRB(0, 528, kWidth, 532));
  multi_rect_region.AddRect(webrtc::DesktopRect::MakeLTRB(0, 536, kWidth, 540));

  const std::vector<webrtc::DesktopRegion> regions = {
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeWH(kWidth, kHeight)),
      multi_rect_region,
  };

  size_t region_idx = 0;
  for (const webrtc::DesktopRegion& region : regions) {
    const webrtc::DesktopRegion aligned_region =
        GetRowAlignedRegion(region, kWidth);
    for (const bool is_i444 : {false, true}) {
      SCOPED_TRACE(testing::Message()
                   << "region_idx=" << region_idx << " is_i444=" << is_i444);
      const int u_stride = is_i444 ? 1104 : 576;
      const int v_stride = is_i444 ? 1120 : 608;
      const int uv_height = is_i444 ? kHeight : kHeight / 2;

      std::vector<uint8_t> expected_y(kYStride * kHeight, kSentinel);
      std::vector<uint8_t> expected_u(u_stride * uv_height, kSentinel);
      std::vector<uint8_t> expected_v(v_stride * uv_height, kSentinel);
      for (webrtc::DesktopRegion::Iterator r(aligned_region); !r.IsAtEnd();
           r.Advance()) {
        ConvertDesktopFrameRectToYuv(frame, r.rect(), is_i444, expected_y,
                                     kYStride, expected_u, u_stride, expected_v,
                                     v_stride);
      }

      std::vector<uint8_t> actual_y(kYStride * kHeight, kSentinel);
      std::vector<uint8_t> actual_u(u_stride * uv_height, kSentinel);
      std::vector<uint8_t> actual_v(v_stride * uv_height, kSentinel);
      ConvertDesktopFrameRegionToYuv(frame, region, is_i444, actual_y, kYStride,
                                     actual_u, u_stride, actual_v, v_stride);

      EXPECT_EQ(actual_y, expected_y);
      EXPECT_EQ(actual_u, expected_u);
      EXPECT_EQ(actual_v, expected_v);
    }
    ++region_idx;
  }
}

TEST(CodecUtilsTest,
     ConvertDesktopFrameRegionToYuv_CompletesInlineWhenThreadPoolStarved) {
  // Declare `workers_occupied` and `release_workers` before `task_environment`
  // so they outlive `~TaskEnvironment()`'s `ThreadPool` worker-thread join.
  base::TestWaitableEvent workers_occupied;
  base::TestWaitableEvent release_workers;
  base::test::TaskEnvironment task_environment;
  base::ScopedClosureRunner release_guard(base::BindOnce(
      &base::TestWaitableEvent::Signal, base::Unretained(&release_workers)));

  // Pin every foreground `ThreadPool` worker on `release_workers` (`Wait()` on
  // `TestWaitableEvent` omits `ScopedBlockingCall`, so `ThreadPool` does not
  // spawn replacement threads). Static `PostTask` + `BarrierClosure` would
  // deadlock here because `release_workers` is only signaled after conversion
  // returns; `CreateJob(...).Join()` drains all strips inline on the calling
  // thread instead.
  base::RepeatingClosure occupied_barrier = base::BarrierClosure(
      base::test::TaskEnvironment::kNumForegroundThreadPoolThreads,
      base::BindOnce(&base::TestWaitableEvent::Signal,
                     base::Unretained(&workers_occupied)));
  for (int i = 0;
       i < base::test::TaskEnvironment::kNumForegroundThreadPoolThreads; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING},
        base::BindOnce(
            [](base::RepeatingClosure on_occupied,
               base::TestWaitableEvent* release) {
              on_occupied.Run();
              release->Wait();
            },
            occupied_barrier, base::Unretained(&release_workers)));
  }
  workers_occupied.Wait();

  constexpr int kWidth = 1024;
  constexpr int kHeight = 512;  // 524,288 pixels >= 512,000.

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(kWidth, kHeight));
  FillPatternFrame(frame);
  const webrtc::DesktopRegion region(
      webrtc::DesktopRect::MakeWH(kWidth, kHeight));

  for (const bool is_i444 : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_i444=" << is_i444);
    const int uv_stride = is_i444 ? kWidth : kWidth / 2;
    const int uv_height = is_i444 ? kHeight : kHeight / 2;

    std::vector<uint8_t> actual_y(kWidth * kHeight, kSentinel);
    std::vector<uint8_t> actual_u(uv_stride * uv_height, kSentinel);
    std::vector<uint8_t> actual_v(uv_stride * uv_height, kSentinel);
    ConvertDesktopFrameRegionToYuv(frame, region, is_i444, actual_y, kWidth,
                                   actual_u, uv_stride, actual_v, uv_stride);

    std::vector<uint8_t> expected_y(kWidth * kHeight, kSentinel);
    std::vector<uint8_t> expected_u(uv_stride * uv_height, kSentinel);
    std::vector<uint8_t> expected_v(uv_stride * uv_height, kSentinel);
    ConvertDesktopFrameRectToYuv(
        frame, webrtc::DesktopRect::MakeWH(kWidth, kHeight), is_i444,
        expected_y, kWidth, expected_u, uv_stride, expected_v, uv_stride);

    EXPECT_EQ(actual_y, expected_y);
    EXPECT_EQ(actual_u, expected_u);
    EXPECT_EQ(actual_v, expected_v);
  }

  release_guard.RunAndReset();
  base::ThreadPoolInstance::Get()->FlushForTesting();
}

TEST(CodecUtilsTest,
     ConvertDesktopFrameRegionToYuv_NoThreadPoolRunsSinglePassAboveThreshold) {
  // Intentionally omit `TaskEnvironment` (`ThreadPoolInstance::Get()` is null)
  // to verify callers without an initialized `ThreadPool` fall back cleanly to
  // single-pass conversion above `kParallelYuvConversionPixelThreshold`.
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

}  // namespace remoting
