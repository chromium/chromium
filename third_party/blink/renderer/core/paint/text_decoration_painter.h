// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_DECORATION_PAINTER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/highlight_painter.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"

namespace blink {

class ComputedStyle;
class FragmentItem;
class GraphicsContextStateSaver;
class TextPainter;
struct LineRelativeRect;
struct PaintInfo;
struct TextPaintStyle;

// TextFragmentPainter helper that paints text-decoration.
//
// We expose a friendlier interface over TextPainter’s decoration
// primitives that’s harder to misuse. Callers of Begin must then call
// PaintExceptLineThrough and PaintOnlyLineThrough, in exactly that order
// (though other painting code may happen in between).
//
// We clip the canvas to ensure that decorations change exactly at the edge of
// any ::selection background, but paint all decorations along the full logical
// width of |decoration_rect|. This yields better results for wavy lines, since
// they stay perfectly continuous and in phase into any highlighted parts.
class CORE_EXPORT TextDecorationPainter {
  STACK_ALLOCATED();

 public:
  explicit TextDecorationPainter(
      TextPainter& text_painter,
      const FragmentItem& text_item,
      const PaintInfo& paint_info,
      const ComputedStyle& style,
      const TextPaintStyle& text_style,
      const LineRelativeRect& decoration_rect,
      HighlightPainter::SelectionPaintState* selection);
  ~TextDecorationPainter();

  // Sets the given optional to a new TextDecorationInfo with the decorations
  // that need to be painted, or nullopt if decorations should not be painted.
  void UpdateDecorationInfo(absl::optional<TextDecorationInfo>&,
                            const ComputedStyle&,
                            absl::optional<LineRelativeRect> = {},
                            const AppliedTextDecoration* = nullptr);

  enum Phase { kOriginating, kSelection };
  void Begin(Phase phase);
  void PaintExceptLineThrough(const TextFragmentPaintInfo&);
  void PaintOnlyLineThrough();

 private:
  enum Step { kBegin, kExcept, kOnly };
  void ClipIfNeeded(GraphicsContextStateSaver&);

  TextPainter& text_painter_;
  const FragmentItem& text_item_;
  const PaintInfo& paint_info_;
  const ComputedStyle& style_;
  const TextPaintStyle& text_style_;
  const LineRelativeRect& decoration_rect_;
  HighlightPainter::SelectionPaintState* selection_;

  Step step_;
  Phase phase_;
  absl::optional<TextDecorationInfo> decoration_info_;
  absl::optional<gfx::RectF> clip_rect_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_HIGHLIGHT_PAINTER_H_
