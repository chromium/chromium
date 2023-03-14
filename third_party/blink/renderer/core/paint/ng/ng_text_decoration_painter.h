// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_DECORATION_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_DECORATION_PAINTER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/ng/ng_highlight_painter.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"

namespace blink {

class ComputedStyle;
class GraphicsContextStateSaver;
class NGFragmentItem;
class NGTextPainter;
struct PaintInfo;
struct PhysicalRect;
struct TextPaintStyle;

// NGTextFragmentPainter helper that paints text-decoration.
//
// We expose a friendlier interface over NGTextPainter’s decoration
// primitives that’s harder to misuse. Callers of Begin must then call
// PaintExceptLineThrough and PaintOnlyLineThrough, in exactly that order
// (though other painting code may happen in between).
//
// We clip the canvas to ensure that decorations change exactly at the edge of
// any ::selection background, but paint all decorations along the full logical
// width of |decoration_rect|. This yields better results for wavy lines, since
// they stay perfectly continuous and in phase into any highlighted parts.
class CORE_EXPORT NGTextDecorationPainter {
  STACK_ALLOCATED();

 public:
  explicit NGTextDecorationPainter(
      NGTextPainter& text_painter,
      const NGFragmentItem& text_item,
      const PaintInfo& paint_info,
      const ComputedStyle& style,
      const TextPaintStyle& text_style,
      const PhysicalRect& decoration_rect,
      NGHighlightPainter::SelectionPaintState* selection);
  ~NGTextDecorationPainter();

  // Sets the given optional to a new TextDecorationInfo with the decorations
  // that need to be painted, or nullopt if decorations should not be painted.
  void UpdateDecorationInfo(absl::optional<TextDecorationInfo>&,
                            const ComputedStyle&,
                            absl::optional<PhysicalRect> = {},
                            const AppliedTextDecoration* = nullptr);

  enum Phase { kOriginating, kSelection };
  void Begin(Phase phase);
  void PaintExceptLineThrough(const NGTextFragmentPaintInfo&);
  void PaintOnlyLineThrough();

 private:
  enum Step { kBegin, kExcept, kOnly };
  void ClipIfNeeded(GraphicsContextStateSaver&);

  NGTextPainter& text_painter_;
  const NGFragmentItem& text_item_;
  const PaintInfo& paint_info_;
  const ComputedStyle& style_;
  const TextPaintStyle& text_style_;
  const PhysicalRect& decoration_rect_;
  NGHighlightPainter::SelectionPaintState* selection_;

  Step step_;
  Phase phase_;
  absl::optional<TextDecorationInfo> decoration_info_;
  absl::optional<gfx::RectF> clip_rect_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_HIGHLIGHT_PAINTER_H_
