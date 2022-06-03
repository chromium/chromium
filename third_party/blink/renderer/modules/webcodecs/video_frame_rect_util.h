// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_RECT_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_RECT_UTIL_H_

#include "media/base/video_types.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class DOMRectInit;
class ExceptionState;

// Convert a DOMRectInit to a gfx::Rect. Validates that all values (including
// the computed |right| and |bottom|) are nonnegative and fit in an int32_t.
// Additionally validates that |rect| fits within |coded_size|. |name|
// is the variable name in error messages, eg. "name.x".
gfx::Rect ToGfxRect(DOMRectInit* rect,
                    const gfx::Size& coded_size,
                    const char* name,
                    ExceptionState&);

// Checks |rect| x, y, width, and height for sample alignment in all planes for
// given |format|. Throws exception to |exception_state| if misalignment is
// detected.
void VerifyRectSampleAlignment(const gfx::Rect& rect,
                               media::VideoPixelFormat format,
                               ExceptionState& exception_state);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_RECT_UTIL_H_
