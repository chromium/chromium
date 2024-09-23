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

#include "base/timer/elapsed_timer.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/alternate_font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_map.h"
#include "third_party/blink/renderer/platform/fonts/font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_performance.h"
#include "third_party/blink/renderer/platform/fonts/segmented_font_data.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

FontFallbackList::FontFallbackList(FontSelector* font_selector)
    : font_selector_(font_selector),
      generation_(FontCache::Get().Generation()),
      has_loading_fallback_(false),
      has_custom_font_(false),
      can_shape_word_by_word_(false),
      can_shape_word_by_word_computed_(false),
      is_invalid_(false),
      nullify_primary_font_data_for_test_(false) {}

void FontFallbackList::Trace(Visitor* visitor) const {
  visitor->Trace(font_list_);
  visitor->Trace(cached_primary_simple_font_data_);
  visitor->Trace(font_selector_);
  visitor->Trace(ng_shape_cache_);
  visitor->Trace(shape_cache_);
}

bool FontFallbackList::ShouldSkipDrawing() const {
  // The DCHECK hit will be fixed by the runtime enabled feature below, so we
  // don't fix it in the legacy code paths.
  DCHECK(IsValid());

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
  base::ElapsedTimer timer;
  const SimpleFontData* result =
      DeterminePrimarySimpleFontDataCore(font_description);
  FontPerformance::AddPrimaryFontTime(timer.Elapsed());
  return result;
}

const SimpleFontData* FontFallbackList::DeterminePrimarySimpleFontDataCore(
    const FontDescription& font_description) {
  bool should_load_custom_font = true;

  for (unsigned font_index = 0;; ++font_index) {
    const FontData* font_data = FontDataAt(font_description, font_index);
    if (!font_data) {
      // All fonts are custom fonts and are loading. Return the first FontData.
      font_data = FontDataAt(font_description, 0);
      if (font_data)
        return font_data->FontDataForCharacter(kSpaceCharacter);

      FontCache& font_cache = FontCache::Get();
      const SimpleFontData* last_resort_fallback =
          font_cache.GetLastResortFallbackFont(font_description);
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

const FontData* FontFallbackList::GetFontData(
    const FontDescription& font_description) {
  const FontFamily* curr_family = &font_description.Family();
  for (int i = 0; curr_family && i < family_index_; i++)
    curr_family = curr_family->Next();

  for (; curr_family; curr_family = curr_family->Next()) {
    family_index_++;
    if (!font_selector_) {
      // Don't query system fonts for empty font family name.
      if (!curr_family->FamilyName().empty()) {
        if (auto* result = FontCache::Get().GetFontData(
                font_description, curr_family->FamilyName())) {
          return result;
        }
      }
      continue;
    }

    const FontData* result =
        font_selector_->GetFontData(font_description, *curr_family);
    // Don't query system fonts for empty font family name.
    if (!result && !curr_family->FamilyName().empty()) {
      result = FontCache::Get().GetFontData(font_description,
                                            curr_family->FamilyName());
      font_selector_->ReportFontLookupByUniqueOrFamilyName(
          curr_family->FamilyName(), font_description,
          DynamicTo<SimpleFontData>(result));
    }
    if (result) {
      font_selector_->ReportSuccessfulFontFamilyMatch(
          curr_family->FamilyName());
      return result;
    }

    font_selector_->ReportFailedFontFamilyMatch(curr_family->FamilyName());
  }
  family_index_ = kCAllFamiliesScanned;

  if (font_selector_) {
    // Try the user's preferred standard font.
    FontFamily font_family(font_family_names::kWebkitStandard,
                           FontFamily::Type::kGenericFamily);
    if (const FontData* data =
            font_selector_->GetFontData(font_description, font_family)) {
      return data;
    }
  }

  // Still no result. Hand back our last resort fallback font.
  auto* last_resort =
      FontCache::Get().GetLastResortFallbackFont(font_description);
  if (font_selector_) {
    font_selector_->ReportLastResortFallbackFontLookup(font_description,
                                                       last_resort);
  }
  return last_resort;
}

const FontData* FontFallbackList::FontDataAt(
    const FontDescription& font_description,
    unsigned realized_font_index) {
  // This fallback font is already in our list.
  if (realized_font_index < font_list_.size())
    return font_list_[realized_font_index].Get();

  // Make sure we're not passing in some crazy value here.
  DCHECK_EQ(realized_font_index, font_list_.size());

  if (family_index_ == kCAllFamiliesScanned)
    return nullptr;

  // Ask the font cache for the font data.
  // We are obtaining this font for the first time.  We keep track of the
  // families we've looked at before in |family_index_|, so that we never scan
  // the same spot in the list twice.  GetFontData will adjust our
  // |family_index_| as it scans for the right font to make.
  DCHECK_EQ(FontCache::Get().Generation(), generation_);
  const FontData* result = GetFontData(font_description);
  if (result) {
    font_list_.push_back(result);
    if (result->IsLoadingFallback())
      has_loading_fallback_ = true;
    if (result->IsCustomFont())
      has_custom_font_ = true;
  }
  return result;
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

}  // namespace blink
