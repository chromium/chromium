/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/font_fallback_list.h"

#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/alternate_font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_family.h"
#include "third_party/blink/renderer/platform/fonts/segmented_font_data.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

FontFallbackList::FontFallbackList(FontSelector* font_selector)
    : cached_primary_simple_font_data_(nullptr),
      font_selector_(font_selector),
      font_selector_version_(font_selector ? font_selector->Version() : 0),
      family_index_(0),
      generation_(FontCache::GetFontCache()->Generation()),
      has_loading_fallback_(false),
      has_custom_font_(false),
      has_advance_override_(false),
      can_shape_word_by_word_(false),
      can_shape_word_by_word_computed_(false),
      is_invalid_(false) {}

void FontFallbackList::RevalidateDeprecated() {
  DCHECK(!RuntimeEnabledFeatures::
             CSSReducedFontLoadingLayoutInvalidationsEnabled());
  ReleaseFontData();
  font_list_.clear();
  cached_primary_simple_font_data_ = nullptr;
  family_index_ = 0;
  has_loading_fallback_ = false;
  has_custom_font_ = false;
  has_advance_override_ = false;
  can_shape_word_by_word_ = false;
  can_shape_word_by_word_computed_ = false;
  font_selector_version_ = font_selector_ ? font_selector_->Version() : 0;
  generation_ = FontCache::GetFontCache()->Generation();
}

void FontFallbackList::ReleaseFontData() {
  unsigned num_fonts = font_list_.size();
  for (unsigned i = 0; i < num_fonts; ++i) {
    if (!font_list_[i]->IsCustomFont()) {
      DCHECK(!font_list_[i]->IsSegmented());
      FontCache::GetFontCache()->ReleaseFontData(
          To<SimpleFontData>(font_list_[i].get()));
    }
  }
  shape_cache_.reset();  // Clear the weak pointer to the cache instance.
}

bool FontFallbackList::LoadingCustomFonts() const {
  // This function is only used for style and layout invalidation purposes. We
  // don't need it for invalidation when the feature below is enabled.
  // TODO(xiaochengh): Remove this function.
  return false;
}

bool FontFallbackList::ShouldSkipDrawing() const {
  // The DCHECK hit will be fixed by the runtime enabled feature below, so we
  // don't fix it in the legacy code paths.
  DCHECK(IsValid() || !RuntimeEnabledFeatures::
                          CSSReducedFontLoadingLayoutInvalidationsEnabled());

  if (!has_loading_fallback_)
    return false;

  unsigned num_fonts = font_list_.size();
  for (unsigned i = 0; i < num_fonts; ++i) {
    if (font_list_[i]->ShouldSkipDrawing())
      return true;
  }
  return false;
}

const SimpleFontData* FontFallbackList::DeterminePrimarySimpleFontData(
    const FontDescription& font_description) {
  bool should_load_custom_font = true;

  for (unsigned font_index = 0;; ++font_index) {
    const FontData* font_data = FontDataAt(font_description, font_index);
    if (!font_data) {
      // All fonts are custom fonts and are loading. Return the first FontData.
      font_data = FontDataAt(font_description, 0);
      if (font_data)
        return font_data->FontDataForCharacter(kSpaceCharacter);

      FontCache* cache = FontCache::GetFontCache();
      SimpleFontData* last_resort_fallback =
          cache->GetLastResortFallbackFont(font_description).get();
      DCHECK(last_resort_fallback);
      return last_resort_fallback;
    }

    const auto* segmented = DynamicTo<SegmentedFontData>(font_data);
    if (segmented && !segmented->ContainsCharacter(kSpaceCharacter))
      continue;

    const SimpleFontData* font_data_for_space =
        font_data->FontDataForCharacter(kSpaceCharacter);
    DCHECK(font_data_for_space);

    // When a custom font is loading, we should use the correct fallback font to
    // layout the text.  Here skip the temporary font for the loading custom
    // font which may not act as the correct fallback font.
    if (!font_data_for_space->IsLoadingFallback())
      return font_data_for_space;

    if (segmented) {
      for (unsigned i = 0; i < segmented->NumFaces(); i++) {
        const SimpleFontData* range_font_data =
            segmented->FaceAt(i)->FontData();
        if (!range_font_data->IsLoadingFallback())
          return range_font_data;
      }
      if (font_data->IsLoading())
        should_load_custom_font = false;
    }

    // Begin to load the first custom font if needed.
    if (should_load_custom_font) {
      should_load_custom_font = false;
      font_data_for_space->GetCustomFontData()->BeginLoadIfNeeded();
    }
  }
}

scoped_refptr<FontData> FontFallbackList::GetFontData(
    const FontDescription& font_description) {
  const FontFamily* curr_family = &font_description.Family();
  for (int i = 0; curr_family && i < family_index_; i++)
    curr_family = curr_family->Next();

  for (; curr_family; curr_family = curr_family->Next()) {
    family_index_++;
    if (curr_family->Family().length()) {
      scoped_refptr<FontData> result;
      if (font_selector_)
        result = font_selector_->GetFontData(font_description,
                                             curr_family->Family());

      if (!result) {
        result = FontCache::GetFontCache()->GetFontData(font_description,
                                                        curr_family->Family());
        if (font_selector_) {
          font_selector_->ReportFontLookupByUniqueOrFamilyName(
              curr_family->Family(), font_description,
              DynamicTo<SimpleFontData>(result.get()));
        }
      }
      if (result) {
        if (font_selector_) {
          font_selector_->ReportSuccessfulFontFamilyMatch(
              curr_family->Family());
        }
        return result;
      }

      if (font_selector_)
        font_selector_->ReportFailedFontFamilyMatch(curr_family->Family());
    }
  }
  family_index_ = kCAllFamiliesScanned;

  if (font_selector_) {
    // Try the user's preferred standard font.
    if (scoped_refptr<FontData> data = font_selector_->GetFontData(
            font_description, font_family_names::kWebkitStandard))
      return data;
  }

  // Still no result. Hand back our last resort fallback font.
  auto last_resort =
      FontCache::GetFontCache()->GetLastResortFallbackFont(font_description);
  if (font_selector_) {
    font_selector_->ReportLastResortFallbackFontLookup(font_description,
                                                       last_resort.get());
  }
  return last_resort;
}

FallbackListCompositeKey FontFallbackList::CompositeKey(
    const FontDescription& font_description) const {
  FallbackListCompositeKey key(font_description);
  const FontFamily* current_family = &font_description.Family();
  while (current_family) {
    if (current_family->Family().length()) {
      FontFaceCreationParams params(
          AdjustFamilyNameToAvoidUnsupportedFonts(current_family->Family()));
      scoped_refptr<FontData> result;
      if (font_selector_)
        result = font_selector_->GetFontData(font_description,
                                             current_family->Family());
      if (!result) {
        if (FontPlatformData* platform_data =
                FontCache::GetFontCache()->GetFontPlatformData(font_description,
                                                               params))
          result = FontCache::GetFontCache()->FontDataFromFontPlatformData(
              platform_data);
      }
      if (result) {
        bool is_unique_match = false;
        key.Add(font_description.CacheKey(params, is_unique_match));
        auto* font_data = DynamicTo<SimpleFontData>(result.get());
        if (!font_data && !result->IsCustomFont())
          FontCache::GetFontCache()->ReleaseFontData(font_data);
      }
    }
    current_family = current_family->Next();
  }

  return key;
}

const FontData* FontFallbackList::FontDataAt(
    const FontDescription& font_description,
    unsigned realized_font_index) {
  // This fallback font is already in our list.
  if (realized_font_index < font_list_.size())
    return font_list_[realized_font_index].get();

  // Make sure we're not passing in some crazy value here.
  DCHECK_EQ(realized_font_index, font_list_.size());

  if (family_index_ == kCAllFamiliesScanned)
    return nullptr;

  // Ask the font cache for the font data.
  // We are obtaining this font for the first time.  We keep track of the
  // families we've looked at before in |family_index_|, so that we never scan
  // the same spot in the list twice.  GetFontData will adjust our
  // |family_index_| as it scans for the right font to make.
  DCHECK_EQ(FontCache::GetFontCache()->Generation(), generation_);
  scoped_refptr<FontData> result = GetFontData(font_description);
  if (result) {
    font_list_.push_back(result);
    if (result->IsLoadingFallback())
      has_loading_fallback_ = true;
    if (result->IsCustomFont())
      has_custom_font_ = true;
    if (result->HasAdvanceOverride())
      has_advance_override_ = true;
  }
  return result.get();
}

bool FontFallbackList::ComputeCanShapeWordByWord(
    const FontDescription& font_description) {
  if (!font_description.GetTypesettingFeatures())
    return true;

  const SimpleFontData* primary_font = PrimarySimpleFontData(font_description);
  if (!primary_font)
    return false;

  const FontPlatformData& platform_data = primary_font->PlatformData();
  TypesettingFeatures features = font_description.GetTypesettingFeatures();
  return !platform_data.HasSpaceInLigaturesOrKerning(features);
}

bool FontFallbackList::CanShapeWordByWord(
    const FontDescription& font_description) {
  if (!can_shape_word_by_word_computed_) {
    can_shape_word_by_word_ = ComputeCanShapeWordByWord(font_description);
    can_shape_word_by_word_computed_ = true;
  }
  return can_shape_word_by_word_;
}

bool FontFallbackList::IsValid() const {
  if (RuntimeEnabledFeatures::
          CSSReducedFontLoadingLayoutInvalidationsEnabled()) {
    return !is_invalid_;
  }

  // The flag can be set only when the feature above is enabled.
  DCHECK(!is_invalid_);

  if (!font_selector_)
    return font_selector_version_ == 0;

  return font_selector_->Version() == font_selector_version_;
}

}  // namespace blink
