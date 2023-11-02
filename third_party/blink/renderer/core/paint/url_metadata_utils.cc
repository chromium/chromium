// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/url_metadata_utils.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"

namespace blink {

void AddURLRectsForInlineChildrenRecursively(
    const LayoutObject& layout_object,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  for (LayoutObject* child = layout_object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsLayoutInline() ||
        To<LayoutBoxModelObject>(child)->HasSelfPaintingLayer())
      continue;
    ObjectPainter(*child).AddURLRectIfNeeded(paint_info, paint_offset);
    AddURLRectsForInlineChildrenRecursively(*child, paint_info, paint_offset);
  }
}

}  // namespace blink
