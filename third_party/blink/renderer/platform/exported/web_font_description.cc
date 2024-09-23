/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/public/platform/web_font_description.h"

#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

WebFontDescription::WebFontDescription(const FontDescription& desc) {
  family = desc.Family().FamilyName();
  family_is_generic = desc.Family().FamilyIsGeneric();
  generic_family = static_cast<GenericFamily>(desc.GenericFamily());
  size = desc.SpecifiedSize();
  italic = desc.Style() == kItalicSlopeValue;
  small_caps = desc.VariantCaps() == FontDescription::kSmallCaps;
  DCHECK(desc.Weight() >= 100 && desc.Weight() <= 900 &&
         static_cast<int>(desc.Weight()) % 100 == 0);
  weight = static_cast<Weight>(static_cast<int>(desc.Weight()) / 100 - 1);
  letter_spacing = desc.LetterSpacing();
  word_spacing = desc.WordSpacing();
}

WebFontDescription::operator FontDescription() const {
  FontDescription desc;
  desc.SetFamily(FontFamily(family, family_is_generic
                                        ? FontFamily::Type::kGenericFamily
                                        : FontFamily::Type::kFamilyName));
  desc.SetGenericFamily(
      static_cast<FontDescription::GenericFamilyType>(generic_family));
  desc.SetSpecifiedSize(size);
  desc.SetComputedSize(size);
  desc.SetStyle(italic ? kItalicSlopeValue : kNormalSlopeValue);
  desc.SetVariantCaps(small_caps ? FontDescription::kSmallCaps
                                 : FontDescription::kCapsNormal);
  static_assert(static_cast<int>(WebFontDescription::kWeight100) == 0,
                "kWeight100 conversion");
  static_assert(static_cast<int>(WebFontDescription::kWeight900) == 8,
                "kWeight900 conversion");
  desc.SetWeight(FontSelectionValue((weight + 1) * 100));
  desc.SetLetterSpacing(letter_spacing);
  desc.SetWordSpacing(word_spacing);
  return desc;
}

}  // namespace blink
