// Copyright 2021 The Chromium Authors. All rights reserved.
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

// Safely converts a double to a non-negative int, which matches the
// requirements of gfx::Rect.
int32_t ToInt31(double value,
                const char* context,
                const char* name,
                ExceptionState& exception_state) {
  // Reject NaN and +/- Infinity. Positive subnormals truncate to zero so they
  // are fine.
  if (!std::isfinite(value)) {
    exception_state.ThrowTypeError(
        String::Format("%s.%s must be finite.", context, name));
    return 0;
  }

  // If we didn't truncate before comparison, INT_MAX + 0.1 would be rejected.
  value = std::trunc(value);

  if (value < 0) {
    exception_state.ThrowTypeError(
        String::Format("%s.%s cannot be negative.", context, name));
    return 0;
  }

  if (value > std::numeric_limits<int32_t>::max()) {
    exception_state.ThrowTypeError(
        String::Format("%s.%s exceeds implementation limit.", context, name));
    return 0;
  }

  return static_cast<int32_t>(value);
}

}  // namespace

gfx::Rect ToGfxRect(DOMRectInit* rect,
                    const gfx::Size& coded_size,
                    const char* name,
                    ExceptionState& exception_state) {
  int32_t x = ToInt31(rect->x(), name, "x", exception_state);
  if (exception_state.HadException())
    return gfx::Rect();

  int32_t y = ToInt31(rect->y(), name, "y", exception_state);
  if (exception_state.HadException())
    return gfx::Rect();

  int32_t width = ToInt31(rect->width(), name, "width", exception_state);
  if (exception_state.HadException())
    return gfx::Rect();

  int32_t height = ToInt31(rect->height(), name, "height", exception_state);
  if (exception_state.HadException())
    return gfx::Rect();

  // |right = x + width| must fit in int. Not actually required by gfx::Rect but
  // probably not handled by code that uses them.
  if (static_cast<int64_t>(x) + width > std::numeric_limits<int32_t>::max()) {
    exception_state.ThrowTypeError(
        String::Format("%s.right exceeds implementation limit.", name));
    return gfx::Rect();
  }

  // Same for |bottom = y + height|.
  if (static_cast<int64_t>(y) + height > std::numeric_limits<int32_t>::max()) {
    exception_state.ThrowTypeError(
        String::Format("%s.bottom exceeds implementation limit.", name));
    return gfx::Rect();
  }

  gfx::Rect gfxRect = gfx::Rect(x, y, width, height);
  if (gfxRect.right() > coded_size.width()) {
    exception_state.ThrowTypeError(
        String::Format("%s.right %i exceeds codedWidth %i.", name,
                       gfxRect.right(), coded_size.width()));
    return gfx::Rect();
  }

  if (gfxRect.bottom() > coded_size.height()) {
    exception_state.ThrowTypeError(
        String::Format("%s.bottom %u exceeds codedHeight %u.", name,
                       gfxRect.bottom(), coded_size.height()));
    return gfx::Rect();
  }

  return gfxRect;
}

void VerifyRectSampleAlignment(const gfx::Rect& rect,
                               media::VideoPixelFormat format,
                               ExceptionState& exception_state) {
  for (size_t i = 0; i < media::VideoFrame::NumPlanes(format); i++) {
    gfx::Size sample_size = media::VideoFrame::SampleSize(format, i);
    if (rect.x() % sample_size.width() != 0) {
      exception_state.ThrowTypeError(String::Format(
          "rect.x %d is not sample-aligned in plane %zu.", rect.x(), i));
      return;
    } else if (rect.width() % sample_size.width() != 0) {
      exception_state.ThrowTypeError(
          String::Format("rect.width %d is not sample-aligned in plane %zu.",
                         rect.width(), i));
      return;
    } else if (rect.y() % sample_size.height() != 0) {
      exception_state.ThrowTypeError(String::Format(
          "rect.y %d is not sample-aligned in plane %zu.", rect.y(), i));
      return;
    } else if (rect.height() % sample_size.height() != 0) {
      exception_state.ThrowTypeError(
          String::Format("rect.height %d is not sample-aligned in plane %zu.",
                         rect.height(), i));
      return;
    }
  }
}

}  // namespace blink
