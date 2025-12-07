// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/mac/character_fallback_cache.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreText.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using base::apple::ScopedCFTypeRef;

namespace blink {

namespace {

String BuildIdentifierKey(CTFontRef ct_font) {
  if (!ct_font) {
    return String();
  }

  ScopedCFTypeRef<CFStringRef> ct_postscript_name(
      CTFontCopyPostScriptName(ct_font));

  if (!ct_postscript_name || !CFStringGetLength(ct_postscript_name.get())) {
    return String();
  }

  String postscript_name =
      String::FromUTF8(base::SysCFStringRefToUTF8(ct_postscript_name.get()));

  if (postscript_name[0] != '.') {
    // Not a system UI font.
    return postscript_name;
  } else {
    ScopedCFTypeRef<CTFontDescriptorRef> font_descriptor;
    font_descriptor.reset(CTFontCopyFontDescriptor(ct_font));

    if (!font_descriptor) {
      return String();
    }

    StringBuilder result_builder;

    ScopedCFTypeRef<CFDictionaryRef> attributes(
        CTFontDescriptorCopyAttributes(font_descriptor.get()));

    if (attributes) {
      // System fonts created with CTFontCreateUIFontForLanguage seem to expose
      // these two properties, which we can use to snapshot their fallback
      // behavior.
      CFStringRef ui_usage_value = (CFStringRef)CFDictionaryGetValue(
          attributes.get(),
          CFSTR("NSCTFontUIUsageAttribute"));  // Private key value
      if (ui_usage_value) {
        result_builder.Append("UIFONTUSAGE:");
        result_builder.Append(
            String::FromUTF8(base::SysCFStringRefToUTF8(ui_usage_value)));
      } else {
        return String();
      }

      CFStringRef language_value = (CFStringRef)CFDictionaryGetValue(
          attributes.get(), CFSTR("CTFontDescriptorLanguageAttribute"));
      if (language_value) {
        if (result_builder.length() > 0) {
          result_builder.Append("-");
        }
        result_builder.Append("LANG:");
        result_builder.Append(
            String::FromUTF8(base::SysCFStringRefToUTF8(language_value)));
      } else {
        return String();
      }
    }

    return result_builder.ToString();
  }
}
}  // namespace

std::optional<CharacterFallbackKey> CharacterFallbackKey::Make(
    CTFontRef ct_font,
    int16_t raw_font_weight,
    int16_t raw_font_style,
    uint8_t orientation,
    float font_size) {
  CharacterFallbackKey returnKey;

  returnKey.font_identifier = BuildIdentifierKey(ct_font);

  if (returnKey.font_identifier.empty()) {
    return std::nullopt;
  }

  returnKey.weight = raw_font_weight;
  returnKey.style = raw_font_style;
  returnKey.font_size = font_size;
  returnKey.orientation = orientation;
  return returnKey;
}

unsigned CharacterFallbackKeyHashTraits::GetHash(
    const CharacterFallbackKey& key) {
  unsigned hash = blink::GetHash(key.font_identifier);
  AddIntToHash(hash, HashInt(key.weight));
  AddIntToHash(hash, HashInt(key.style));
  AddIntToHash(hash, HashInt(key.orientation));
  AddIntToHash(hash, HashFloat(key.font_size));
  return hash;
}

}  // namespace blink
