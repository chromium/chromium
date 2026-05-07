// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CARET_COLOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CARET_COLOR_H_

#include "third_party/blink/renderer/core/css/style_auto_color.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Stores the two values of the caret-color CSS property:
//   caret-color: <fill-color> [<text-color>]?
// The second value is optional. It specifies the color of text overlapping a
// block caret.
class StyleCaretColor {
  DISALLOW_NEW();

 public:
  StyleCaretColor()
      : fill_color_(StyleAutoColor::AutoColor()),
        text_color_(StyleAutoColor::AutoColor()) {}

  StyleCaretColor(StyleAutoColor fill_color, StyleAutoColor text_color)
      : fill_color_(std::move(fill_color)),
        text_color_(std::move(text_color)) {}

  // For being back compatible with single value form.
  bool IsAutoColor() const { return fill_color_.IsAutoColor(); }
  bool IsCurrentColor() const { return fill_color_.IsCurrentColor(); }
  const StyleColor& ToStyleColor() const { return fill_color_.ToStyleColor(); }
  bool DependsOnCurrentColor() const {
    return fill_color_.DependsOnCurrentColor() ||
           text_color_.DependsOnCurrentColor();
  }

  const StyleAutoColor& FillColor() const { return fill_color_; }
  const StyleAutoColor& TextColor() const { return text_color_; }

  void Trace(Visitor* visitor) const {
    TraceIfNeeded<StyleAutoColor>::Trace(visitor, fill_color_);
    TraceIfNeeded<StyleAutoColor>::Trace(visitor, text_color_);
  }

 private:
  StyleAutoColor fill_color_;
  StyleAutoColor text_color_;
};

inline bool operator==(const StyleCaretColor& a, const StyleCaretColor& b) {
  return a.FillColor() == b.FillColor() && a.TextColor() == b.TextColor();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_CARET_COLOR_H_
