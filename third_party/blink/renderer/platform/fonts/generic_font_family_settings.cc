/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"

#include <memory>

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

namespace blink {

// A holdback feature to measure the performance impact.
BASE_FEATURE(kGenericFontSettingCache,
             "GenericFontSettingCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

GenericFontFamilySettings::GenericFontFamilySettings(
    const GenericFontFamilySettings& other)
    : standard_font_family_map_(other.standard_font_family_map_),
      serif_font_family_map_(other.serif_font_family_map_),
      fixed_font_family_map_(other.fixed_font_family_map_),
      sans_serif_font_family_map_(other.sans_serif_font_family_map_),
      cursive_font_family_map_(other.cursive_font_family_map_),
      fantasy_font_family_map_(other.fantasy_font_family_map_),
      math_font_family_map_(other.math_font_family_map_),
      first_available_font_for_families_(other.first_available_font_for_families_) {}

GenericFontFamilySettings& GenericFontFamilySettings::operator=(
    const GenericFontFamilySettings& other) {
  standard_font_family_map_ = other.standard_font_family_map_;
  serif_font_family_map_ = other.serif_font_family_map_;
  fixed_font_family_map_ = other.fixed_font_family_map_;
  sans_serif_font_family_map_ = other.sans_serif_font_family_map_;
  cursive_font_family_map_ = other.cursive_font_family_map_;
  fantasy_font_family_map_ = other.fantasy_font_family_map_;
  math_font_family_map_ = other.math_font_family_map_;
  first_available_font_for_families_ = other.first_available_font_for_families_;
  return *this;
}

// Sets the entry in the font map for the given script. If family is the empty
// string, removes the entry instead.
void GenericFontFamilySettings::SetGenericFontFamilyMap(
    ScriptFontFamilyMap& font_map,
    const AtomicString& family,
    UScriptCode script) {
  ScriptFontFamilyMap::iterator it = font_map.find(static_cast<int>(script));
  if (family.empty()) {
    if (it == font_map.end())
      return;
    font_map.erase(it);
  } else if (it != font_map.end() && it->value == family) {
    return;
  } else {
    font_map.Set(static_cast<int>(script), family);
  }
}

const AtomicString& GenericFontFamilySettings::GenericFontFamilyForScript(
    const ScriptFontFamilyMap& font_map,
    UScriptCode script) const {
  ScriptFontFamilyMap::iterator it =
      const_cast<ScriptFontFamilyMap&>(font_map).find(static_cast<int>(script));
  if (it != font_map.end()) {
    // If it is not a list, just return it.
    if (it->value.empty() || it->value[0] != ',') {
      return it->value;
    }
    if (!base::FeatureList().IsEnabled(kGenericFontSettingCache)) {
      // Replace with the first available font if it starts with ",".
      it->value = AtomicString(FontCache::FirstAvailableOrFirst(it->value));
      return it->value;
    }

    if (auto font_cache_it = first_available_font_for_families_.find(it->value);
        font_cache_it != first_available_font_for_families_.end()) {
      // If another script has already used the font and cached the result,
      // just use the cached data.
      it->value = font_cache_it->value;
    } else {
      // Add the result to cache.
      AtomicString first_available_font =
          AtomicString(FontCache::FirstAvailableOrFirst(it->value));
      first_available_font_for_families_.Set(it->value, first_available_font);
      it->value = first_available_font;
    }
    return it->value;
  }
  if (script != USCRIPT_COMMON)
    return GenericFontFamilyForScript(font_map, USCRIPT_COMMON);
  return g_empty_atom;
}

bool GenericFontFamilySettings::ShouldUpdateFontFamily(
    const AtomicString& old_first_available_family,
    const AtomicString& new_family) const {
  // If the two font families are already the same.
  if (new_family == old_first_available_family) {
    return false;
  }
  // If the feature is disabled, it does not use the cache and just say the two
  // settings are different.
  if (!base::FeatureList().IsEnabled(kGenericFontSettingCache)) {
    return true;
  }
  // Then if the new family is not a list, this should update the setting.
  if (new_family.empty() || new_family[0] != ',') {
    return true;
  }

  // If the list of new specified families' first available font has already
  // been cached and it is the same as 'old_first_available_family`, we do not
  // need ot update font setting.
  if (auto it = first_available_font_for_families_.find(new_family);
      it != first_available_font_for_families_.end()) {
    return it->value != old_first_available_family;
  }
  return true;
}

const AtomicString& GenericFontFamilySettings::Standard(
    UScriptCode script) const {
  return GenericFontFamilyForScript(standard_font_family_map_, script);
}

bool GenericFontFamilySettings::UpdateStandard(const AtomicString& family,
                                               UScriptCode script) {
  auto& old_family = base::FeatureList().IsEnabled(kGenericFontSettingCache)
                         ? Standard(script)
                         : Standard();
  if (!ShouldUpdateFontFamily(old_family, family)) {
    return false;
  }
  SetGenericFontFamilyMap(standard_font_family_map_, family, script);
  return true;
}

const AtomicString& GenericFontFamilySettings::Fixed(UScriptCode script) const {
  const AtomicString& fixed_font =
      GenericFontFamilyForScript(fixed_font_family_map_, script);
#if BUILDFLAG(IS_MAC)
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, kOsaka, ("Osaka"));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, kOsakaMono, ("Osaka-Mono"));
  if (fixed_font == kOsaka) {
    return kOsakaMono;
  }
#endif
  return fixed_font;
}

bool GenericFontFamilySettings::UpdateFixed(const AtomicString& family,
                                            UScriptCode script) {
  const AtomicString& old_family =
      base::FeatureList().IsEnabled(kGenericFontSettingCache) ? Fixed(script)
                                                              : Fixed();
  if (!ShouldUpdateFontFamily(old_family, family)) {
    return false;
  }
  SetGenericFontFamilyMap(fixed_font_family_map_, family, script);
  return true;
}

const AtomicString& GenericFontFamilySettings::Serif(UScriptCode script) const {
  return GenericFontFamilyForScript(serif_font_family_map_, script);
}

bool GenericFontFamilySettings::UpdateSerif(const AtomicString& family,
                                            UScriptCode script) {
  const AtomicString& old_family =
      base::FeatureList().IsEnabled(kGenericFontSettingCache) ? Serif(script)
                                                              : Serif();
  if (!ShouldUpdateFontFamily(old_family, family)) {
    return false;
  }
  SetGenericFontFamilyMap(serif_font_family_map_, family, script);
  return true;
}

const AtomicString& GenericFontFamilySettings::SansSerif(
    UScriptCode script) const {
  return GenericFontFamilyForScript(sans_serif_font_family_map_, script);
}

bool GenericFontFamilySettings::UpdateSansSerif(const AtomicString& family,
                                                UScriptCode script) {
  const AtomicString& old_family =
      base::FeatureList().IsEnabled(kGenericFontSettingCache)
          ? SansSerif(script)
          : SansSerif();
  if (!ShouldUpdateFontFamily(old_family, family)) {
    return false;
  }
  SetGenericFontFamilyMap(sans_serif_font_family_map_, family, script);
  return true;
}

const AtomicString& GenericFontFamilySettings::Cursive(
    UScriptCode script) const {
  return GenericFontFamilyForScript(cursive_font_family_map_, script);
}

bool GenericFontFamilySettings::UpdateCursive(const AtomicString& family,
                                              UScriptCode script) {
  const AtomicString& old_family =
      base::FeatureList().IsEnabled(kGenericFontSettingCache) ? Cursive(script)
                                                              : Cursive();
  if (!ShouldUpdateFontFamily(old_family, family)) {
    return false;
  }
  SetGenericFontFamilyMap(cursive_font_family_map_, family, script);
  return true;
}

const AtomicString& GenericFontFamilySettings::Fantasy(
    UScriptCode script) const {
  return GenericFontFamilyForScript(fantasy_font_family_map_, script);
}

bool GenericFontFamilySettings::UpdateFantasy(const AtomicString& family,
                                              UScriptCode script) {
  const AtomicString& old_family =
      base::FeatureList().IsEnabled(kGenericFontSettingCache) ? Fantasy(script)
                                                              : Fantasy();
  if (!ShouldUpdateFontFamily(old_family, family)) {
    return false;
  }
  SetGenericFontFamilyMap(fantasy_font_family_map_, family, script);
  return true;
}

const AtomicString& GenericFontFamilySettings::Math(UScriptCode script) const {
  return GenericFontFamilyForScript(math_font_family_map_, script);
}

bool GenericFontFamilySettings::UpdateMath(const AtomicString& family,
                                           UScriptCode script) {
  const AtomicString& old_family =
      base::FeatureList().IsEnabled(kGenericFontSettingCache) ? Math(script)
                                                              : Math();
  if (!ShouldUpdateFontFamily(old_family, family)) {
    return false;
  }
  SetGenericFontFamilyMap(math_font_family_map_, family, script);
  return true;
}

void GenericFontFamilySettings::Reset() {
  standard_font_family_map_.clear();
  serif_font_family_map_.clear();
  fixed_font_family_map_.clear();
  sans_serif_font_family_map_.clear();
  cursive_font_family_map_.clear();
  fantasy_font_family_map_.clear();
  math_font_family_map_.clear();
  first_available_font_for_families_.clear();
}

}  // namespace blink
