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

#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_iterator.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_list.h"
#include "third_party/blink/renderer/platform/fonts/ng_text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/bidi_resolver.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/text/text_run_iterator.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace blink {

Font::Font() : can_shape_word_by_word_(0), shape_word_by_word_computed_(0) {}

Font::Font(const FontDescription& fd)
    : font_description_(fd),
      can_shape_word_by_word_(0),
      shape_word_by_word_computed_(0) {}

Font::Font(const Font& other)
    : font_description_(other.font_description_),
      font_fallback_list_(other.font_fallback_list_),
      // TODO(yosin): We should have a comment the reason why don't we copy
      // |m_canShapeWordByWord| and |m_shapeWordByWordComputed| from |other|,
      // since |operator=()| copies them from |other|.
      can_shape_word_by_word_(0),
      shape_word_by_word_computed_(0) {}

Font& Font::operator=(const Font& other) {
  font_description_ = other.font_description_;
  font_fallback_list_ = other.font_fallback_list_;
  can_shape_word_by_word_ = other.can_shape_word_by_word_;
  shape_word_by_word_computed_ = other.shape_word_by_word_computed_;
  return *this;
}

bool Font::operator==(const Font& other) const {
  FontSelector* first =
      font_fallback_list_ ? font_fallback_list_->GetFontSelector() : nullptr;
  FontSelector* second = other.font_fallback_list_
                             ? other.font_fallback_list_->GetFontSelector()
                             : nullptr;

  return first == second && font_description_ == other.font_description_ &&
         (font_fallback_list_
              ? font_fallback_list_->FontSelectorVersion()
              : 0) == (other.font_fallback_list_
                           ? other.font_fallback_list_->FontSelectorVersion()
                           : 0) &&
         (font_fallback_list_ ? font_fallback_list_->Generation() : 0) ==
             (other.font_fallback_list_
                  ? other.font_fallback_list_->Generation()
                  : 0);
}

void Font::Update(FontSelector* font_selector) const {
  // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr,
  // but it ends up being reasonably safe (because inherited fonts in the render
  // tree pick up the new style anyway. Other copies are transient, e.g., the
  // state in the GraphicsContext, and won't stick around long enough to get you
  // in trouble). Still, this is pretty disgusting, and could eventually be
  // rectified by using RefPtrs for Fonts themselves.
  if (!font_fallback_list_)
    font_fallback_list_ = FontFallbackList::Create();
  font_fallback_list_->Invalidate(font_selector);
}

namespace {

void DrawBlobs(cc::PaintCanvas* canvas,
               const cc::PaintFlags& flags,
               const ShapeResultBloberizer::BlobBuffer& blobs,
               const FloatPoint& point,
               cc::NodeId node_id = cc::kInvalidNodeId) {
  for (const auto& blob_info : blobs) {
    DCHECK(blob_info.blob);
    cc::PaintCanvasAutoRestore auto_restore(canvas, false);
    if (blob_info.rotation == CanvasRotationInVertical::kRotateCanvasUpright) {
      canvas->save();

      SkMatrix m;
      m.setSinCos(-1, 0, point.X(), point.Y());
      canvas->concat(m);
    }
    if (node_id != cc::kInvalidNodeId) {
      canvas->drawTextBlob(blob_info.blob, point.X(), point.Y(), node_id,
                           flags);
    } else {
      canvas->drawTextBlob(blob_info.blob, point.X(), point.Y(), flags);
    }
  }
}

}  // anonymous ns

void Font::DrawText(cc::PaintCanvas* canvas,
                    const TextRunPaintInfo& run_info,
                    const FloatPoint& point,
                    float device_scale_factor,
                    const cc::PaintFlags& flags) const {
  DrawText(canvas, run_info, point, device_scale_factor, cc::kInvalidNodeId,
           flags);
}

void Font::DrawText(cc::PaintCanvas* canvas,
                    const TextRunPaintInfo& run_info,
                    const FloatPoint& point,
                    float device_scale_factor,
                    cc::NodeId node_id,
                    const cc::PaintFlags& flags) const {
  // Don't draw anything while we are using custom fonts that are in the process
  // of loading.
  if (ShouldSkipDrawing())
    return;

  ShapeResultBloberizer bloberizer(*this, device_scale_factor);
  CachingWordShaper word_shaper(*this);
  ShapeResultBuffer buffer;
  word_shaper.FillResultBuffer(run_info, &buffer);
  bloberizer.FillGlyphs(run_info, buffer);
  DrawBlobs(canvas, flags, bloberizer.Blobs(), point, node_id);
}

void Font::DrawText(cc::PaintCanvas* canvas,
                    const NGTextFragmentPaintInfo& text_info,
                    const FloatPoint& point,
                    float device_scale_factor,
                    cc::NodeId node_id,
                    const cc::PaintFlags& flags) const {
  // Don't draw anything while we are using custom fonts that are in the process
  // of loading.
  if (ShouldSkipDrawing())
    return;

  ShapeResultBloberizer bloberizer(*this, device_scale_factor);
  bloberizer.FillGlyphs(text_info.text, text_info.from, text_info.to,
                        text_info.shape_result);
  DrawBlobs(canvas, flags, bloberizer.Blobs(), point, node_id);
}

bool Font::DrawBidiText(cc::PaintCanvas* canvas,
                        const TextRunPaintInfo& run_info,
                        const FloatPoint& point,
                        CustomFontNotReadyAction custom_font_not_ready_action,
                        float device_scale_factor,
                        const cc::PaintFlags& flags) const {
  // Don't draw anything while we are using custom fonts that are in the process
  // of loading, except if the 'force' argument is set to true (in which case it
  // will use a fallback font).
  if (ShouldSkipDrawing() &&
      custom_font_not_ready_action == kDoNotPaintIfFontNotReady)
    return false;

  // sub-run painting is not supported for Bidi text.
  const TextRun& run = run_info.run;
  DCHECK_EQ(run_info.from, 0u);
  DCHECK_EQ(run_info.to, run.length());
  BidiResolver<TextRunIterator, BidiCharacterRun> bidi_resolver;
  bidi_resolver.SetStatus(
      BidiStatus(run.Direction(), run.DirectionalOverride()));
  bidi_resolver.SetPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));

  // FIXME: This ownership should be reversed. We should pass BidiRunList
  // to BidiResolver in createBidiRunsForLine.
  BidiRunList<BidiCharacterRun>& bidi_runs = bidi_resolver.Runs();
  bidi_resolver.CreateBidiRunsForLine(TextRunIterator(&run, run.length()));
  if (!bidi_runs.RunCount())
    return true;

  FloatPoint curr_point = point;
  BidiCharacterRun* bidi_run = bidi_runs.FirstRun();
  CachingWordShaper word_shaper(*this);
  while (bidi_run) {
    TextRun subrun =
        run.SubRun(bidi_run->Start(), bidi_run->Stop() - bidi_run->Start());
    bool is_rtl = bidi_run->Level() % 2;
    subrun.SetDirection(is_rtl ? TextDirection::kRtl : TextDirection::kLtr);
    subrun.SetDirectionalOverride(bidi_run->DirOverride(false));

    TextRunPaintInfo subrun_info(subrun);

    ShapeResultBloberizer bloberizer(*this, device_scale_factor);
    ShapeResultBuffer buffer;
    word_shaper.FillResultBuffer(subrun_info, &buffer);
    float run_width = bloberizer.FillGlyphs(subrun_info, buffer);
    DrawBlobs(canvas, flags, bloberizer.Blobs(), curr_point);

    bidi_run = bidi_run->Next();
    curr_point.Move(run_width, 0);
  }

  bidi_runs.DeleteRuns();
  return true;
}

void Font::DrawEmphasisMarks(cc::PaintCanvas* canvas,
                             const TextRunPaintInfo& run_info,
                             const AtomicString& mark,
                             const FloatPoint& point,
                             float device_scale_factor,
                             const cc::PaintFlags& flags) const {
  if (ShouldSkipDrawing())
    return;

  FontCachePurgePreventer purge_preventer;

  const auto emphasis_glyph_data = GetEmphasisMarkGlyphData(mark);
  if (!emphasis_glyph_data.font_data)
    return;

  ShapeResultBloberizer bloberizer(*this, device_scale_factor);
  CachingWordShaper word_shaper(*this);
  ShapeResultBuffer buffer;
  word_shaper.FillResultBuffer(run_info, &buffer);
  bloberizer.FillTextEmphasisGlyphs(run_info, emphasis_glyph_data, buffer);
  DrawBlobs(canvas, flags, bloberizer.Blobs(), point);
}

void Font::DrawEmphasisMarks(cc::PaintCanvas* canvas,
                             const NGTextFragmentPaintInfo& text_info,
                             const AtomicString& mark,
                             const FloatPoint& point,
                             float device_scale_factor,
                             const cc::PaintFlags& flags) const {
  if (ShouldSkipDrawing())
    return;

  FontCachePurgePreventer purge_preventer;
  const auto emphasis_glyph_data = GetEmphasisMarkGlyphData(mark);
  if (!emphasis_glyph_data.font_data)
    return;

  ShapeResultBloberizer bloberizer(*this, device_scale_factor);
  bloberizer.FillTextEmphasisGlyphs(text_info.text, text_info.from,
                                    text_info.to, emphasis_glyph_data,
                                    text_info.shape_result);
  DrawBlobs(canvas, flags, bloberizer.Blobs(), point);
}

FloatRect Font::TextInkBounds(const NGTextFragmentPaintInfo& text_info) const {
  // No need to compute bounds if using custom fonts that are in the process
  // of loading as it won't be painted.
  if (ShouldSkipDrawing())
    return FloatRect();

  // NOTE(eae): We could use the SkTextBlob::bounds API [1] however by default
  // it returns conservative bounds (rather than tight bounds) which are
  // unsuitable for our needs. If we could get the tight bounds from Skia that
  // would be quite a bit faster than the two-stage approach employed by the
  // ShapeResultView::ComputeInkBounds method.
  // 1: https://skia.org/user/api/SkTextBlob_Reference#SkTextBlob_bounds
  return text_info.shape_result->ComputeInkBounds();
}

float Font::Width(const TextRun& run,
                  HashSet<const SimpleFontData*>* fallback_fonts,
                  FloatRect* glyph_bounds) const {
  FontCachePurgePreventer purge_preventer;
  CachingWordShaper shaper(*this);
  return shaper.Width(run, fallback_fonts, glyph_bounds);
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
    if (blob_info.rotation == CanvasRotationInVertical::kRotateCanvasUpright)
      continue;

    SkScalar* offset_intercepts_buffer = nullptr;
    if (intercepts_buffer)
      offset_intercepts_buffer = &intercepts_buffer[num_intervals];
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

void Font::GetTextIntercepts(const TextRunPaintInfo& run_info,
                             float device_scale_factor,
                             const cc::PaintFlags& flags,
                             const std::tuple<float, float>& bounds,
                             Vector<TextIntercept>& intercepts) const {
  if (ShouldSkipDrawing())
    return;

  ShapeResultBloberizer bloberizer(
      *this, device_scale_factor, ShapeResultBloberizer::Type::kTextIntercepts);
  CachingWordShaper word_shaper(*this);
  ShapeResultBuffer buffer;
  word_shaper.FillResultBuffer(run_info, &buffer);
  bloberizer.FillGlyphs(run_info, buffer);

  GetTextInterceptsInternal(bloberizer.Blobs(), flags, bounds, intercepts);
}

void Font::GetTextIntercepts(const NGTextFragmentPaintInfo& text_info,
                             float device_scale_factor,
                             const cc::PaintFlags& flags,
                             const std::tuple<float, float>& bounds,
                             Vector<TextIntercept>& intercepts) const {
  if (ShouldSkipDrawing())
    return;

  ShapeResultBloberizer bloberizer(
      *this, device_scale_factor, ShapeResultBloberizer::Type::kTextIntercepts);
  bloberizer.FillGlyphs(text_info.text, text_info.from, text_info.to,
                        text_info.shape_result);

  GetTextInterceptsInternal(bloberizer.Blobs(), flags, bounds, intercepts);
}

static inline FloatRect PixelSnappedSelectionRect(FloatRect rect) {
  // Using roundf() rather than ceilf() for the right edge as a compromise to
  // ensure correct caret positioning.
  float rounded_x = roundf(rect.X());
  return FloatRect(rounded_x, rect.Y(), roundf(rect.MaxX() - rounded_x),
                   rect.Height());
}

FloatRect Font::SelectionRectForText(const TextRun& run,
                                     const FloatPoint& point,
                                     float height,
                                     int from,
                                     int to) const {
  to = (to == -1 ? run.length() : to);

  FontCachePurgePreventer purge_preventer;

  CachingWordShaper shaper(*this);
  CharacterRange range = shaper.GetCharacterRange(run, from, to);

  return PixelSnappedSelectionRect(
      FloatRect(point.X() + range.start, point.Y(), range.Width(), height));
}

FloatRect Font::BoundingBox(const TextRun& run, int from, int to) const {
  to = (to == -1 ? run.length() : to);
  FontCachePurgePreventer purge_preventer;
  CachingWordShaper shaper(*this);
  CharacterRange range = shaper.GetCharacterRange(run, from, to);
  return FloatRect(range.start, -range.ascent, range.Width(), range.Height());
}

int Font::OffsetForPosition(const TextRun& run,
                            float x_float,
                            IncludePartialGlyphsOption partial_glyphs,
                            BreakGlyphsOption break_glyphs) const {
  FontCachePurgePreventer purge_preventer;
  CachingWordShaper shaper(*this);
  return shaper.OffsetForPosition(run, x_float, partial_glyphs, break_glyphs);
}

ShapeCache* Font::GetShapeCache() const {
  return font_fallback_list_->GetShapeCache(font_description_);
}

bool Font::CanShapeWordByWord() const {
  if (!shape_word_by_word_computed_) {
    can_shape_word_by_word_ = ComputeCanShapeWordByWord();
    shape_word_by_word_computed_ = true;
  }
  return can_shape_word_by_word_;
}

bool Font::ComputeCanShapeWordByWord() const {
  if (!GetFontDescription().GetTypesettingFeatures())
    return true;

  if (!PrimaryFont())
    return false;

  const FontPlatformData& platform_data = PrimaryFont()->PlatformData();
  TypesettingFeatures features = GetFontDescription().GetTypesettingFeatures();
  return !platform_data.HasSpaceInLigaturesOrKerning(features);
}

void Font::ReportNotDefGlyph() const {
  FontSelector* fontSelector = font_fallback_list_->GetFontSelector();
  // We have a few non-DOM usages of Font code, for example in DragImage::Create
  // and in EmbeddedObjectPainter::paintReplaced. In those cases, we can't
  // retrieve a font selector as our connection to a Document object to report
  // UseCounter metrics, and thus we cannot report notdef glyphs.
  if (fontSelector)
    fontSelector->ReportNotDefGlyph();
}

void Font::WillUseFontData(const String& text) const {
  const FontFamily& family = GetFontDescription().Family();
  if (font_fallback_list_ && font_fallback_list_->GetFontSelector() &&
      !family.FamilyIsEmpty())
    font_fallback_list_->GetFontSelector()->WillUseFontData(
        GetFontDescription(), family.Family(), text);
}

scoped_refptr<FontFallbackIterator> Font::CreateFontFallbackIterator(
    FontFallbackPriority fallback_priority) const {
  return FontFallbackIterator::Create(font_description_, font_fallback_list_,
                                      fallback_priority);
}

GlyphData Font::GetEmphasisMarkGlyphData(const AtomicString& mark) const {
  if (mark.IsEmpty())
    return GlyphData();

  TextRun emphasis_mark_run(mark, mark.length());
  return CachingWordShaper(*this).EmphasisMarkGlyphData(emphasis_mark_run);
}

int Font::EmphasisMarkAscent(const AtomicString& mark) const {
  FontCachePurgePreventer purge_preventer;

  const auto mark_glyph_data = GetEmphasisMarkGlyphData(mark);
  const SimpleFontData* mark_font_data = mark_glyph_data.font_data;
  if (!mark_font_data)
    return 0;

  return mark_font_data->GetFontMetrics().Ascent();
}

int Font::EmphasisMarkDescent(const AtomicString& mark) const {
  FontCachePurgePreventer purge_preventer;

  const auto mark_glyph_data = GetEmphasisMarkGlyphData(mark);
  const SimpleFontData* mark_font_data = mark_glyph_data.font_data;
  if (!mark_font_data)
    return 0;

  return mark_font_data->GetFontMetrics().Descent();
}

int Font::EmphasisMarkHeight(const AtomicString& mark) const {
  FontCachePurgePreventer purge_preventer;

  const auto mark_glyph_data = GetEmphasisMarkGlyphData(mark);
  const SimpleFontData* mark_font_data = mark_glyph_data.font_data;
  if (!mark_font_data)
    return 0;

  return mark_font_data->GetFontMetrics().Height();
}

CharacterRange Font::GetCharacterRange(const TextRun& run,
                                       unsigned from,
                                       unsigned to) const {
  FontCachePurgePreventer purge_preventer;
  CachingWordShaper shaper(*this);
  return shaper.GetCharacterRange(run, from, to);
}

Vector<CharacterRange> Font::IndividualCharacterRanges(
    const TextRun& run) const {
  FontCachePurgePreventer purge_preventer;
  CachingWordShaper shaper(*this);
  auto ranges = shaper.IndividualCharacterRanges(run);
  // The shaper should return ranges.size == run.length but on some platforms
  // (OSX10.9.5) we are seeing cases in the upper end of the unicode range
  // where this is not true (see: crbug.com/620952). To catch these cases on
  // more popular platforms, and to protect users, we are using a CHECK here.
  CHECK_EQ(ranges.size(), run.length());
  return ranges;
}

Vector<double> Font::IndividualCharacterAdvances(const TextRun& run) const {
  FontCachePurgePreventer purge_preventer;
  CachingWordShaper shaper(*this);
  return shaper.IndividualCharacterAdvances(run);
}

void Font::ExpandRangeToIncludePartialGlyphs(const TextRun& text_run,
                                             int* from,
                                             int* to) const {
  TextRunPaintInfo run_info(text_run);
  run_info.from = *from;
  run_info.to = *to;
  CachingWordShaper word_shaper(*this);
  ShapeResultBuffer buffer;
  word_shaper.FillResultBuffer(run_info, &buffer);
  buffer.ExpandRangeToIncludePartialGlyphs(from, to);
}

float Font::TabWidth(const SimpleFontData* font_data,
                     const TabSize& tab_size,
                     float position) const {
  float base_tab_width = TabWidth(font_data, tab_size);
  if (!base_tab_width)
    return GetFontDescription().LetterSpacing();

  float distance_to_tab_stop = base_tab_width - fmodf(position, base_tab_width);

  // Let the minimum width be the half of the space width so that it's always
  // recognizable.  if the distance to the next tab stop is less than that,
  // advance an additional tab stop.
  if (distance_to_tab_stop < font_data->SpaceWidth() / 2)
    distance_to_tab_stop += base_tab_width;

  return distance_to_tab_stop;
}

LayoutUnit Font::TabWidth(const TabSize& tab_size, LayoutUnit position) const {
  const SimpleFontData* font_data = PrimaryFont();
  if (!font_data)
    return LayoutUnit::FromFloatCeil(GetFontDescription().LetterSpacing());
  float base_tab_width = tab_size.GetPixelSize(font_data->SpaceWidth());
  if (!base_tab_width)
    return LayoutUnit::FromFloatCeil(GetFontDescription().LetterSpacing());

  LayoutUnit distance_to_tab_stop = LayoutUnit::FromFloatFloor(
      base_tab_width - fmodf(position, base_tab_width));

  // Let the minimum width be the half of the space width so that it's always
  // recognizable.  if the distance to the next tab stop is less than that,
  // advance an additional tab stop.
  if (distance_to_tab_stop < font_data->SpaceWidth() / 2)
    distance_to_tab_stop += base_tab_width;

  return distance_to_tab_stop;
}

bool Font::LoadingCustomFonts() const {
  return font_fallback_list_ && font_fallback_list_->LoadingCustomFonts();
}

bool Font::IsFallbackValid() const {
  return !font_fallback_list_ || font_fallback_list_->IsValid();
}

}  // namespace blink
