// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_RECT_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_RECT_UTIL_H_

#include "media/base/video_types.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class DOMRectInit;
class ExceptionState;

// Converts DOMRectInit to gfx::Rect. Validates that all values (including the
// computed |right| and |bottom|) are nonnegative and fit into |coded_size|, and
// that the result is nonempty. |rect_name| is the variable name in error
// messages, eg. "name.x".
gfx::Rect ToGfxRect(const DOMRectInit* rect,
                    const char* rect_name,
                    const gfx::Size& coded_size,
                    ExceptionState& exception_state);

// Checks |rect| x, y, width, and height for sample alignment in all planes.
bool ValidateCropAlignment(media::VideoPixelFormat format,
                           const char* format_str,
                           const gfx::Rect& rect,
                           const char* rect_name,
                           ExceptionState& exception_state);

// Checks |rect| x and y for sample alignment in all planes.
bool ValidateOffsetAlignment(media::VideoPixelFormat format,
                             const gfx::Rect& rect,
                             const char* rect_name,
                             ExceptionState& exception_state);

// Aligns a crop by expanding the size if necessary.
gfx::Rect AlignCrop(media::VideoPixelFormat format, const gfx::Rect& rect);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_RECT_UTIL_H_
