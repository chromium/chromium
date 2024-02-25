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

FontPlatformDataCache::FontPlatformDataCache()
    : font_size_limit_(std::nextafter(
          (static_cast<float>(std::numeric_limits<unsigned>::max()) - 2.f) /
              static_cast<float>(blink::FontCacheKey::PrecisionMultiplier()),
          0.f)) {}

const FontPlatformData* FontPlatformDataCache::GetOrCreateFontPlatformData(
    FontCache* font_cache,
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    AlternateFontName alternate_font_name) {
  const bool is_unique_match =
      alternate_font_name == AlternateFontName::kLocalUniqueFace;
  FontCacheKey key =
      font_description.CacheKey(creation_params, is_unique_match);
  DCHECK(!key.IsHashTableDeletedValue());

  const float size =
      std::min(font_description.EffectiveFontSize(), font_size_limit_);

  auto it = map_.find(key);
  if (it != map_.end()) {
    return it->value.Get();
  }

  if (const FontPlatformData* result = font_cache->CreateFontPlatformData(
          font_description, creation_params, size, alternate_font_name)) {
    map_.insert(key, result);
    return result;
  }

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
  if (const FontPlatformData* result = GetOrCreateFontPlatformData(
          font_cache, font_description, create_by_alternate_family,
          AlternateFontName::kNoAlternate)) {
    // Cache the platform_data under the old name.
    // "accessibility/font-changed.html" reaches here.
    map_.insert(key, result);
    return result;
  }

  return nullptr;
}

}  // namespace blink
