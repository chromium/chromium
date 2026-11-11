// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "remoting/codec/utils.h"

#include "base/check.h"
#include "base/check_op.h"
#include "remoting/base/util.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"

namespace remoting {

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
  const webrtc::DesktopRegion aligned_region =
      GetRowAlignedRegion(region, frame.size().width());
  for (webrtc::DesktopRegion::Iterator r(aligned_region); !r.IsAtEnd();
       r.Advance()) {
    ConvertDesktopFrameRectToYuv(frame, r.rect(), is_i444, y_data, y_stride,
                                 u_data, u_stride, v_data, v_stride);
  }
}

}  // namespace remoting
