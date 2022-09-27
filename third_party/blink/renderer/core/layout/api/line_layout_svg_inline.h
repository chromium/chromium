// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_SVG_INLINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_SVG_INLINE_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"

namespace blink {

class LineLayoutSVGInline : public LineLayoutInline {
 public:
  explicit LineLayoutSVGInline(LayoutSVGInline* layout_svg_inline)
      : LineLayoutInline(layout_svg_inline) {}

  explicit LineLayoutSVGInline(const LineLayoutItem& item)
      : LineLayoutInline(item) {
    SECURITY_DCHECK(!item || item.IsSVGInline());
  }

  explicit LineLayoutSVGInline(std::nullptr_t) : LineLayoutInline(nullptr) {}

  LineLayoutSVGInline() = default;

 private:
  LayoutSVGInline* ToSVGInline() {
    return To<LayoutSVGInline>(GetLayoutObject());
  }

  const LayoutSVGInline* ToSVGInline() const {
    return To<LayoutSVGInline>(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_SVG_INLINE_H_
