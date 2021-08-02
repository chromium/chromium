// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_selector.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_list.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_map.h"
#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"

namespace blink {

AtomicString FontSelector::FamilyNameFromSettings(
    const GenericFontFamilySettings& settings,
    const FontDescription& font_description,
    const AtomicString& generic_family_name) {
#if defined(OS_ANDROID)
  if (font_description.GenericFamily() == FontDescription::kStandardFamily) {
    return FontCache::GetGenericFamilyNameForScript(
        font_family_names::kWebkitStandard, font_description);
  }

  if (generic_family_name.StartsWith("-webkit-")) {
    return FontCache::GetGenericFamilyNameForScript(generic_family_name,
                                                    font_description);
  }
#else   // !defined(OS_ANDROID)
  UScriptCode script = font_description.GetScript();
  if (font_description.GenericFamily() == FontDescription::kStandardFamily)
    return settings.Standard(script);
  if (generic_family_name == font_family_names::kWebkitSerif)
    return settings.Serif(script);
  if (generic_family_name == font_family_names::kWebkitSansSerif)
    return settings.SansSerif(script);
  if (generic_family_name == font_family_names::kWebkitCursive)
    return settings.Cursive(script);
  if (generic_family_name == font_family_names::kWebkitFantasy)
    return settings.Fantasy(script);
  if (generic_family_name == font_family_names::kWebkitMonospace)
    return settings.Fixed(script);
  if (generic_family_name == font_family_names::kWebkitPictograph)
    return settings.Pictograph(script);
  if (generic_family_name == font_family_names::kWebkitStandard)
    return settings.Standard(script);
#endif  // !defined(OS_ANDROID)
  return g_empty_atom;
}

void FontSelector::Trace(Visitor* visitor) const {
  visitor->Trace(font_fallback_map_);
  FontCacheClient::Trace(visitor);
}

FontFallbackMap& FontSelector::GetFontFallbackMap() {
  if (!font_fallback_map_) {
    font_fallback_map_ = MakeGarbageCollected<FontFallbackMap>(this);
    RegisterForInvalidationCallbacks(font_fallback_map_);
  }
  return *font_fallback_map_;
}

}  // namespace blink
