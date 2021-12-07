// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_FRAGMENT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_FRAGMENT_PAINTER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class DisplayItemClient;
class LayoutObject;
class NGInlineCursor;
struct NGTextFragmentPaintInfo;
struct PaintInfo;
struct PhysicalOffset;
struct PhysicalRect;
struct PhysicalSize;

// Text fragment painter for LayoutNG. Operates on NGFragmentItem that IsText()
// and handles clipping, selection, etc. Delegates to NGTextPainter to paint the
// text itself.
class NGTextFragmentPainter {
  STACK_ALLOCATED();

 public:
  explicit NGTextFragmentPainter(const NGInlineCursor& cursor)
      : cursor_(cursor) {}
  NGTextFragmentPainter(const NGInlineCursor& cursor,
                        const PhysicalOffset& parent_offset)
      : cursor_(cursor), parent_offset_(parent_offset) {}

  void Paint(const PaintInfo&, const PhysicalOffset& paint_offset);

 private:
  void Paint(const NGTextFragmentPaintInfo& fragment_paint_info,
             const LayoutObject* layout_object,
             const DisplayItemClient& display_item_client,
             const ComputedStyle& style,
             PhysicalRect box_rect,
             const gfx::Rect& visual_rect,
             bool is_ellipsis,
             bool is_symbol_marker,
             const PaintInfo& paint_info,
             const PhysicalOffset& paint_offset);

  static void PaintSymbol(const LayoutObject* layout_object,
                          const ComputedStyle& style,
                          const PhysicalSize box_size,
                          const PaintInfo& paint_info,
                          const PhysicalOffset& paint_offset);

  const NGInlineCursor& cursor_;
  PhysicalOffset parent_offset_;
  absl::optional<NGInlineCursor> inline_cursor_for_block_flow_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_FRAGMENT_PAINTER_H_
