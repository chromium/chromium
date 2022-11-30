// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_SVG_TEXT_PATH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_SVG_TEXT_PATH_H_

#include <memory>
#include "third_party/blink/renderer/core/layout/api/line_layout_svg_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text_path.h"

namespace blink {

class LineLayoutSVGTextPath : public LineLayoutSVGInline {
 public:
  explicit LineLayoutSVGTextPath(LayoutSVGTextPath* layout_svg_text_path)
      : LineLayoutSVGInline(layout_svg_text_path) {}

  explicit LineLayoutSVGTextPath(const LineLayoutItem& item)
      : LineLayoutSVGInline(item) {
    SECURITY_DCHECK(!item || item.IsSVGTextPath());
  }

  explicit LineLayoutSVGTextPath(std::nullptr_t)
      : LineLayoutSVGInline(nullptr) {}

  LineLayoutSVGTextPath() = default;

  std::unique_ptr<PathPositionMapper> LayoutPath() const {
    return ToSVGTextPath()->LayoutPath();
  }

 private:
  LayoutSVGTextPath* ToSVGTextPath() {
    return To<LayoutSVGTextPath>(GetLayoutObject());
  }

  const LayoutSVGTextPath* ToSVGTextPath() const {
    return To<LayoutSVGTextPath>(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_SVG_TEXT_PATH_H_
