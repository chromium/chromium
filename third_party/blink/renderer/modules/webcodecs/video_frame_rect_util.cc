// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame_init_util.h"

#include <stdint.h>
#include <cmath>
#include <limits>

#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Safely converts a double to a non-negative int, as required for gfx::Rect.
int32_t ToInt31(double value,
                const char* object_name,
                const char* property_name,
                ExceptionState& exception_state) {
  // Reject NaN and +/- Infinity.
  if (!std::isfinite(value)) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid %s. %s must be finite.", object_name, property_name));
    return 0;
  }

  // Truncate before comparison, otherwise INT_MAX + 0.1 would be rejected.
  value = std::trunc(value);

  if (value < 0) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid %s. %s cannot be negative.", object_name, property_name));
    return 0;
  }

  if (value > std::numeric_limits<int32_t>::max()) {
    exception_state.ThrowTypeError(
        String::Format("Invalid %s. %s exceeds implementation limit.",
                       object_name, property_name));
    return 0;
  }

  return static_cast<int32_t>(value);
}

}  // namespace

gfx::Rect ToGfxRect(const DOMRectInit* rect,
                    const char* rect_name,
                    const gfx::Size& coded_size,
                    ExceptionState& exception_state) {
  int32_t x = ToInt31(rect->x(), rect_name, "x", exception_state);
  if (exception_state.HadException())
    return gfx::Rect();

  int32_t y = ToInt31(rect->y(), rect_name, "y", exception_state);
  if (exception_state.HadException())
    return gfx::Rect();

  int32_t width = ToInt31(rect->width(), rect_name, "width", exception_state);
  if (exception_state.HadException())
    return gfx::Rect();

  int32_t height =
      ToInt31(rect->height(), rect_name, "height", exception_state);
  if (exception_state.HadException())
    return gfx::Rect();

  if (width == 0) {
    exception_state.ThrowTypeError(
        String::Format("Invalid %s. width must be nonzero.", rect_name));
    return gfx::Rect();
  }

  if (height == 0) {
    exception_state.ThrowTypeError(
        String::Format("Invalid %s. height must be nonzero.", rect_name));
    return gfx::Rect();
  }

  if (static_cast<int64_t>(x) + width > std::numeric_limits<int32_t>::max()) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid %s. right exceeds implementation limit.", rect_name));
    return gfx::Rect();
  }

  if (static_cast<int64_t>(y) + height > std::numeric_limits<int32_t>::max()) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid %s. bottom exceeds implementation limit.", rect_name));
    return gfx::Rect();
  }

  gfx::Rect gfx_rect = gfx::Rect(x, y, width, height);
  if (gfx_rect.right() > coded_size.width()) {
    exception_state.ThrowTypeError(
        String::Format("Invalid %s. right %i exceeds codedWidth %i.", rect_name,
                       gfx_rect.right(), coded_size.width()));
    return gfx::Rect();
  }

  if (gfx_rect.bottom() > coded_size.height()) {
    exception_state.ThrowTypeError(
        String::Format("Invalid %s. bottom %u exceeds codedHeight %u.",
                       rect_name, gfx_rect.bottom(), coded_size.height()));
    return gfx::Rect();
  }

  return gfx_rect;
}

bool ValidateOffsetAlignment(media::VideoPixelFormat format,
                             const gfx::Rect& rect,
                             const char* rect_name,
                             ExceptionState& exception_state) {
  const wtf_size_t num_planes =
      static_cast<wtf_size_t>(media::VideoFrame::NumPlanes(format));
  for (wtf_size_t i = 0; i < num_planes; i++) {
    const gfx::Size sample_size = media::VideoFrame::SampleSize(format, i);
    if (rect.x() % sample_size.width() != 0) {
      exception_state.ThrowTypeError(String::Format(
          "Invalid %s. x is not sample-aligned in plane %u.", rect_name, i));
      return false;
    }
    if (rect.y() % sample_size.height() != 0) {
      exception_state.ThrowTypeError(String::Format(
          "Invalid %s. y is not sample-aligned in plane %u.", rect_name, i));
      return false;
    }
  }
  return true;
}

int PlaneSize(int frame_size, int sample_size) {
  return (frame_size + sample_size - 1) / sample_size;
}

gfx::Rect PlaneRect(gfx::Rect frame_rect, gfx::Size sample_size) {
  DCHECK_EQ(frame_rect.x() % sample_size.width(), 0);
  DCHECK_EQ(frame_rect.y() % sample_size.height(), 0);
  return gfx::Rect(frame_rect.x() / sample_size.width(),
                   frame_rect.y() / sample_size.height(),
                   PlaneSize(frame_rect.width(), sample_size.width()),
                   PlaneSize(frame_rect.height(), sample_size.height()));
}

}  // namespace blink
