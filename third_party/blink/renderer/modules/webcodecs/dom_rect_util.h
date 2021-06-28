// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_DOM_RECT_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_DOM_RECT_UTIL_H_

#include "ui/gfx/geometry/rect.h"

namespace blink {

class DOMRectInit;
class ExceptionState;

// Convert a DOMRectInit to a gfx::Rect. Validates that all values (including
// the computed |right| and |bottom|) are nonnegative and fit in an int32_t.
// |name| is the variable name in error messages, eg. "name.x".
gfx::Rect ToGfxRect(DOMRectInit*, const char* name, ExceptionState&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_DOM_RECT_UTIL_H_
