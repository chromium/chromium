// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_DIRTY_LINES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_DIRTY_LINES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {

// This class computes dirty line boxes.
class CORE_EXPORT NGDirtyLines {
  STACK_ALLOCATED();

 public:
  explicit NGDirtyLines(const NGPaintFragment* block_fragment)
      : block_fragment_(block_fragment) {
    DCHECK(block_fragment_);
  }

  // Call |Handle*| functions for each object by traversing the LayoutObject
  // tree in pre-order DFS.
  //
  // They return |true| when a dirty line was found. Because only the first
  // dirty line is relevant, no further calls are necessary.
  bool HandleText(LayoutText* layout_text) {
    if (layout_text->SelfNeedsLayout()) {
      MarkLastFragment();
      return true;
    }
    UpdateLastFragment(layout_text->FirstInlineFragment());
    return false;
  }

  bool HandleInlineBox(LayoutInline* layout_inline) {
    if (layout_inline->SelfNeedsLayout()) {
      MarkLastFragment();
      return true;
    }
    // Do not keep fragments of LayoutInline unless it's a leaf, because
    // the last fragment of LayoutInline is not the previous fragment of its
    // descendants.
    if (UNLIKELY(!layout_inline->FirstChild()))
      UpdateLastFragment(layout_inline->FirstInlineFragment());
    return false;
  }

  bool HandleAtomicInline(LayoutBox* layout_box) {
    if (layout_box->NeedsLayout()) {
      MarkLastFragment();
      return true;
    }
    UpdateLastFragment(layout_box->FirstInlineFragment());
    return false;
  }

  bool HandleFloatingOrOutOfFlowPositioned(LayoutObject* layout_object) {
    DCHECK(layout_object->IsFloatingOrOutOfFlowPositioned());
    if (layout_object->NeedsLayout()) {
      MarkLastFragment();
      return true;
    }
    // Don't update last fragment. Floats and OOF are opaque.
    return false;
  }

  // Mark the line box at the specified text offset dirty.
  void MarkAtTextOffset(unsigned offset);

 private:
  void UpdateLastFragment(NGPaintFragment* fragment) {
    if (fragment)
      last_fragment_ = fragment;
  }

  // Mark the line box that contains |last_fragment_| dirty. If |last_fragment_|
  // is |nullptr|, the first line box is marked as dirty.
  void MarkLastFragment();

  const NGPaintFragment* block_fragment_;
  NGPaintFragment* last_fragment_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_DIRTY_LINES_H_
