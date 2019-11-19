// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_LOCAL_CARET_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_LOCAL_CARET_RECT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"

namespace blink {

class LayoutObject;

// A transient struct representing a caret rect local to |layout_object|.
struct LocalCaretRect {
  STACK_ALLOCATED();

 public:
  const LayoutObject* layout_object = nullptr;
  PhysicalRect rect;

  LocalCaretRect() = default;
  LocalCaretRect(const LayoutObject* layout_object, const PhysicalRect& rect)
      : layout_object(layout_object), rect(rect) {}

  bool IsEmpty() const { return !layout_object || rect.IsEmpty(); }
};

// Rect is local to the returned layoutObject
// TODO(xiaochengh): Get rid of the default parameter.
CORE_EXPORT LocalCaretRect LocalCaretRectOfPosition(
    const PositionWithAffinity&,
    LayoutUnit* /* extra_width_to_end_of_line */ = nullptr);
CORE_EXPORT LocalCaretRect
LocalCaretRectOfPosition(const PositionInFlatTreeWithAffinity&);

LocalCaretRect LocalSelectionRectOfPosition(const PositionWithAffinity&);

// Bounds of (possibly transformed) caret in absolute coords
CORE_EXPORT IntRect
AbsoluteCaretBoundsOf(const PositionWithAffinity&,
                      LayoutUnit* extra_width_to_end_of_line = nullptr);
CORE_EXPORT IntRect
AbsoluteCaretBoundsOf(const PositionInFlatTreeWithAffinity&);

CORE_EXPORT IntRect AbsoluteSelectionBoundsOf(const VisiblePosition&);
CORE_EXPORT IntRect AbsoluteSelectionBoundsOf(const VisiblePositionInFlatTree&);

// Exposed to tests only. Implemented in local_caret_rect_test.cc.
bool operator==(const LocalCaretRect&, const LocalCaretRect&);
std::ostream& operator<<(std::ostream&, const LocalCaretRect&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_LOCAL_CARET_RECT_H_
