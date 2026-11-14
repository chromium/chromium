// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "remoting/codec/utils.h"

#include <algorithm>
#include <atomic>
#include <initializer_list>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/system/sys_info.h"
#include "base/task/post_job.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "remoting/base/util.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"

namespace remoting {

namespace {

using ConvertRectFn = base::FunctionRef<void(const webrtc::DesktopRect&)>;

int64_t GetRegionPixelCount(const webrtc::DesktopRegion& region) {
  int64_t pixels = 0;
  for (webrtc::DesktopRegion::Iterator r(region); !r.IsAtEnd(); r.Advance()) {
    pixels += int64_t{r.rect().width()} * r.rect().height();
  }
  return pixels;
}

size_t GetMaxParallelWorkers(int64_t yuv_pixels) {
  if (yuv_pixels < kParallelYuvConversionPixelThreshold ||
      !base::ThreadPoolInstance::Get()) {
    return 1;
  }
  constexpr int64_t kMaxWorkers = 8;
  constexpr int64_t kPixelsPerWorker = 128000;
  static_assert(kParallelYuvConversionPixelThreshold >= kPixelsPerWorker);
  const int64_t cpu_cap =
      std::max<int64_t>(1, (base::SysInfo::NumberOfProcessors() + 1) / 2);
  return static_cast<size_t>(
      std::min({cpu_cap, yuv_pixels / kPixelsPerWorker, kMaxWorkers}));
}

// Splits `region` into ~`target_strips` horizontal strips with even Y-offsets
// so parallel workers write to disjoint 2x2 subsampled I420 chroma rows.
std::vector<webrtc::DesktopRect> SplitRegionIntoStrips(
    const webrtc::DesktopRegion& region,
    int64_t total_pixels,
    size_t target_strips) {
  std::vector<webrtc::DesktopRect> strips;
  strips.reserve(target_strips + 4);
  for (webrtc::DesktopRegion::Iterator r(region); !r.IsAtEnd(); r.Advance()) {
    const webrtc::DesktopRect& rect = r.rect();
    const int64_t rect_pixels = int64_t{rect.width()} * rect.height();
    const int num_strips = std::clamp<int>(
        static_cast<int>((rect_pixels * static_cast<int64_t>(target_strips) +
                          total_pixels - 1) /
                         total_pixels),
        1, std::max(1, rect.height() / 4));
    int y0 = rect.top();
    for (int s = 1; s <= num_strips; ++s) {
      const int y1 =
          (s == num_strips)
              ? rect.bottom()
              : rect.top() + RoundToTwosMultiple(static_cast<int>(
                                 (int64_t{rect.height()} * s) / num_strips));
      if (y1 > y0) {
        strips.push_back(
            webrtc::DesktopRect::MakeLTRB(rect.left(), y0, rect.right(), y1));
        y0 = y1;
      }
    }
  }
  return strips;
}

void RunStripsInParallel(base::span<const webrtc::DesktopRect> strips,
                         size_t max_workers,
                         ConvertRectFn convert_rect) {
  std::atomic<size_t> next_strip{0};
  const size_t max_concurrency = std::min(max_workers, strips.size());
  base::CreateJob(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindRepeating(
          [](base::span<const webrtc::DesktopRect> strip_list,
             const ConvertRectFn* convert_fn, std::atomic<size_t>* next_idx,
             base::JobDelegate* delegate) {
            do {
              const size_t i =
                  next_idx->fetch_add(1, std::memory_order_relaxed);
              if (i >= strip_list.size()) {
                return;
              }
              (*convert_fn)(strip_list[i]);
            } while (!delegate->ShouldYield());
          },
          strips, base::Unretained(&convert_rect),
          base::Unretained(&next_strip)),
      base::BindRepeating(
          [](size_t limit, size_t total, const std::atomic<size_t>* next_idx,
             size_t worker_count) {
            const size_t claimed =
                std::min(total, next_idx->load(std::memory_order_relaxed));
            return std::min(limit, worker_count + (total - claimed));
          },
          max_concurrency, strips.size(), base::Unretained(&next_strip)))
      .Join();
}

}  // namespace

webrtc::DesktopRect GetBoundingRect(const webrtc::DesktopRegion& region) {
  webrtc::DesktopRect bound;
  for (webrtc::DesktopRegion::Iterator r(region); !r.IsAtEnd(); r.Advance()) {
    bound.UnionWith(r.rect());
  }
  return bound;
}

void ConvertDesktopFrameRectToYuv(const webrtc::DesktopFrame& frame,
                                  const webrtc::DesktopRect& rect,
                                  bool is_i444,
                                  base::span<uint8_t> y_data,
                                  int y_stride,
                                  base::span<uint8_t> u_data,
                                  int u_stride,
                                  base::span<uint8_t> v_data,
                                  int v_stride) {
  if (rect.is_empty()) {
    return;
  }
  CHECK_EQ(frame.pixel_format(), webrtc::FOURCC_ARGB);
  DCHECK(webrtc::DesktopRect::MakeSize(frame.size()).ContainsRect(rect));
  const uint8_t* const rgb_data = frame.GetFrameDataAtPos(rect.top_left());
  const int rgb_stride = frame.stride();
  const int y_offset = y_stride * rect.top() + rect.left();
  if (is_i444) {
    const int u_offset = u_stride * rect.top() + rect.left();
    const int v_offset = v_stride * rect.top() + rect.left();
    libyuv::ARGBToI444(
        rgb_data, rgb_stride,
        y_data.subspan(static_cast<size_t>(y_offset)).data(), y_stride,
        u_data.subspan(static_cast<size_t>(u_offset)).data(), u_stride,
        v_data.subspan(static_cast<size_t>(v_offset)).data(), v_stride,
        rect.width(), rect.height());
  } else {
    DCHECK_EQ(rect.left() % 2, 0);
    DCHECK_EQ(rect.top() % 2, 0);
    const int u_offset = u_stride * (rect.top() / 2) + rect.left() / 2;
    const int v_offset = v_stride * (rect.top() / 2) + rect.left() / 2;
    libyuv::ARGBToI420(
        rgb_data, rgb_stride,
        y_data.subspan(static_cast<size_t>(y_offset)).data(), y_stride,
        u_data.subspan(static_cast<size_t>(u_offset)).data(), u_stride,
        v_data.subspan(static_cast<size_t>(v_offset)).data(), v_stride,
        rect.width(), rect.height());
  }
}

webrtc::DesktopRegion GetRowAlignedRegion(const webrtc::DesktopRegion& region,
                                          int frame_width) {
  // `region` contains non-overlapping rects, but widening the `left` and
  // `right` X-coordinates of each rect via `GetRowAlignedRect()` can cause
  // nearby rects with different `[top, bottom)` Y-ranges to partially overlap
  // (forming L- or T-shapes). Inserting them into a new `DesktopRegion` splits
  // and merges any overlaps into disjoint rects whose `left` and `right`
  // X-coordinates remain SIMD-aligned.
  webrtc::DesktopRegion aligned_region;
  for (webrtc::DesktopRegion::Iterator r(region); !r.IsAtEnd(); r.Advance()) {
    aligned_region.AddRect(GetRowAlignedRect(r.rect(), frame_width));
  }
  return aligned_region;
}

void ConvertDesktopFrameRegionToYuv(const webrtc::DesktopFrame& frame,
                                    const webrtc::DesktopRegion& region,
                                    bool is_i444,
                                    base::span<uint8_t> y_data,
                                    int y_stride,
                                    base::span<uint8_t> u_data,
                                    int u_stride,
                                    base::span<uint8_t> v_data,
                                    int v_stride) {
  auto convert_rect = [&](const webrtc::DesktopRect& rect) {
    ConvertDesktopFrameRectToYuv(frame, rect, is_i444, y_data, y_stride, u_data,
                                 u_stride, v_data, v_stride);
  };

  const webrtc::DesktopRegion aligned_region =
      GetRowAlignedRegion(region, frame.size().width());
  const int64_t yuv_pixels = GetRegionPixelCount(aligned_region);
  const size_t max_workers = GetMaxParallelWorkers(yuv_pixels);
  if (max_workers > 1) {
    const std::vector<webrtc::DesktopRect> strips =
        SplitRegionIntoStrips(aligned_region, yuv_pixels, max_workers * 2);
    if (strips.size() > 1) {
      RunStripsInParallel(strips, max_workers, convert_rect);
      return;
    }
  }

  for (webrtc::DesktopRegion::Iterator r(aligned_region); !r.IsAtEnd();
       r.Advance()) {
    convert_rect(r.rect());
  }
}

}  // namespace remoting
