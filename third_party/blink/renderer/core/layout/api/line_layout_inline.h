// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_INLINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_INLINE_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_box_model.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class LayoutInline;

class LineLayoutInline : public LineLayoutBoxModel {
 public:
  explicit LineLayoutInline(LayoutInline* layout_inline)
      : LineLayoutBoxModel(layout_inline) {}

  explicit LineLayoutInline(const LineLayoutItem& item)
      : LineLayoutBoxModel(item) {
    SECURITY_DCHECK(!item || item.IsLayoutInline());
  }

  explicit LineLayoutInline(std::nullptr_t) : LineLayoutBoxModel(nullptr) {}

  LineLayoutInline() = default;

  LineLayoutItem FirstChild() const {
    return LineLayoutItem(ToInline()->FirstChild());
  }

  LineLayoutItem LastChild() const {
    return LineLayoutItem(ToInline()->LastChild());
  }

  LayoutUnit MarginStart() const { return ToInline()->MarginStart(); }

  LayoutUnit MarginEnd() const { return ToInline()->MarginEnd(); }

  LayoutUnit BorderStart() const { return ToInline()->BorderStart(); }

  LayoutUnit BorderEnd() const { return ToInline()->BorderEnd(); }

  LayoutUnit PaddingStart() const { return ToInline()->PaddingStart(); }

  LayoutUnit PaddingEnd() const { return ToInline()->PaddingEnd(); }

  bool HasInlineDirectionBordersPaddingOrMargin() const {
    return ToInline()->HasInlineDirectionBordersPaddingOrMargin();
  }

  bool AlwaysCreateLineBoxes() const {
    return ToInline()->AlwaysCreateLineBoxes();
  }

  InlineBox* FirstLineBoxIncludingCulling() const {
    return ToInline()->FirstLineBoxIncludingCulling();
  }

  InlineBox* LastLineBoxIncludingCulling() const {
    return ToInline()->LastLineBoxIncludingCulling();
  }

  LineBoxList* LineBoxes() { return ToInline()->MutableLineBoxes(); }

  bool HitTestCulledInline(HitTestResult& result,
                           const HitTestLocation& location_in_container,
                           const LayoutPoint& accumulated_offset) {
    return ToInline()->HitTestCulledInline(result, location_in_container,
                                           accumulated_offset);
  }

  LayoutBoxModelObject* Continuation() const {
    return ToInline()->Continuation();
  }

  InlineBox* CreateAndAppendInlineFlowBox() {
    return ToInline()->CreateAndAppendInlineFlowBox();
  }

  InlineFlowBox* LastLineBox() { return ToInline()->LastLineBox(); }

 protected:
  LayoutInline* ToInline() { return ToLayoutInline(GetLayoutObject()); }

  const LayoutInline* ToInline() const {
    return ToLayoutInline(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_INLINE_H_
