/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_H_

#include "cc/paint/node_id.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_iterator.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_list.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/tab_size.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

// To avoid conflicts with the DrawText macro from the Windows SDK...
#undef DrawText

namespace gfx {
class PointF;
class RectF;
}  // namespace gfx

namespace cc {
class PaintCanvas;
class PaintFlags;
}  // namespace cc

namespace blink {

class FontSelector;
struct TextFragmentPaintInfo;

class PLATFORM_EXPORT Font : public GarbageCollected<Font> {
 public:
  Font();
  explicit Font(const FontDescription&);
  Font(const FontDescription&, FontSelector*);

  Font(const Font&) = default;
  Font(Font&&) = default;
  Font& operator=(const Font&) = default;
  Font& operator=(Font&&) = default;

  void Trace(Visitor* visitor) const { visitor->Trace(font_fallback_list_); }

  bool operator==(const Font& other) const;

  const FontDescription& GetFontDescription() const {
    return font_description_;
  }

  enum class DrawType { kGlyphsOnly, kGlyphsAndClusters };

  enum CustomFontNotReadyAction {
    kDoNotPaintIfFontNotReady,
    kUseFallbackIfFontNotReady
  };

  void DrawText(cc::PaintCanvas*,
                const TextFragmentPaintInfo&,
                const gfx::PointF&,
                cc::NodeId node_id,
                const cc::PaintFlags&,
                DrawType = DrawType::kGlyphsOnly) const;
  void DrawEmphasisMarks(cc::PaintCanvas*,
                         const TextFragmentPaintInfo&,
                         const AtomicString& mark,
                         const gfx::PointF&,
                         const cc::PaintFlags&) const;

  gfx::RectF TextInkBounds(const TextFragmentPaintInfo&) const;

  struct TextIntercept {
    float begin_, end_;
  };

  // Compute the text intercepts along the axis of the advance and write them
  // into the specified Vector of TextIntercepts. The number of those is zero or
  // a multiple of two, and is at most the number of glyphs * 2 in the text part
  // of TextFragmentPaintInfo. Specify bounds for the upper and lower extend of
  // a line crossing through the text, parallel to the baseline.
  // TODO(drott): crbug.com/655154 Fix this for upright in vertical.
  void GetTextIntercepts(const TextFragmentPaintInfo&,
                         const cc::PaintFlags&,
                         const std::tuple<float, float>& bounds,
                         Vector<TextIntercept>&) const;

  // Metrics that we query the FontFallbackList for.
  float SpaceWidth() const {
    DCHECK(PrimaryFont());
    return (PrimaryFont() ? PrimaryFont()->SpaceWidth() : 0) +
           GetFontDescription().LetterSpacing();
  }

  // Compute the base tab width; the width when its position is zero.
  float TabWidth(const SimpleFontData*, const TabSize&) const;
  // Compute the tab width for the specified |position|.
  float TabWidth(const SimpleFontData*, const TabSize&, float position) const;
  float TabWidth(const TabSize& tab_size, float position) const {
    return TabWidth(PrimaryFont(), tab_size, position);
  }
  LayoutUnit TabWidth(const TabSize&, LayoutUnit position) const;

  int EmphasisMarkAscent(const AtomicString&) const;
  int EmphasisMarkDescent(const AtomicString&) const;
  int EmphasisMarkHeight(const AtomicString&) const;

  // The inter-script spacing by the CSS `text-autospace` property.
  // https://drafts.csswg.org/css-text-4/#inter-script-spacing
  float TextAutoSpaceInlineSize() const;

  // This may fail and return a nullptr in case the last resort font cannot be
  // loaded. This *should* not happen but in reality it does ever now and then
  // when, for whatever reason, the last resort font cannot be loaded.
  const SimpleFontData* PrimaryFont() const;

  // Returns the primary font that contains the digit zero glyph.
  const SimpleFontData* PrimaryFontWithDigitZero() const;

  // Returns the primary font that contains the CJK water glyph.
  const SimpleFontData* PrimaryFontWithCjkWater() const;

  // Returns the primary font that contains the space glyph for tab-size.
  const SimpleFontData* PrimaryFontForTabSize() const;

  // Returns a list of font features for this `FontDescription`. The returned
  // list is common for all `SimpleFontData` for `this`.
  base::span<const FontFeatureRange> GetFontFeatures() const;

  // True if `this` has any non-initial font features. This includes not only
  // `GetFontFeatures()` but also features computed in later stages.
  bool HasNonInitialFontFeatures() const;

  // Access the NG shape cache associated with this particular font object.
  // Should *not* be retained across layout calls as it may become invalid.
  NGShapeCache& GetNGShapeCache() const;

  // Whether the font supports shaping word by word instead of shaping the
  // full run in one go. Allows better caching for fonts where space cannot
  // participate in kerning and/or ligatures.
  bool CanShapeWordByWord() const;

  void SetCanShapeWordByWordForTesting(bool b) {
    EnsureFontFallbackList()->SetCanShapeWordByWordForTesting(b);
  }

  // Causes PrimaryFont to return nullptr, which is useful for simulating
  // a situation where the "last resort font" did not load.
  void NullifyPrimaryFontForTesting() {
    EnsureFontFallbackList()->NullifyPrimarySimpleFontDataForTesting();
  }

  // Reset `font_fallback_list_` to decouple `SimpleFontData`. This is
  // required not to leak `SimpleFontData` via initial `ComputedStyle`.
  void NullifyForTesting() { font_fallback_list_ = nullptr; }

  void ReportNotDefGlyph() const;

 private:
  enum ForTextEmphasisOrNot { kNotForTextEmphasis, kForTextEmphasis };

  GlyphData GetEmphasisMarkGlyphData(const AtomicString&) const;

  std::pair<float, bool> TabWidthInternal(const SimpleFontData* font_data,
                                          const TabSize& tab_size) const;

 public:
  FontSelector* GetFontSelector() const;
  FontFallbackIterator CreateFontFallbackIterator(
      FontFallbackPriority fallback_priority) const {
    EnsureFontFallbackList();
    return FontFallbackIterator(font_description_, font_fallback_list_,
                                fallback_priority);
  }

  void WillUseFontData(const String& text) const;

  bool IsFallbackValid() const;

  bool ShouldSkipDrawing() const {
    if (!font_fallback_list_)
      return false;
    return EnsureFontFallbackList()->ShouldSkipDrawing();
  }

  bool HasCustomFont() const {
    if (!font_fallback_list_)
      return false;
    return EnsureFontFallbackList()->HasCustomFont();
  }

  // TODO(xiaochengh): The function not only initializes null FontFallbackList,
  // but also syncs invalid FontFallbackList. Rename it for better readability.
  FontFallbackList* EnsureFontFallbackList() const;

 private:
  FontDescription font_description_;
  mutable Member<FontFallbackList> font_fallback_list_;
};

// Uses space as lookup character.
inline const SimpleFontData* Font::PrimaryFont() const {
  return EnsureFontFallbackList()->PrimarySimpleFontDataWithSpace(
      font_description_);
}

// Uses digit zero as lookup character.
inline const SimpleFontData* Font::PrimaryFontWithDigitZero() const {
  return EnsureFontFallbackList()->PrimarySimpleFontDataWithDigitZero(
      font_description_);
}

// Uses CJK water as lookup character.
inline const SimpleFontData* Font::PrimaryFontWithCjkWater() const {
  return EnsureFontFallbackList()->PrimarySimpleFontDataWithCjkWater(
      font_description_);
}

inline const SimpleFontData* Font::PrimaryFontForTabSize() const {
  return EnsureFontFallbackList()->PrimarySimpleFontDataForTabSize(
      font_description_);
}

inline FontSelector* Font::GetFontSelector() const {
  return font_fallback_list_ ? font_fallback_list_->GetFontSelector() : nullptr;
}

inline float Font::TabWidth(const SimpleFontData* font_data,
                            const TabSize& tab_size) const {
  auto [base_tab_width, is_successed] = TabWidthInternal(font_data, tab_size);
  return base_tab_width;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_H_
