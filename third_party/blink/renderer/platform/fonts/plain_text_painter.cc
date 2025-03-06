// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"

#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"

namespace blink {

PlainTextPainter::PlainTextPainter(PlainTextPainter::Mode mode) : mode_(mode) {}

void PlainTextPainter::Trace(Visitor* visitor) const {}

PlainTextPainter& PlainTextPainter::Shared() {
  DCHECK(RuntimeEnabledFeatures::PlainTextPainterEnabled());
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(Persistent<PlainTextPainter>, shared_instance,
                      (MakeGarbageCollected<PlainTextPainter>(kShared)));
  return *shared_instance;
}

const PlainTextNode& PlainTextPainter::SegmentAndShape(const TextRun& run,
                                                       const Font& font) {
  DCHECK(RuntimeEnabledFeatures::CanvasTextNgEnabled() ||
         RuntimeEnabledFeatures::PlainTextPainterEnabled());
  // This function doesn't support DirectionOverride because there are no such
  // callers.
  DCHECK(!run.DirectionalOverride());
  FontCachePurgePreventer purge_preventer;
  return CreateNode(run, font);
}

void PlainTextPainter::DrawWithoutBidi(const TextRun& run,
                                       const Font& font,
                                       cc::PaintCanvas& canvas,
                                       const gfx::PointF& location,
                                       const cc::PaintFlags& flags,
                                       Font::DrawType draw_type) {
  // TODO(crbug.com/389726691): Implement this without Font::DrawText().
  font.DrawText(&canvas, run, location, flags, draw_type);
}

bool PlainTextPainter::DrawWithBidiReorder(
    const TextRun& run,
    unsigned from_index,
    unsigned to_index,
    const Font& font,
    Font::CustomFontNotReadyAction action,
    cc::PaintCanvas& canvas,
    const gfx::PointF& location,
    const cc::PaintFlags& flags,
    Font::DrawType draw_type) {
  // TODO(crbug.com/389726691): Implement this without Font::DrawText().
  TextRunPaintInfo run_info(run);
  run_info.from = from_index;
  run_info.to = to_index;
  return font.DrawBidiText(&canvas, run_info, location, action, flags,
                           draw_type);
}

float PlainTextPainter::ComputeInlineSize(const TextRun& run,
                                          const Font& font,
                                          gfx::RectF* glyph_bounds) {
  FontCachePurgePreventer purge_preventer;
  return CreateNode(run, font).AccumulateInlineSize(glyph_bounds);
}

float PlainTextPainter::ComputeSubInlineSize(const TextRun& run,
                                             unsigned from_index,
                                             unsigned to_index,
                                             const Font& font,
                                             gfx::RectF* glyph_bounds) {
  // TODO(crbug.com/389726691): Implement this without Font::SubRunWidth().
  return font.SubRunWidth(run, from_index, to_index, glyph_bounds);
}

float PlainTextPainter::ComputeInlineSizeWithoutBidi(const TextRun& run,
                                                     const Font& font) {
  FontCachePurgePreventer purge_preventer;
  constexpr bool kSupportsBidi = true;
  return CreateNode(run, font, !kSupportsBidi).AccumulateInlineSize(nullptr);
}

int PlainTextPainter::OffsetForPositionWithoutBidi(
    const TextRun& run,
    const Font& font,
    float position,
    IncludePartialGlyphsOption partial_option,
    BreakGlyphsOption break_option) {
  // TODO(crbug.com/389726691): Implement this without
  // Font::OffsetForPosition().
  return font.OffsetForPosition(run, position, partial_option, break_option);
}

gfx::RectF PlainTextPainter::SelectionRectForTextWithoutBidi(
    const TextRun& run,
    unsigned from_index,
    unsigned to_index,
    const Font& font,
    const gfx::PointF& left_baseline,
    float height) {
  // TODO(crbug.com/389726691): Implement this without
  // Font::SelectionRectForText().
  return font.SelectionRectForText(run, left_baseline, height, from_index,
                                   to_index);
}

const PlainTextNode& PlainTextPainter::CreateNode(const TextRun& text_run,
                                                  const Font& font,
                                                  bool supports_bidi) {
  // TODO(crbug.com/389726691): Introduce a cache.
  return *MakeGarbageCollected<PlainTextNode>(text_run, mode_ == kCanvas, font,
                                              supports_bidi);
}

}  // namespace blink
