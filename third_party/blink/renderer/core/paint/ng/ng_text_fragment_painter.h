// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_FRAGMENT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_FRAGMENT_PAINTER_H_

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class DisplayItemClient;
class LayoutObject;
class NGPaintFragment;
class NGInlineCursor;
struct NGTextFragmentPaintInfo;
struct PaintInfo;
struct PhysicalOffset;
struct PhysicalRect;
struct PhysicalSize;

// The helper class for templatize |NGTextFragmentPainter|.
// TODO(yosin): Remove |NGTextPainterCursor| once the transition to
// |NGFragmentItem| is done. http://crbug.com/982194
class NGTextPainterCursor {
  STACK_ALLOCATED();

 public:
  explicit NGTextPainterCursor(const NGPaintFragment& paint_fragment)
      : paint_fragment_(paint_fragment),
        text_fragment_(
            To<NGPhysicalTextFragment>(paint_fragment.PhysicalFragment())) {}

  const NGPaintFragment& PaintFragment() const { return paint_fragment_; }
  const NGPhysicalTextFragment* CurrentItem() const { return &text_fragment_; }
  const NGPaintFragment& RootPaintFragment() const;

 private:
  const NGPaintFragment& paint_fragment_;
  const NGPhysicalTextFragment& text_fragment_;
  mutable const NGPaintFragment* root_paint_fragment_ = nullptr;
};

// Text fragment painter for LayoutNG. Operates on NGPhysicalTextFragments and
// handles clipping, selection, etc. Delegates to NGTextPainter to paint the
// text itself.
// TODO(yosin): We should make |NGTextFragmentPainter| non-template class onnce
// we get rid of |NGPaintFragment|.
template <typename Cursor>
class NGTextFragmentPainter {
  STACK_ALLOCATED();

 public:
  explicit NGTextFragmentPainter(const Cursor&);

  void Paint(const PaintInfo&, const PhysicalOffset& paint_offset);

 private:
  void Paint(const NGTextFragmentPaintInfo& fragment_paint_info,
             const LayoutObject* layout_object,
             const DisplayItemClient& display_item_client,
             const ComputedStyle& style,
             PhysicalRect box_rect,
             const IntRect& visual_rect,
             bool is_ellipsis,
             bool is_symbol_marker,
             const PaintInfo& paint_info,
             const PhysicalOffset& paint_offset);

  static void PaintSymbol(const LayoutObject* layout_object,
                          const ComputedStyle& style,
                          const PhysicalSize box_size,
                          const PaintInfo& paint_info,
                          const PhysicalOffset& paint_offset);

  const Cursor& cursor_;
};

extern template class NGTextFragmentPainter<NGTextPainterCursor>;
extern template class NGTextFragmentPainter<NGInlineCursor>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_FRAGMENT_PAINTER_H_
