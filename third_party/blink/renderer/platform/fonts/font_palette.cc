// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_palette.h"

#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/skia/include/core/SkFontArguments.h"

namespace blink {

static_assert(sizeof(FontPalette::FontPaletteOverride) ==
                      sizeof(SkFontArguments::Palette::Override) &&
                  sizeof(FontPalette::FontPaletteOverride::index) ==
                      sizeof(SkFontArguments::Palette::Override::index) &&
                  sizeof(FontPalette::FontPaletteOverride::color) ==
                      sizeof(SkFontArguments::Palette::Override::color) &&
                  offsetof(FontPalette::FontPaletteOverride, index) ==
                      offsetof(SkFontArguments::Palette::Override, index) &&
                  offsetof(FontPalette::FontPaletteOverride, color) ==
                      offsetof(SkFontArguments::Palette::Override, color),
              "Struct FontPalette::FontPaletteOverride must match "
              "SkFontArguments::Palette::Override.");

unsigned FontPalette::GetHash() const {
  unsigned computed_hash = 0;
  WTF::AddIntToHash(computed_hash, palette_keyword_);

  if (palette_keyword_ != kCustomPalette)
    return computed_hash;

  WTF::AddIntToHash(computed_hash,
                    AtomicStringHash::GetHash(palette_values_name_));
  WTF::AddIntToHash(computed_hash,
                    match_font_family_.empty()
                        ? 0
                        : AtomicStringHash::GetHash(match_font_family_));
  WTF::AddIntToHash(computed_hash, base_palette_.type);
  WTF::AddIntToHash(computed_hash, base_palette_.index);

  for (auto& override_entry : palette_overrides_) {
    WTF::AddIntToHash(computed_hash, override_entry.index);
  }
  return computed_hash;
}

bool FontPalette::operator==(const FontPalette& other) const {
  return palette_keyword_ == other.palette_keyword_ &&
         palette_values_name_ == other.palette_values_name_ &&
         match_font_family_ == other.match_font_family_ &&
         base_palette_ == other.base_palette_ &&
         palette_overrides_ == other.palette_overrides_;
}

}  // namespace blink
