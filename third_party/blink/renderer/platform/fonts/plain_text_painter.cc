// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"

#include <cmath>

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/frame_shape_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

PlainTextPainter::PlainTextPainter(PlainTextPainter::Mode mode) : mode_(mode) {
  // We don't use FrameShapeCache in the kShared mode. See GetCacheFor().
  if (mode_ == kCanvas &&
      RuntimeEnabledFeatures::CanvasTextMemoryPressureEnabled()) {
    memory_pressure_listener_registration_.emplace(
        FROM_HERE, base::MemoryPressureListenerTag::kPlainTextPainter, this);
  }
}

void PlainTextPainter::Trace(Visitor* visitor) const {
  visitor->Trace(cache_map_);
}

void PlainTextPainter::Dispose() {
  if (memory_pressure_listener_registration_) {
    memory_pressure_listener_registration_->Dispose();
  }
}

PlainTextPainter& PlainTextPainter::Shared() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(Persistent<PlainTextPainter>, shared_instance,
                      (MakeGarbageCollected<PlainTextPainter>(kShared)));
  return *shared_instance;
}

const PlainTextNode& PlainTextPainter::SegmentAndShape(const TextRun& run,
                                                       const Font& font) {
  // This function doesn't support DirectionOverride because there are no such
  // callers.
  DCHECK(!run.DirectionalOverride());
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

  if (!is_sub_run && !node.ContainsRtlItems()) [[likely]] {
    STACK_UNINITIALIZED ShapeResultBloberizer::FillGlyphs bloberizer(
        font_desc, node, blob_type);
    DrawTextBlobs(bloberizer.Blobs(), canvas, curr_point, flags);
    return true;
  }
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
    TextRun text_run(sub_text, item.Direction());
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
  constexpr bool kSupportsBidi = true;
  return CreateNode(run, font, !kSupportsBidi).AccumulateInlineSize(nullptr);
}

void PlainTextPainter::DidSwitchFrame() {
  for (auto& cache : cache_map_.Values()) {
    cache->DidSwitchFrame();
  }
}

const PlainTextNode& PlainTextPainter::CreateNode(const TextRun& text_run,
                                                  const Font& font,
                                                  bool supports_bidi) {
  FrameShapeCache* cache = GetCacheFor(font);
  if (!cache || !supports_bidi || text_run.DirectionalOverride()) {
    // `supports_bidi` and `DirectionalOverride()` affect segmentation results.
    // So PlainTextNode is not cached.  However we can use ShapeResults in the
    // cache.
    return *MakeGarbageCollected<PlainTextNode>(text_run, mode_ == kCanvas,
                                                font, supports_bidi, cache);
  }
  String text = text_run.ToStringView().ToString();
  FrameShapeCache::NodeEntry* entry =
      cache->FindOrCreateNodeEntry(text, text_run.Direction());
  if (entry->node) {
    return *entry->node;
  }
  auto* node = MakeGarbageCollected<PlainTextNode>(text_run, mode_ == kCanvas,
                                                   font, supports_bidi, cache);
  cache->RegisterNodeEntry(text, text_run.Direction(), node, entry);
  return *node;
}

FrameShapeCache* PlainTextPainter::GetCacheFor(const Font& font) {
  if (mode_ == kShared || MemoryPressureListenerRegistry::IsLowEndDevice()) {
    return nullptr;
  }
  FontFallbackList* key = font.EnsureFontFallbackList();
  auto result = cache_map_.insert(key, Member<FrameShapeCache>());
  FrameShapeCache* cache;
  if (result.is_new_entry) {
    cache = MakeGarbageCollected<FrameShapeCache>();
    result.stored_value->value = cache;
  } else {
    cache = result.stored_value->value;
  }
  return cache;
}

void PlainTextPainter::OnMemoryPressure(
    base::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level == base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    cache_map_.clear();
  }
}

}  // namespace blink
