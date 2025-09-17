// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/layout_table_caption.h"

#include "third_party/blink/renderer/core/layout/table/layout_table.h"

namespace blink {

LayoutTableCaption::LayoutTableCaption(Element* element)
    : LayoutBlockFlow(element) {}

LayoutTable* LayoutTableCaption::Table() const {
  NOT_DESTROYED();
  if (LayoutObject* parent = Parent()) {
    return To<LayoutTable>(parent);
  }
  return nullptr;
}

void LayoutTableCaption::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const StyleChangeContext& style_change_context) {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    // Modifying the `caption-side` property means that the structure of the
    // table has changed. In this case, we need to repaint the table to ensure
    // the borders are properly updated.
    if (old_style && old_style->CaptionSide() != StyleRef().CaptionSide()) {
      table->SetShouldDoFullPaintInvalidation();
    }
  }
  LayoutBlockFlow::StyleDidChange(diff, old_style, style_change_context);
}

}  // namespace blink
