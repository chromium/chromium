/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (c) 2007, 2008, 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/platform/fonts/font.h"

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_list.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_map.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {

FontFallbackList* GetOrCreateFontFallbackList(
    const FontDescription& font_description,
    FontSelector* font_selector) {
  FontFallbackMap& fallback_map = font_selector
                                      ? font_selector->GetFontFallbackMap()
                                      : FontCache::Get().GetFontFallbackMap();
  return fallback_map.Get(font_description);
}

}  // namespace

Font::Font() = default;

Font::Font(const FontDescription& fd) : font_description_(fd) {}

Font::Font(const FontDescription& font_description, FontSelector* font_selector)
    : font_description_(font_description),
      font_fallback_list_(
          font_selector
              ? GetOrCreateFontFallbackList(font_description, font_selector)
              : nullptr) {}

FontFallbackList* Font::EnsureFontFallbackList() const {
  if (!font_fallback_list_ || !font_fallback_list_->IsValid()) {
    font_fallback_list_ =
        GetOrCreateFontFallbackList(font_description_, GetFontSelector());
  }
  return font_fallback_list_.Get();
}

bool Font::operator==(const Font& other) const {
  // Font objects with the same FontDescription and FontSelector should always
  // hold reference to the same FontFallbackList object, unless invalidated.
  if (font_fallback_list_ && font_fallback_list_->IsValid() &&
      other.font_fallback_list_ && other.font_fallback_list_->IsValid()) {
    return font_fallback_list_ == other.font_fallback_list_;
  }

  return GetFontSelector() == other.GetFontSelector() &&
         font_description_ == other.font_description_;
}

void Font::DrawText(cc::PaintCanvas* canvas,
                    const TextFragmentPaintInfo& text_info,
                    const gfx::PointF& point,
                    cc::NodeId node_id,
                    const cc::PaintFlags& flags,
                    DrawType draw_type) const {
  // Don't draw anything while we are using custom fonts that are in the process
  // of loading.
  if (ShouldSkipDrawing())
    return;

  // For performance avoid stack initialization on this large object.
  STACK_UNINITIALIZED ShapeResultBloberizer::FillGlyphsNG bloberizer(
      GetFontDescription(), text_info.text, text_info.from, text_info.to,
      text_info.shape_result,
      draw_type == Font::DrawType::kGlyphsOnly
          ? ShapeResultBloberizer::Type::kNormal
          : ShapeResultBloberizer::Type::kEmitText);
  DrawTextBlobs(bloberizer.Blobs(), *canvas, point, flags, node_id);
}

void Font::DrawEmphasisMarks(cc::PaintCanvas* canvas,
                             const TextFragmentPaintInfo& text_info,
                             const AtomicString& mark,
                             const gfx::PointF& point,
                             const cc::PaintFlags& flags) const {
  if (ShouldSkipDrawing())
    return;

  const auto emphasis_glyph_data = GetEmphasisMarkGlyphData(mark);
  if (!emphasis_glyph_data.font_data)
    return;

  ShapeResultBloberizer::FillTextEmphasisGlyphsNG bloberizer(
      GetFontDescription(), text_info.text, text_info.from, text_info.to,
      text_info.shape_result, emphasis_glyph_data);
  DrawTextBlobs(bloberizer.Blobs(), *canvas, point, flags);
}

gfx::RectF Font::TextInkBounds(const TextFragmentPaintInfo& text_info) const {
  // No need to compute bounds if using custom fonts that are in the process
  // of loading as it won't be painted.
  if (ShouldSkipDrawing())
    return gfx::RectF();

  // NOTE(eae): We could use the SkTextBlob::bounds API [1] however by default
  // it returns conservative bounds (rather than tight bounds) which are
  // unsuitable for our needs. If we could get the tight bounds from Skia that
  // would be quite a bit faster than the two-stage approach employed by the
  // ShapeResultView::ComputeInkBounds method.
  // 1: https://skia.org/user/api/SkTextBlob_Reference#SkTextBlob_bounds
  return text_info.shape_result->ComputeInkBounds();
}

namespace {  // anonymous namespace

unsigned InterceptsFromBlobs(const ShapeResultBloberizer::BlobBuffer& blobs,
                             const SkPaint& paint,
                             const std::tuple<float, float>& bounds,
                             SkScalar* intercepts_buffer) {
  SkScalar bounds_array[2] = {std::get<0>(bounds), std::get<1>(bounds)};

  unsigned num_intervals = 0;
  for (const auto& blob_info : blobs) {
    DCHECK(blob_info.blob);

    // ShapeResultBloberizer splits for a new blob rotation, but does not split
    // for a change in font. A TextBlob can contain runs with differing fonts
    // and the getTextBlobIntercepts method handles multiple fonts for us. For
    // upright in vertical blobs we currently have to bail, see crbug.com/655154
    if (IsCanvasRotationInVerticalUpright(blob_info.rotation))
      continue;

    SkScalar* offset_intercepts_buffer = nullptr;
    if (intercepts_buffer)
      offset_intercepts_buffer = UNSAFE_TODO(&intercepts_buffer[num_intervals]);
    num_intervals += blob_info.blob->getIntercepts(
        bounds_array, offset_intercepts_buffer, &paint);
  }
  return num_intervals;
}

void GetTextInterceptsInternal(const ShapeResultBloberizer::BlobBuffer& blobs,
                               const cc::PaintFlags& flags,
                               const std::tuple<float, float>& bounds,
                               Vector<Font::TextIntercept>& intercepts) {
  // Get the number of intervals, without copying the actual values by
  // specifying nullptr for the buffer, following the Skia allocation model for
  // retrieving text intercepts.
  SkPaint paint = flags.ToSkPaint();
  unsigned num_intervals = InterceptsFromBlobs(blobs, paint, bounds, nullptr);
  if (!num_intervals)
    return;
  DCHECK_EQ(num_intervals % 2, 0u);
  intercepts.resize(num_intervals / 2u);

  InterceptsFromBlobs(blobs, paint, bounds,
                      reinterpret_cast<SkScalar*>(intercepts.data()));
}

}  // anonymous namespace

void Font::GetTextIntercepts(const TextFragmentPaintInfo& text_info,
                             const cc::PaintFlags& flags,
                             const std::tuple<float, float>& bounds,
                             Vector<TextIntercept>& intercepts) const {
  if (ShouldSkipDrawing())
    return;

  ShapeResultBloberizer::FillGlyphsNG bloberizer(
      GetFontDescription(), text_info.text, text_info.from, text_info.to,
      text_info.shape_result, ShapeResultBloberizer::Type::kTextIntercepts);

  GetTextInterceptsInternal(bloberizer.Blobs(), flags, bounds, intercepts);
}

base::span<const FontFeatureRange> Font::GetFontFeatures() const {
  return EnsureFontFallbackList()->GetFontFeatures(font_description_);
}

bool Font::HasNonInitialFontFeatures() const {
  return EnsureFontFallbackList()->HasNonInitialFontFeatures(font_description_);
}

bool Font::CanShapeWordByWord() const {
  return EnsureFontFallbackList()->CanShapeWordByWord(GetFontDescription());
}

void Font::ReportNotDefGlyph() const {
  FontSelector* fontSelector = EnsureFontFallbackList()->GetFontSelector();
  // We have a few non-DOM usages of Font code, for example in DragImage::Create
  // and in EmbeddedObjectPainter::paintReplaced. In those cases, we can't
  // retrieve a font selector as our connection to a Document object to report
  // UseCounter metrics, and thus we cannot report notdef glyphs.
  if (fontSelector)
    fontSelector->ReportNotDefGlyph();
}

void Font::WillUseFontData(const String& text) const {
  const FontDescription& font_description = GetFontDescription();
  const FontFamily& family = font_description.Family();
  if (family.FamilyName().empty()) [[unlikely]] {
    return;
  }
  if (FontSelector* font_selector = GetFontSelector()) {
    font_selector->WillUseFontData(font_description, family, text);
    return;
  }
  // Non-DOM usages can't resolve generic family.
  if (family.IsPrewarmed() || family.FamilyIsGeneric())
    return;
  family.SetIsPrewarmed();
  FontCache::PrewarmFamily(family.FamilyName());
}

GlyphData Font::GetEmphasisMarkGlyphData(const AtomicString& mark) const {
  if (mark.empty())
    return GlyphData();
  return EnsureFontFallbackList()
      ->GetOrCreateEmphasisMarkShape(*this, mark)
      .EmphasisMarkGlyphData(font_description_);
}

int Font::EmphasisMarkAscent(const AtomicString& mark) const {
  const auto mark_glyph_data = GetEmphasisMarkGlyphData(mark);
  const SimpleFontData* mark_font_data = mark_glyph_data.font_data;
  if (!mark_font_data)
    return 0;

  return mark_font_data->GetFontMetrics().Ascent();
}

int Font::EmphasisMarkDescent(const AtomicString& mark) const {
  const auto mark_glyph_data = GetEmphasisMarkGlyphData(mark);
  const SimpleFontData* mark_font_data = mark_glyph_data.font_data;
  if (!mark_font_data)
    return 0;

  return mark_font_data->GetFontMetrics().Descent();
}

int Font::EmphasisMarkHeight(const AtomicString& mark) const {
  const auto mark_glyph_data = GetEmphasisMarkGlyphData(mark);
  const SimpleFontData* mark_font_data = mark_glyph_data.font_data;
  if (!mark_font_data)
    return 0;

  return mark_font_data->GetFontMetrics().Height();
}

float Font::TextAutoSpaceInlineSize() const {
  if (const SimpleFontData* font_data = PrimaryFont()) {
    return font_data->TextAutoSpaceInlineSize();
  }
  NOTREACHED();
}

std::pair<float, bool> Font::TabWidthInternal(const SimpleFontData* font_data,
                                              const TabSize& tab_size) const {
  const auto& font_description = GetFontDescription();
  float letter_spacing = font_description.LetterSpacing();
  if (!font_data) {
    return {letter_spacing, false};
  }
  float word_spacing = font_description.WordSpacing();
  float base_tab_width = tab_size.GetPixelSize(font_data->SpaceWidth(),
                                               letter_spacing, word_spacing);
  if (!base_tab_width) {
    return {letter_spacing, false};
  }
  return {base_tab_width, true};
}

float Font::TabWidth(const SimpleFontData* font_data,
                     const TabSize& tab_size,
                     float position) const {
  float base_tab_width = TabWidth(font_data, tab_size);
  if (!base_tab_width)
    return GetFontDescription().LetterSpacing();

  float modulized_position = fmodf(position, base_tab_width);
  if (RuntimeEnabledFeatures::TabWidthNegativePositionEnabled() &&
      modulized_position < 0) [[unlikely]] {
    modulized_position += base_tab_width;
  }

  float distance_to_tab_stop = base_tab_width - modulized_position;

  // Let the minimum width be the half of the space width so that it's always
  // recognizable.  if the distance to the next tab stop is less than that,
  // advance an additional tab stop.
  if (distance_to_tab_stop < font_data->SpaceWidth() / 2)
    distance_to_tab_stop += base_tab_width;

  return distance_to_tab_stop;
}

LayoutUnit Font::TabWidth(const TabSize& tab_size, LayoutUnit position) const {
  const SimpleFontData* font_data = PrimaryFont();
  auto [base_tab_width, is_successed] = TabWidthInternal(font_data, tab_size);
  if (!is_successed) {
    return LayoutUnit::FromFloatCeil(base_tab_width);
  }

  float modulized_position = fmodf(position, base_tab_width);
  if (RuntimeEnabledFeatures::TabWidthNegativePositionEnabled() &&
      modulized_position < 0) [[unlikely]] {
    modulized_position += base_tab_width;
  }

  LayoutUnit distance_to_tab_stop =
      LayoutUnit::FromFloatFloor(base_tab_width - modulized_position);

  // Let the minimum width be the half of the space width so that it's always
  // recognizable.  if the distance to the next tab stop is less than that,
  // advance an additional tab stop.
  if (distance_to_tab_stop < font_data->SpaceWidth() / 2)
    distance_to_tab_stop += base_tab_width;

  return distance_to_tab_stop;
}

bool Font::IsFallbackValid() const {
  return !font_fallback_list_ || font_fallback_list_->IsValid();
}

}  // namespace blink
