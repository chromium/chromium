// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/mac/character_fallback_cache.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreText.h>

#include <array>

#include "base/apple/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

using base::apple::ScopedCFTypeRef;

namespace blink {

namespace {

String BuildIdentifierKeyUncached(CTFontRef ct_font) {
  ScopedCFTypeRef<CFStringRef> ct_postscript_name(
      CTFontCopyPostScriptName(ct_font));

  if (!ct_postscript_name || !CFStringGetLength(ct_postscript_name.get())) {
    return String();
  }

  if (CFStringGetCharacterAtIndex(ct_postscript_name.get(), 0) != '.') {
    // Not a system UI font.
    return String::FromUtf8(
        base::SysCFStringRefToUTF8(ct_postscript_name.get()));
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
            String::FromUtf8(base::SysCFStringRefToUTF8(ui_usage_value)));
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
            String::FromUtf8(base::SysCFStringRefToUTF8(language_value)));
      } else {
        return String();
      }
    }

    return result_builder.ToString();
  }
}

String BuildIdentifierKey(CTFontRef ct_font) {
  if (!ct_font) {
    return String();
  }

  // Retain `font` so that a deallocated CTFontRef cannot have its address
  // reused by a newly allocated CTFontRef (ABA problem).
  struct CachedFontIdentifier {
    ScopedCFTypeRef<CTFontRef> font;
    String identifier;
  };
  struct CachedFontIdentifierCache {
    std::array<CachedFontIdentifier, 4> entries;
  };
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<CachedFontIdentifierCache>,
                                  cache_tls, ());
  auto& entries = cache_tls->entries;
  for (const auto& entry : entries) {
    if (entry.font.get() == ct_font) {
      return entry.identifier;
    }
  }

  String identifier = BuildIdentifierKeyUncached(ct_font);
  if (!identifier.empty()) {
    identifier.Impl()->GetHash();
    for (size_t i = entries.size() - 1; i > 0; --i) {
      entries[i] = std::move(entries[i - 1]);
    }
    entries[0].font.reset(ct_font, base::scoped_policy::RETAIN);
    entries[0].identifier = identifier;
  }
  return identifier;
}
}  // namespace

std::optional<CharacterFallbackKey> CharacterFallbackKey::Make(
    CTFontRef ct_font,
    int16_t raw_font_weight,
    int16_t raw_font_style,
    uint8_t orientation,
    float font_size,
    UChar32 character,
    uint8_t fallback_flags) {
  CharacterFallbackKey returnKey;

  returnKey.font_identifier = BuildIdentifierKey(ct_font);

  if (returnKey.font_identifier.empty()) {
    return std::nullopt;
  }

  returnKey.weight = raw_font_weight;
  returnKey.style = raw_font_style;
  returnKey.font_size = font_size;
  returnKey.orientation = orientation;
  returnKey.character = character;
  returnKey.fallback_flags = fallback_flags;
  return returnKey;
}

uint32_t CharacterFallbackKeyHashTraits::GetHash(
    const CharacterFallbackKey& key) {
  uint32_t hash = blink::GetHash(key.font_identifier);
  AddIntToHash(hash, HashInt(key.weight));
  AddIntToHash(hash, HashInt(key.style));
  AddIntToHash(hash, HashInt(key.orientation));
  AddIntToHash(hash, HashFloat(key.font_size));
  if (key.character || key.fallback_flags) {
    AddIntToHash(hash, HashInt(key.character));
    AddIntToHash(hash, HashInt(key.fallback_flags));
  }
  return hash;
}

}  // namespace blink
