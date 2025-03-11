// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"

#include <cmath>

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"

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
  // Don't draw anything while we are using custom fonts that are in the process
  // of loading, except if the 'force' argument is set to true (in which case it
  // will use a fallback font).
  if (font.ShouldSkipDrawing() && action == Font::kDoNotPaintIfFontNotReady) {
    return false;
  }
  if (!run.length()) {
    return true;
  }

  const PlainTextNode& node = CreateNode(run, font);
  const bool is_sub_run =
      (from_index != 0 || to_index != node.TextContent().length());

  gfx::PointF curr_point = location;
  const ShapeResultBloberizer::Type blob_type =
      draw_type == Font::DrawType::kGlyphsOnly
          ? ShapeResultBloberizer::Type::kNormal
          : ShapeResultBloberizer::Type::kEmitText;
  const FontDescription& font_desc = font.GetFontDescription();
  for (const PlainTextItem& item : node.ItemList()) {
    if (item.EndOffset() <= from_index || to_index <= item.StartOffset()) {
      continue;
    }

    wtf_size_t subrun_from = 0;
    wtf_size_t subrun_to = item.Length();
    CharacterRange range(0, 0, 0, 0);
    if (is_sub_run) [[unlikely]] {
      // Calculate the required indexes for this specific run.
      subrun_from = std::max(0u, from_index - item.StartOffset());
      subrun_to = std::min(item.Length(), to_index - item.StartOffset());
      TextRun subrun(item.Text(), item.Direction());
      const PlainTextNode& sub_node =
          CreateNode(subrun, font, /* supports_bidi */ false);
      // The range provides information required for positioning the subrun.
      range = sub_node.ComputeCharacterRange(subrun_from, subrun_to);
    }

    // STACK_UNINITIALIZED fixes regression with
    // -ftrivial-auto-var-init=pattern. See crbug.com/1055652.
    STACK_UNINITIALIZED ShapeResultBloberizer::FillGlyphsNG bloberizer(
        font_desc, item.Text(), subrun_from, subrun_to, item.EnsureView(),
        blob_type);
    if (is_sub_run) [[unlikely]] {
      // Align the subrun with the point given.
      curr_point.Offset(-range.start, 0);
    }
    DrawTextBlobs(bloberizer.Blobs(), canvas, curr_point, flags);

    if (is_sub_run) [[unlikely]] {
      curr_point.Offset(range.Width(), 0);
    } else {
      curr_point.Offset(bloberizer.Advance(), 0);
    }
  }
  return true;
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
