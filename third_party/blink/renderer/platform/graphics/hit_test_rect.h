// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_HIT_TEST_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_HIT_TEST_RECT_H_

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// TODO(pdr): Rename this TouchActionRect.
struct PLATFORM_EXPORT HitTestRect {
  LayoutRect rect;
  TouchAction allowed_touch_action;

  HitTestRect(const LayoutRect& layout_rect)
      : HitTestRect(layout_rect, TouchAction::kTouchActionNone) {}
  HitTestRect(const LayoutRect& layout_rect, TouchAction action)
      : rect(layout_rect), allowed_touch_action(action) {}

  static LayoutRect GetBounds(const Vector<HitTestRect>&);

  bool operator==(const HitTestRect& rhs) const {
    return rect == rhs.rect && allowed_touch_action == rhs.allowed_touch_action;
  }

  bool operator!=(const HitTestRect& rhs) const { return !(*this == rhs); }

  String ToString() const;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const HitTestRect&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_HIT_TEST_RECT_H_
