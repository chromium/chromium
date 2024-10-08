/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"

// clang-format off
#include <hb.h>
#include <hb-cplusplus.hh>
#include <hb-ot.h>
// clang-format on

#include <memory>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face_from_typeface.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/variation_selector_mode.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/fonts/skia/skia_text_metrics.h"
#include "third_party/blink/renderer/platform/fonts/unicode_range_set.h"
#include "third_party/blink/renderer/platform/resolution_units.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

HarfBuzzFace::HarfBuzzFace(const FontPlatformData* platform_data,
                           uint64_t unique_id)
    : platform_data_(platform_data),
      harfbuzz_font_data_(FontGlobalContext::GetHarfBuzzFontCache().GetOrCreate(
          unique_id,
          platform_data)) {}

void HarfBuzzFace::Trace(Visitor* visitor) const {
  visitor->Trace(platform_data_);
  visitor->Trace(harfbuzz_font_data_);
}

VariationSelectorMode& GetIgnoreVariationSelectorModeRef() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(WTF::ThreadSpecific<VariationSelectorMode>,
                                  variation_selector_mode, ());
  return *variation_selector_mode;
}

VariationSelectorMode HarfBuzzFace::GetVariationSelectorMode() {
  return GetIgnoreVariationSelectorModeRef();
}

void HarfBuzzFace::SetVariationSelectorMode(VariationSelectorMode value) {
  // Ignore variation selectors mode should be on only when the
  // FontVariationSequences runtime flag is enabled.
  DCHECK(RuntimeEnabledFeatures::FontVariationSequencesEnabled() ||
         !ShouldIgnoreVariationSelector(value));
  DCHECK(RuntimeEnabledFeatures::FontVariantEmojiEnabled() ||
         !UseFontVariantEmojiVariationSelector(value));
  GetIgnoreVariationSelectorModeRef() = value;
}

bool& GetIsSystemFallbackStageRef() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(WTF::ThreadSpecific<bool>,
                                  is_system_fallback_stage, ());
  return *is_system_fallback_stage;
}

bool HarfBuzzFace::GetIsSystemFallbackStage() {
  DCHECK(RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled());
  return GetIsSystemFallbackStageRef();
}

void HarfBuzzFace::SetIsSystemFallbackStage(bool value) {
  DCHECK(RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled());
  GetIsSystemFallbackStageRef() = value;
}

static hb_bool_t HarfBuzzGetGlyph(hb_font_t* hb_font,
                                  void* font_data,
                                  hb_codepoint_t unicode,
                                  hb_codepoint_t variation_selector,
                                  hb_codepoint_t* glyph,
                                  void* user_data) {
  HarfBuzzFontData* hb_font_data =
      reinterpret_cast<HarfBuzzFontData*>(font_data);

  CHECK(hb_font_data);
  if (hb_font_data->range_set_ &&
      !hb_font_data->range_set_->Contains(unicode)) {
    return false;
  }

  // If the system fonts do not have a glyph coverage for line separator
  // (0x2028) or paragraph separator (0x2029), missing glyph would be displayed
  // in Chrome instead. If the system font has l-sep and p-sep symbols for the
  // 0x2028 and 0x2029 codepoints, they will be displayed, see
  // https://crbug.com/550275. To prevent that, we are replacing line and
  // paragraph separators with space, as it is said in unicode specification,
  // compare: https://www.unicode.org/faq/unsup_char.html#2.
  if (unicode == kLineSeparator || unicode == kParagraphSeparator) {
    unicode = kSpaceCharacter;
  }

  bool consider_variation_selector = false;
  bool is_variation_sequence = false;

  // Emoji System Fonts on Mac, Win and Android either do not have cmap 14
  // subtable or it does not include all emojis from their cmap table. We use
  // cmap 14 subtable to identify whether there is a colored (emoji
  // presentation) or a monochromatic (text presentation) glyph in the font.
  // This may lead to the cases when we will not be able to get the glyph ID
  // for the requested variation sequence using fallback system font and will
  // continue the second shaping fallback list pass ignoring variation
  // selectors and may end up using web font with wrong emoji presentation
  // instead of using system font with the correct presentation. To prevent that
  // once we reached system fallback fonts, we can ignore emoji variation
  // selectors since we will get the font with the correct presentation relying
  // on FontFallbackPriority in `FontCache::PlatformFallbackFontForCharacter`.
  VariationSelectorMode variation_selector_mode =
      HarfBuzzFace::GetVariationSelectorMode();
  if (RuntimeEnabledFeatures::FontVariationSequencesEnabled()) {
    if (!ShouldIgnoreVariationSelector(variation_selector_mode) &&
        Character::IsUnicodeVariationSelector(variation_selector) &&
        Character::IsVariationSequence(unicode, variation_selector)) {
      is_variation_sequence = true;
      consider_variation_selector = true;
      // We only want to ignore emoji variation sectors for system fallback
      // fonts, standardized and ideographic variation selectors should be still
      // considered.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
      if (RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled() &&
          Character::IsUnicodeEmojiVariationSelector(variation_selector) &&
          HarfBuzzFace::GetIsSystemFallbackStage()) {
        consider_variation_selector = false;
      }
#endif
    } else if (RuntimeEnabledFeatures::FontVariantEmojiEnabled() &&
               UseFontVariantEmojiVariationSelector(variation_selector_mode) &&
               Character::IsEmoji(unicode)) {
      consider_variation_selector = true;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
      if (RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled() &&
          HarfBuzzFace::GetIsSystemFallbackStage()) {
        consider_variation_selector = false;
      }
#endif
    }
  }

  // Variation sequences are a special case because we want to distinguish
  // between the cases when we found a glyph for the whole variation sequence in
  // cmap format 14 subtable and when we found only a base character of the
  // variation sequence. In the latter case we set the glyph value to
  // `kUnmatchedVSGlyphId`.
  if (consider_variation_selector) {
    if (!is_variation_sequence) {
      if (variation_selector_mode == kForceVariationSelector15 ||
          (variation_selector_mode == kUseUnicodeDefaultPresentation &&
           Character::IsEmojiTextDefault(unicode))) {
        variation_selector = kVariationSelector15Character;
      } else if (variation_selector_mode == kForceVariationSelector16 ||
                 (variation_selector_mode == kUseUnicodeDefaultPresentation &&
                  Character::IsEmojiEmojiDefault(unicode))) {
        variation_selector = kVariationSelector16Character;
      }
    }
    hb_bool_t hb_has_vs_glyph = hb_font_get_variation_glyph(
        hb_font_get_parent(hb_font), unicode, variation_selector, glyph);
    if (hb_has_vs_glyph) {
      // Found a glyph for variation sequence, no need to look for a base
      // character, can just return.
      return true;
    }
    // Unable to find a glyph for variation sequence, now we need to look
    // for a glyph for the base character from variation sequence.
    variation_selector = 0;
  }

  hb_bool_t hb_has_base_glyph = hb_font_get_glyph(
      hb_font_get_parent(hb_font), unicode, variation_selector, glyph);

  if (consider_variation_selector && hb_has_base_glyph) {
    // Unable to find a glyph for variation sequence, but found a glyph for
    // the base character from variation sequence ignoring variation selector.
    *glyph = kUnmatchedVSGlyphId;
  }

// MacOS CoreText API synthesizes GlyphID for several unicode codepoints,
// for example, hyphens and separators for some fonts. HarfBuzz does not
// synthesize such glyphs, and as it's not found from the last resort font, we
// end up with displaying tofu, see https://crbug.com/1267606 for details.
// Chrome uses Times as last resort fallback font and in Times the only visible
// synthesizing characters are hyphen (0x2010) and non-breaking hyphen (0x2011).
// For performance reasons, we limit this fallback lookup to the specific
// missing glyphs for hyphens and only to Mac OS, where we're facing this issue.
#if BUILDFLAG(IS_APPLE)
  if (!hb_has_base_glyph) {
    SkTypeface* typeface = hb_font_data->font_.getTypeface();
    if (!typeface) {
      return false;
    }
    if (unicode == kHyphenCharacter || unicode == kNonBreakingHyphen) {
      SkGlyphID sk_glyph_id = typeface->unicharToGlyph(unicode);
      *glyph = sk_glyph_id;
      return sk_glyph_id;
    }
  }
#endif
  return hb_has_base_glyph;
}

static hb_bool_t HarfBuzzGetNominalGlyph(hb_font_t* hb_font,
                                         void* font_data,
                                         hb_codepoint_t unicode,
                                         hb_codepoint_t* glyph,
                                         void* user_data) {
  return HarfBuzzGetGlyph(hb_font, font_data, unicode, 0, glyph, user_data);
}

static hb_position_t HarfBuzzGetGlyphHorizontalAdvance(hb_font_t* hb_font,
                                                       void* font_data,
                                                       hb_codepoint_t glyph,
                                                       void* user_data) {
  HarfBuzzFontData* hb_font_data =
      reinterpret_cast<HarfBuzzFontData*>(font_data);
  hb_position_t advance = 0;

  SkFontGetGlyphWidthForHarfBuzz(hb_font_data->font_, glyph, &advance);
  return advance;
}

static void HarfBuzzGetGlyphHorizontalAdvances(
    hb_font_t* font,
    void* font_data,
    unsigned count,
    const hb_codepoint_t* first_glyph,
    unsigned int glyph_stride,
    hb_position_t* first_advance,
    unsigned int advance_stride,
    void* user_data) {
  HarfBuzzFontData* hb_font_data =
      reinterpret_cast<HarfBuzzFontData*>(font_data);
  SkFontGetGlyphWidthForHarfBuzz(hb_font_data->font_, count, first_glyph,
                                 glyph_stride, first_advance, advance_stride);
}

static hb_bool_t HarfBuzzGetGlyphVerticalOrigin(hb_font_t* hb_font,
                                                void* font_data,
                                                hb_codepoint_t glyph,
                                                hb_position_t* x,
                                                hb_position_t* y,
                                                void* user_data) {
  HarfBuzzFontData* hb_font_data =
      reinterpret_cast<HarfBuzzFontData*>(font_data);
  OpenTypeVerticalData* vertical_data = hb_font_data->VerticalData();
  if (!vertical_data) {
    return false;
  }

  float result[] = {0, 0};
  Glyph the_glyph = glyph;
  vertical_data->GetVerticalTranslationsForGlyphs(hb_font_data->font_,
                                                  &the_glyph, 1, result);
  *x = SkiaScalarToHarfBuzzPosition(-result[0]);
  *y = SkiaScalarToHarfBuzzPosition(-result[1]);
  return true;
}

static hb_position_t HarfBuzzGetGlyphVerticalAdvance(hb_font_t* hb_font,
                                                     void* font_data,
                                                     hb_codepoint_t glyph,
                                                     void* user_data) {
  HarfBuzzFontData* hb_font_data =
      reinterpret_cast<HarfBuzzFontData*>(font_data);
  OpenTypeVerticalData* vertical_data = hb_font_data->VerticalData();
  if (!vertical_data) {
    return SkiaScalarToHarfBuzzPosition(hb_font_data->height_fallback_);
  }

  Glyph the_glyph = glyph;
  float advance_height = -vertical_data->AdvanceHeight(the_glyph);
  return SkiaScalarToHarfBuzzPosition(SkFloatToScalar(advance_height));
}

static hb_bool_t HarfBuzzGetGlyphExtents(hb_font_t* hb_font,
                                         void* font_data,
                                         hb_codepoint_t glyph,
                                         hb_glyph_extents_t* extents,
                                         void* user_data) {
  HarfBuzzFontData* hb_font_data =
      reinterpret_cast<HarfBuzzFontData*>(font_data);

  SkFontGetGlyphExtentsForHarfBuzz(hb_font_data->font_, glyph, extents);
  return true;
}

static inline bool TableHasSpace(hb_face_t* face,
                                 hb_set_t* glyphs,
                                 hb_tag_t tag,
                                 hb_codepoint_t space) {
  unsigned count = hb_ot_layout_table_get_lookup_count(face, tag);
  for (unsigned i = 0; i < count; i++) {
    hb_ot_layout_lookup_collect_glyphs(face, tag, i, glyphs, glyphs, glyphs,
                                       nullptr);
    if (hb_set_has(glyphs, space)) {
      return true;
    }
  }
  return false;
}

static bool GetSpaceGlyph(hb_font_t* font, hb_codepoint_t& space) {
  return hb_font_get_nominal_glyph(font, kSpaceCharacter, &space);
}

bool HarfBuzzFace::HasSpaceInLigaturesOrKerning(TypesettingFeatures features) {
  const hb_codepoint_t kInvalidCodepoint = static_cast<hb_codepoint_t>(-1);
  hb_codepoint_t space = kInvalidCodepoint;

  hb::unique_ptr<hb_set_t> glyphs(hb_set_create());

  hb_font_t* unscaled_font = harfbuzz_font_data_->unscaled_font_.get();

  // Check whether computing is needed and compute for gpos/gsub.
  if (features & kKerning &&
      harfbuzz_font_data_->space_in_gpos_ ==
          HarfBuzzFontData::SpaceGlyphInOpenTypeTables::kUnknown) {
    if (space == kInvalidCodepoint && !GetSpaceGlyph(unscaled_font, space)) {
      return false;
    }
    // Compute for gpos.
    hb_face_t* face = hb_font_get_face(unscaled_font);
    DCHECK(face);
    harfbuzz_font_data_->space_in_gpos_ =
        hb_ot_layout_has_positioning(face) &&
                TableHasSpace(face, glyphs.get(), HB_OT_TAG_GPOS, space)
            ? HarfBuzzFontData::SpaceGlyphInOpenTypeTables::kPresent
            : HarfBuzzFontData::SpaceGlyphInOpenTypeTables::kNotPresent;
  }

  hb_set_clear(glyphs.get());

  if (features & kLigatures &&
      harfbuzz_font_data_->space_in_gsub_ ==
          HarfBuzzFontData::SpaceGlyphInOpenTypeTables::kUnknown) {
    if (space == kInvalidCodepoint && !GetSpaceGlyph(unscaled_font, space)) {
      return false;
    }
    // Compute for gpos.
    hb_face_t* face = hb_font_get_face(unscaled_font);
    DCHECK(face);
    harfbuzz_font_data_->space_in_gsub_ =
        hb_ot_layout_has_substitution(face) &&
                TableHasSpace(face, glyphs.get(), HB_OT_TAG_GSUB, space)
            ? HarfBuzzFontData::SpaceGlyphInOpenTypeTables::kPresent
            : HarfBuzzFontData::SpaceGlyphInOpenTypeTables::kNotPresent;
  }

  return (features & kKerning &&
          harfbuzz_font_data_->space_in_gpos_ ==
              HarfBuzzFontData::SpaceGlyphInOpenTypeTables::kPresent) ||
         (features & kLigatures &&
          harfbuzz_font_data_->space_in_gsub_ ==
              HarfBuzzFontData::SpaceGlyphInOpenTypeTables::kPresent);
}

unsigned HarfBuzzFace::UnitsPerEmFromHeadTable() {
  hb_face_t* face = hb_font_get_face(harfbuzz_font_data_->unscaled_font_.get());
  return hb_face_get_upem(face);
}

Glyph HarfBuzzFace::HbGlyphForCharacter(UChar32 character) {
  hb_codepoint_t glyph = 0;
  HarfBuzzGetNominalGlyph(harfbuzz_font_data_->unscaled_font_.get(),
                          harfbuzz_font_data_, character, &glyph, nullptr);
  return glyph;
}

hb_codepoint_t HarfBuzzFace::HarfBuzzGetGlyphForTesting(
    UChar32 character,
    UChar32 variation_selector) {
  hb_codepoint_t glyph = 0;
  HarfBuzzGetGlyph(harfbuzz_font_data_->unscaled_font_.get(),
                   harfbuzz_font_data_, character, variation_selector, &glyph,
                   nullptr);
  return glyph;
}

bool HarfBuzzFace::ShouldSubpixelPosition() {
  return harfbuzz_font_data_->font_.isSubpixel();
}

// `HarfBuzzSkiaFontFuncs` is shared hb_font_funcs_t`s among threads for
// calculating horizontal advances functions.
class HarfBuzzSkiaFontFuncs final {
 public:
  static HarfBuzzSkiaFontFuncs& Get() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(HarfBuzzSkiaFontFuncs, shared_hb_funcs, ());
    return shared_hb_funcs;
  }

#if BUILDFLAG(IS_APPLE)
  HarfBuzzSkiaFontFuncs()
      : hb_font_funcs_skia_advances_(
            CreateFontFunctions(kSkiaHorizontalAdvances)),
        hb_font_funcs_harfbuzz_advances_(
            CreateFontFunctions(kHarfBuzzHorizontalAdvances)) {}

  ~HarfBuzzSkiaFontFuncs() {
    hb_font_funcs_destroy(hb_font_funcs_skia_advances_);
    hb_font_funcs_destroy(hb_font_funcs_harfbuzz_advances_);
  }

  hb_font_funcs_t* GetFunctions(SkTypeface* typeface) {
    bool has_trak = false;
    bool has_sbix = false;

    const int num_tags = typeface->countTables();

    Vector<SkFontTableTag> tags(num_tags);

    const int returned_tags = typeface->getTableTags(tags.data());
    DCHECK_EQ(num_tags, returned_tags);

    for (auto& tag : tags) {
      if (tag == SkSetFourByteTag('t', 'r', 'a', 'k')) {
        has_trak = true;
      }
      if (tag == SkSetFourByteTag('s', 'b', 'i', 'x')) {
        has_sbix = true;
      }
    }

    return has_trak && !has_sbix ? hb_font_funcs_harfbuzz_advances_
                                 : hb_font_funcs_skia_advances_;
  }
#else
  HarfBuzzSkiaFontFuncs()
      : hb_font_funcs_skia_advances_(
            CreateFontFunctions(kSkiaHorizontalAdvances)) {}

  ~HarfBuzzSkiaFontFuncs() {
    hb_font_funcs_destroy(hb_font_funcs_skia_advances_);
  }

  hb_font_funcs_t* GetFunctions(SkTypeface*) {
    return hb_font_funcs_skia_advances_;
  }
#endif

  HarfBuzzSkiaFontFuncs(const HarfBuzzSkiaFontFuncs&) = delete;
  HarfBuzzSkiaFontFuncs(HarfBuzzSkiaFontFuncs&&) = delete;

  HarfBuzzSkiaFontFuncs& operator=(const HarfBuzzSkiaFontFuncs&) = delete;
  HarfBuzzSkiaFontFuncs& operator=(HarfBuzzSkiaFontFuncs&&) = delete;

 private:
  enum HorizontalAdvanceSource {
    kSkiaHorizontalAdvances,
#if BUILDFLAG(IS_APPLE)
    kHarfBuzzHorizontalAdvances,
#endif
  };

  static hb_font_funcs_t* CreateFontFunctions(
      HorizontalAdvanceSource advance_source) {
    hb_font_funcs_t* funcs = hb_font_funcs_create();

    if (advance_source == kSkiaHorizontalAdvances) {
      hb_font_funcs_set_glyph_h_advance_func(
          funcs, HarfBuzzGetGlyphHorizontalAdvance, nullptr, nullptr);
      hb_font_funcs_set_glyph_h_advances_func(
          funcs, HarfBuzzGetGlyphHorizontalAdvances, nullptr, nullptr);
    }
    hb_font_funcs_set_variation_glyph_func(funcs, HarfBuzzGetGlyph, nullptr,
                                           nullptr);
    hb_font_funcs_set_nominal_glyph_func(funcs, HarfBuzzGetNominalGlyph,
                                         nullptr, nullptr);
    // TODO(crbug.com/899718): Replace vertical metrics callbacks with
    // HarfBuzz VORG/VMTX internal implementation by deregistering those.
    hb_font_funcs_set_glyph_v_advance_func(
        funcs, HarfBuzzGetGlyphVerticalAdvance, nullptr, nullptr);
    hb_font_funcs_set_glyph_v_origin_func(funcs, HarfBuzzGetGlyphVerticalOrigin,
                                          nullptr, nullptr);
    hb_font_funcs_set_glyph_extents_func(funcs, HarfBuzzGetGlyphExtents,
                                         nullptr, nullptr);

    hb_font_funcs_make_immutable(funcs);
    return funcs;
  }

  hb_font_funcs_t* const hb_font_funcs_skia_advances_;
#if BUILDFLAG(IS_APPLE)
  hb_font_funcs_t* const hb_font_funcs_harfbuzz_advances_;
#endif
};

static hb_blob_t* HarfBuzzSkiaGetTable(hb_face_t* face,
                                       hb_tag_t tag,
                                       void* user_data) {
  SkTypeface* typeface = reinterpret_cast<SkTypeface*>(user_data);

  const wtf_size_t table_size =
      base::checked_cast<wtf_size_t>(typeface->getTableSize(tag));
  if (!table_size) {
    return nullptr;
  }

  char* buffer = reinterpret_cast<char*>(WTF::Partitions::FastMalloc(
      table_size, WTF_HEAP_PROFILER_TYPE_NAME(HarfBuzzFontData)));
  if (!buffer) {
    return nullptr;
  }
  size_t actual_size = typeface->getTableData(tag, 0, table_size, buffer);
  if (table_size != actual_size) {
    WTF::Partitions::FastFree(buffer);
    return nullptr;
  }
  return hb_blob_create(const_cast<char*>(buffer), table_size,
                        HB_MEMORY_MODE_WRITABLE, buffer,
                        WTF::Partitions::FastFree);
}

// TODO(yosin): We should move |CreateFace()| to "harfbuzz_font_cache.cc".
static hb::unique_ptr<hb_face_t> CreateFace(
    const FontPlatformData* platform_data) {
  hb::unique_ptr<hb_face_t> face;

  sk_sp<SkTypeface> typeface = sk_ref_sp(platform_data->Typeface());
  CHECK(typeface);
#if !BUILDFLAG(IS_APPLE)
  face = HbFaceFromSkTypeface(typeface);
#endif

  // Fallback to table copies if there is no in-memory access.
  if (!face) {
    face = hb::unique_ptr<hb_face_t>(hb_face_create_for_tables(
        HarfBuzzSkiaGetTable, typeface.get(), nullptr));
  }

  DCHECK(face);
  return face;
}

namespace {

HarfBuzzFontData* CreateHarfBuzzFontData(hb_face_t* face,
                                         SkTypeface* typeface) {
  hb::unique_ptr<hb_font_t> ot_font(hb_font_create(face));
  hb_ot_font_set_funcs(ot_font.get());

  int axis_count = typeface->getVariationDesignPosition(nullptr, 0);
  if (axis_count > 0) {
    Vector<SkFontArguments::VariationPosition::Coordinate> axis_values;
    axis_values.resize(axis_count);
    if (typeface->getVariationDesignPosition(axis_values.data(),
                                             axis_values.size()) > 0) {
      hb_font_set_variations(
          ot_font.get(), reinterpret_cast<hb_variation_t*>(axis_values.data()),
          axis_values.size());
    }
  }

  // Creating a sub font means that non-available functions
  // are found from the parent.
  hb_font_t* const unscaled_font = hb_font_create_sub_font(ot_font.get());
  HarfBuzzFontData* data =
      MakeGarbageCollected<HarfBuzzFontData>(unscaled_font);
  hb_font_set_funcs(unscaled_font,
                    HarfBuzzSkiaFontFuncs::Get().GetFunctions(typeface), data,
                    nullptr);
  return data;
}

}  // namespace

HarfBuzzFontData* HarfBuzzFontCache::GetOrCreate(
    uint64_t unique_id,
    const FontPlatformData* platform_data) {
  const auto& result = font_map_.insert(unique_id, nullptr);
  if (result.is_new_entry) {
    hb::unique_ptr<hb_face_t> face = CreateFace(platform_data);
    result.stored_value->value =
        CreateHarfBuzzFontData(face.get(), platform_data->Typeface());
  }
  return result.stored_value->value.Get();
}

static_assert(
    std::is_same<decltype(SkFontArguments::VariationPosition::Coordinate::axis),
                 decltype(hb_variation_t::tag)>::value &&
        std::is_same<
            decltype(SkFontArguments::VariationPosition::Coordinate::value),
            decltype(hb_variation_t::value)>::value &&
        sizeof(SkFontArguments::VariationPosition::Coordinate) ==
            sizeof(hb_variation_t),
    "Skia and HarfBuzz Variation parameter types must match in structure and "
    "size.");

const OpenTypeVerticalData& HarfBuzzFace::VerticalData() const {
  // Ensure `HarfBuzzFontData` and its `OpenTypeVerticalData` is up-to-date,
  // with `kPrepareForVerticalLayout`, even when this font isn't used for
  // vertical flow. See `GetScaledFont()`.
  harfbuzz_font_data_->UpdateFallbackMetricsAndScale(
      *platform_data_, HarfBuzzFace::kPrepareForVerticalLayout);
  return *harfbuzz_font_data_->VerticalData();
}

hb_font_t* HarfBuzzFace::GetScaledFont(const UnicodeRangeSet* range_set,
                                       VerticalLayoutCallbacks vertical_layout,
                                       float specified_size) const {
  harfbuzz_font_data_->range_set_ = range_set;
  harfbuzz_font_data_->UpdateFallbackMetricsAndScale(*platform_data_,
                                                     vertical_layout);

  int scale = SkiaScalarToHarfBuzzPosition(platform_data_->size());
  hb_font_t* unscaled_font = harfbuzz_font_data_->unscaled_font_.get();
  hb_font_set_scale(unscaled_font, scale, scale);
  // See contended discussion in https://github.com/harfbuzz/harfbuzz/pull/1484
  // Setting ptem here is critical for HarfBuzz to know where to lookup spacing
  // offset in the AAT trak table, the unit pt in ptem here means "CoreText"
  // points. After discussion on the pull request and with Apple developers, the
  // meaning of HarfBuzz' hb_font_set_ptem API was changed to expect the
  // equivalent of CSS pixels here.
  hb_font_set_ptem(unscaled_font, specified_size > 0 ? specified_size
                                                     : platform_data_->size());

  return unscaled_font;
}

hb_font_t* HarfBuzzFace::GetScaledFont() const {
  return GetScaledFont(nullptr, HarfBuzzFace::kNoVerticalLayout,
                       platform_data_->size());
}

void HarfBuzzFace::Init() {
  DCHECK(IsMainThread());
  HarfBuzzSkiaFontFuncs::Get();
}

}  // namespace blink
