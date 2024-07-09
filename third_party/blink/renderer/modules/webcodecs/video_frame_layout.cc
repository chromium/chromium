// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webcodecs/video_frame_layout.h"

#include <stdint.h>
#include <vector>

#include "base/numerics/checked_math.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_plane_layout.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_rect_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

VideoFrameLayout::VideoFrameLayout() : format_(media::PIXEL_FORMAT_UNKNOWN) {}

VideoFrameLayout::VideoFrameLayout(media::VideoPixelFormat format,
                                   const gfx::Size& coded_size,
                                   ExceptionState& exception_state)
    : format_(format), coded_size_(coded_size) {
  DCHECK_LE(coded_size_.width(), media::limits::kMaxDimension);
  DCHECK_LE(coded_size_.height(), media::limits::kMaxDimension);

  const wtf_size_t num_planes =
      static_cast<wtf_size_t>(media::VideoFrame::NumPlanes(format_));
  uint32_t offset = 0;
  for (wtf_size_t i = 0; i < num_planes; i++) {
    const gfx::Size sample_size = media::VideoFrame::SampleSize(format_, i);
    const uint32_t sample_bytes =
        media::VideoFrame::BytesPerElement(format_, i);
    const uint32_t columns =
        PlaneSize(coded_size_.width(), sample_size.width());
    const uint32_t rows = PlaneSize(coded_size_.height(), sample_size.height());
    const uint32_t stride = columns * sample_bytes;
    planes_.push_back(Plane{offset, stride});
    offset += stride * rows;
  }
}

VideoFrameLayout::VideoFrameLayout(
    media::VideoPixelFormat format,
    const gfx::Size& coded_size,
    const HeapVector<Member<PlaneLayout>>& layout,
    ExceptionState& exception_state)
    : format_(format), coded_size_(coded_size) {
  DCHECK_LE(coded_size_.width(), media::limits::kMaxDimension);
  DCHECK_LE(coded_size_.height(), media::limits::kMaxDimension);

  const wtf_size_t num_planes =
      static_cast<wtf_size_t>(media::VideoFrame::NumPlanes(format_));
  if (layout.size() != num_planes) {
    exception_state.ThrowTypeError(
        String::Format("Invalid layout. Expected %u planes, found %u.",
                       num_planes, layout.size()));
    return;
  }

  uint32_t end[media::VideoFrame::kMaxPlanes] = {0};
  for (wtf_size_t i = 0; i < num_planes; i++) {
    const gfx::Size sample_size = media::VideoFrame::SampleSize(format_, i);
    const uint32_t sample_bytes =
        media::VideoFrame::BytesPerElement(format_, i);
    const uint32_t columns =
        PlaneSize(coded_size_.width(), sample_size.width());
    const uint32_t rows = PlaneSize(coded_size_.height(), sample_size.height());
    const uint32_t offset = layout[i]->offset();
    const uint32_t stride = layout[i]->stride();

    // Each row must fit inside the stride.
    const uint32_t min_stride = columns * sample_bytes;
    if (stride < min_stride) {
      exception_state.ThrowTypeError(
          String::Format("Invalid layout. Expected plane %u to have stride at "
                         "least %u, found %u.",
                         i, min_stride, stride));
      return;
    }

    const auto checked_bytes = base::CheckedNumeric<uint32_t>(stride) * rows;
    const auto checked_end = checked_bytes + offset;

    // Each plane size must not overflow int for compatibility with libyuv.
    // There are probably tighter bounds we could enforce.
    if (!checked_bytes.Cast<int>().IsValid()) {
      exception_state.ThrowTypeError(String::Format(
          "Invalid layout. Plane %u with stride %u and height %u exceeds "
          "implementation limit.",
          i, stride, rows));
      return;
    }

    // The size of the buffer must not overflow uint32_t for compatibility with
    // ArrayBuffer.
    if (!checked_end.IsValid()) {
      exception_state.ThrowTypeError(
          String::Format("Invalid layout. Plane %u with offset %u and stride "
                         "%u exceeds implementation limit.",
                         i, offset, stride));
      return;
    }

    // Planes must not overlap.
    end[i] = checked_end.ValueOrDie();
    for (wtf_size_t j = 0; j < i; j++) {
      if (offset < end[j] && planes_[j].offset < end[i]) {
        exception_state.ThrowTypeError(String::Format(
            "Invalid layout. Plane %u overlaps with plane %u.", i, j));
        return;
      }
    }

    planes_.push_back(Plane{offset, stride});
  }
}

media::VideoFrameLayout VideoFrameLayout::ToMediaLayout() {
  std::vector<media::ColorPlaneLayout> planes;
  planes.reserve(planes_.size());
  for (wtf_size_t i = 0; i < planes_.size(); i++) {
    auto& plane = planes_[i];
    const gfx::Size sample_size = media::VideoFrame::SampleSize(format_, i);
    const uint32_t height = coded_size_.height() / sample_size.height();
    const size_t plane_size = plane.stride * height;
    planes.emplace_back(plane.stride, plane.offset, plane_size);
  }
  return media::VideoFrameLayout::CreateWithPlanes(format_, coded_size_,
                                                   std::move(planes))
      .value();
}

uint32_t VideoFrameLayout::Size() const {
  uint32_t size = 0;
  for (wtf_size_t i = 0; i < planes_.size(); i++) {
    const gfx::Size sample_size = media::VideoFrame::SampleSize(format_, i);
    const uint32_t rows = PlaneSize(coded_size_.height(), sample_size.height());
    const uint32_t end = planes_[i].offset + planes_[i].stride * rows;
    size = std::max(size, end);
  }
  return size;
}

media::VideoPixelFormat VideoFrameLayout::Format() const {
  return format_;
}

wtf_size_t VideoFrameLayout::NumPlanes() const {
  return planes_.size();
}

uint32_t VideoFrameLayout::Offset(wtf_size_t i) const {
  return planes_[i].offset;
}

uint32_t VideoFrameLayout::Stride(wtf_size_t i) const {
  return planes_[i].stride;
}

}  // namespace blink
