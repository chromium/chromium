/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
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

#include "third_party/blink/renderer/platform/fonts/font_platform_data_cache.h"

#include <algorithm>
#include <cmath>
#include "base/feature_list.h"
#include "third_party/blink/renderer/platform/fonts/alternate_font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {
BASE_FEATURE(kFontCacheNoSizeInKey,
             "FontCacheNoSizeInKey",
             base::FEATURE_DISABLED_BY_DEFAULT);
}

// static
std::unique_ptr<FontPlatformDataCache> FontPlatformDataCache::Create() {
  return std::make_unique<FontPlatformDataCache>();
}

FontPlatformDataCache::FontPlatformDataCache()
    : font_size_limit_(std::nextafter(
          (static_cast<float>(std::numeric_limits<unsigned>::max()) - 2.f) /
              static_cast<float>(blink::FontCacheKey::PrecisionMultiplier()),
          0.f)),
      no_size_in_key_(base::FeatureList::IsEnabled(kFontCacheNoSizeInKey)) {}

FontPlatformDataCache::~FontPlatformDataCache() = default;

FontPlatformData* FontPlatformDataCache::GetOrCreateFontPlatformData(
    FontCache* font_cache,
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    AlternateFontName alternate_font_name) {
  const bool is_unique_match =
      alternate_font_name == AlternateFontName::kLocalUniqueFace;
  FontCacheKey key =
      font_description.CacheKey(creation_params, is_unique_match);
  DCHECK(!key.IsHashTableDeletedValue());

  if (no_size_in_key_) {
    // Clear font size from they key. Size is not required in the primary key
    // because per-size FontPlatformData are held in a nested map.
    key.ClearFontSize();
  }

  const float size =
      std::min(font_description.EffectiveFontSize(), font_size_limit_);

  const unsigned rounded_size = size * FontCacheKey::PrecisionMultiplier();

  // Assert that the computed hash map key rounded_size value does not hit
  // the empty (max()) or deleted (max()-1) sentinel values of the hash map,
  // compare UnsignedWithZeroKeyHashTraits() in hash_traits.h.
  DCHECK_LT(rounded_size, std::numeric_limits<unsigned>::max() - 1);

  // Assert that rounded_size was not reset to 0 due to an integer overflow,
  // i.e. if size was non-zero, rounded_size can't be zero, but if size was 0,
  // it may be 0.
  DCHECK_EQ(!!size, !!rounded_size);

  // Remove the font size from the cache key, and handle the font size
  // separately in the inner map. So that different size of `FontPlatformData`
  // can share underlying SkTypeface.
  SizedFontPlatformDataSet& sized_fonts = GetOrCreateSizeMap(key);

  if (auto* result = sized_fonts.GetOrCreateFontPlatformData(
          font_cache, font_description, creation_params, size,
          alternate_font_name, rounded_size))
    return result;

  if (alternate_font_name != AlternateFontName::kAllowAlternate ||
      creation_params.CreationType() != kCreateFontByFamily)
    return nullptr;

  // We were unable to find a font. We have a small set of fonts that we alias
  // to other names, e.g., Arial/Helvetica, Courier/Courier New, etc. Try
  // looking up the font under the aliased name.
  const AtomicString& alternate_name =
      AlternateFamilyName(creation_params.Family());
  if (alternate_name.empty())
    return nullptr;

  FontFaceCreationParams create_by_alternate_family(alternate_name);
  FontPlatformData* const platform_data = GetOrCreateFontPlatformData(
      font_cache, font_description, create_by_alternate_family,
      AlternateFontName::kNoAlternate);
  if (!platform_data)
    return nullptr;

  // "accessibility/font-changed.html" reaches here.

  // Cache the platform_data under the old name.
  sized_fonts.Set(rounded_size, platform_data);
  return platform_data;
}

size_t FontPlatformDataCache::ByteSize() const {
  return map_.size() * sizeof(SizedFontPlatformDataSet);
}

void FontPlatformDataCache::Clear() {
  map_.clear();
}

void FontPlatformDataCache::Purge(const FontDataCache& font_data_cache) {
  Vector<FontCacheKey> keys_to_remove;
  keys_to_remove.ReserveInitialCapacity(map_.size());
  for (auto& entry : map_) {
    if (entry.value->Purge(font_data_cache))
      keys_to_remove.push_back(entry.key);
  }
  map_.RemoveAll(keys_to_remove);
}

FontPlatformDataCache::SizedFontPlatformDataSet&
FontPlatformDataCache::GetOrCreateSizeMap(const FontCacheKey& key) {
  auto result = map_.insert(key, nullptr);
  if (result.is_new_entry)
    result.stored_value->value = SizedFontPlatformDataSet::Create();
  return *result.stored_value->value;
}

// --

// static
scoped_refptr<FontPlatformDataCache::SizedFontPlatformDataSet>
FontPlatformDataCache::SizedFontPlatformDataSet::Create() {
  return base::AdoptRef(new SizedFontPlatformDataSet());
}

FontPlatformDataCache::SizedFontPlatformDataSet::SizedFontPlatformDataSet() =
    default;

FontPlatformDataCache::SizedFontPlatformDataSet::~SizedFontPlatformDataSet() =
    default;

FontPlatformData*
FontPlatformDataCache::SizedFontPlatformDataSet::GetOrCreateFontPlatformData(
    FontCache* font_cache,
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    float size,
    AlternateFontName alternate_font_name,
    unsigned rounded_size) {
  // Take a different size instance of the same font before adding an entry to
  // `size_to_data_map`.
  FontPlatformData* const another_size =
      size_to_data_map_.empty() ? nullptr
                                : size_to_data_map_.begin()->value.get();
  const auto add_result = size_to_data_map_.insert(rounded_size, nullptr);
  std::unique_ptr<FontPlatformData>* found = &add_result.stored_value->value;
  if (!add_result.is_new_entry)
    return found->get();

  if (!another_size) {
    *found = font_cache->CreateFontPlatformData(
        font_description, creation_params, size, alternate_font_name);
    return found->get();
  }

  *found = font_cache->ScaleFontPlatformData(*another_size, font_description,
                                             creation_params, size);
  return found->get();
}

bool FontPlatformDataCache::SizedFontPlatformDataSet::Purge(
    const FontDataCache& font_data_cache) {
  Vector<unsigned> sizes_to_remove;
  sizes_to_remove.ReserveInitialCapacity(size_to_data_map_.size());
  for (const auto& entry : size_to_data_map_) {
    if (entry.value && !font_data_cache.Contains(entry.value.get()))
      sizes_to_remove.push_back(entry.key);
  }
  size_to_data_map_.RemoveAll(sizes_to_remove);
  return size_to_data_map_.empty();
}

void FontPlatformDataCache::SizedFontPlatformDataSet::Set(
    unsigned rounded_size,
    FontPlatformData* platform_data) {
  size_to_data_map_.insert(rounded_size,
                           std::make_unique<FontPlatformData>(*platform_data));
}

}  // namespace blink
