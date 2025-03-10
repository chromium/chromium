// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"

#include <cmath>

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"
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
  // Don't draw anything while we are using custom fonts that are in the process
  // of loading.
  if (font.ShouldSkipDrawing()) {
    return;
  }

  const PlainTextNode& node = CreateNode(run, font, /* supports_bidi */ false);
  gfx::PointF point = location;
  for (const auto& item : node.ItemList()) {
    ShapeResultBloberizer::FillGlyphsNG bloberizer(
        font.GetFontDescription(), item.Text(), 0, item.Length(),
        item.EnsureView(),
        draw_type == Font::DrawType::kGlyphsOnly
            ? ShapeResultBloberizer::Type::kNormal
            : ShapeResultBloberizer::Type::kEmitText);
    DrawTextBlobs(bloberizer.Blobs(), canvas, point, flags, cc::kInvalidNodeId);
    point.Offset(bloberizer.Advance(), 0);
  }
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
  if (run.length() == 0) {
    return 0;
  }
  FontCachePurgePreventer purge_preventer;

  const PlainTextNode& node = CreateNode(run, font);
  float x_pos = 0;
  for (const auto& item : node.ItemList()) {
    wtf_size_t start_offset = item.StartOffset();
    if (item.EndOffset() <= from_index || to_index <= start_offset) {
      continue;
    }
    // Calculate the required indexes for this specific run.
    unsigned run_from = std::max(0u, from_index - start_offset);
    unsigned run_to = std::min(item.Length(), to_index - start_offset);
    // Measure the subrun.
    StringView sub_text(node.TextContent(), start_offset, item.Length());
    TextRun text_run(sub_text, item.Direction(),
                     /* directional_override */ false, mode_ == kCanvas);
    const PlainTextNode& sub_node =
        CreateNode(text_run, font, /* supports_bidi */ false);
    CharacterRange character_range =
        sub_node.ComputeCharacterRange(run_from, run_to);

    // Accumulate the position and the glyph bounding box.
    if (glyph_bounds) {
      gfx::RectF range_bounds(character_range.start, -character_range.ascent,
                              character_range.Width(),
                              character_range.Height());
      // ComputeCharacterRange() returns bounds positioned as if the whole run
      // was there, so the rect has to be moved to align with the current
      // position.
      range_bounds.Offset(-range_bounds.x() + x_pos, 0);
      glyph_bounds->Union(range_bounds);
    }
    x_pos += character_range.Width();
  }
  if (glyph_bounds) {
    glyph_bounds->Offset(-glyph_bounds->x(), 0);
  }
  return x_pos;
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
  const PlainTextNode& node = CreateNode(run, font, /* supports_bidi */ false);
  unsigned total_offset;
  if (run.Rtl()) {
    total_offset = node.TextContent().length();
    for (const auto& item : base::Reversed(node.ItemList())) {
      const ShapeResult* word_result = item.GetShapeResult();
      total_offset -= word_result->NumCharacters();
      if (position >= 0 && position <= word_result->Width()) {
        int offset_for_word = word_result->OffsetForPosition(
            position, item.Text(), partial_option, break_option);
        return total_offset + offset_for_word;
      }
      position -= word_result->Width();
    }
  } else {
    total_offset = 0;
    for (const auto& item : node.ItemList()) {
      const ShapeResult* word_result = item.GetShapeResult();
      int offset_for_word = word_result->OffsetForPosition(
          position, item.Text(), partial_option, break_option);
      DCHECK_GE(offset_for_word, 0);
      total_offset += offset_for_word;
      if (position >= 0 && position <= word_result->Width()) {
        return total_offset;
      }
      position -= word_result->Width();
    }
  }
  return total_offset;
}

gfx::RectF PlainTextPainter::SelectionRectForTextWithoutBidi(
    const TextRun& run,
    unsigned from_index,
    unsigned to_index,
    const Font& font,
    const gfx::PointF& left_baseline,
    float height) {
  const PlainTextNode& node = CreateNode(run, font, /* supports_bidi */ false);
  CharacterRange range = node.ComputeCharacterRange(from_index, to_index);
  float rounded_x = std::round(left_baseline.x() + range.start);
  return gfx::RectF(
      rounded_x, left_baseline.y(),
      std::round(left_baseline.x() + range.start + range.Width()) - rounded_x,
      height);
}

const PlainTextNode& PlainTextPainter::CreateNode(const TextRun& text_run,
                                                  const Font& font,
                                                  bool supports_bidi) {
  // TODO(crbug.com/389726691): Introduce a cache.
  return *MakeGarbageCollected<PlainTextNode>(text_run, mode_ == kCanvas, font,
                                              supports_bidi);
}

}  // namespace blink
