/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "ui/gfx/font_fallback_linux.h"

namespace blink {

static AtomicString& MutableSystemFontFamily() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, system_font_family, ());
  return system_font_family;
}

// static
const AtomicString& FontCache::SystemFontFamily() {
  return MutableSystemFontFamily();
}

// static
void FontCache::SetSystemFontFamily(const AtomicString& family_name) {
  DCHECK(!family_name.empty());
  MutableSystemFontFamily() = family_name;
}

bool FontCache::GetFontForCharacter(UChar32 c,
                                    const char* preferred_locale,
                                    gfx::FallbackFontData* fallback_font) {
  if (Platform::Current()->GetSandboxSupport()) {
    return Platform::Current()
        ->GetSandboxSupport()
        ->GetFallbackFontForCharacter(c, preferred_locale, fallback_font);
  } else {
    std::string locale = preferred_locale ? preferred_locale : std::string();
    return gfx::GetFallbackFontForChar(c, locale, fallback_font);
  }
}

const SimpleFontData* FontCache::PlatformFallbackFontForCharacter(
    const FontDescription& font_description,
    UChar32 c,
    const SimpleFontData*,
    FontFallbackPriority fallback_priority) {
  // The m_fontManager is set only if it was provided by the embedder with
  // WebFontRendering::setSkiaFontManager. This is used to emulate android fonts
  // on linux so we always request the family from the font manager and if none
  // is found, we return the LastResort fallback font and avoid using
  // FontCache::GetFontForCharacter which would use sandbox support to query the
  // underlying system for the font family.
  if (font_manager_) {
    AtomicString family_name = GetFamilyNameForCharacter(
        font_manager_.get(), c, font_description, nullptr, fallback_priority);
    if (family_name.empty())
      return GetLastResortFallbackFont(font_description);
    return FontDataFromFontPlatformData(GetFontPlatformData(
        font_description, FontFaceCreationParams(family_name)));
  }

  if (IsEmojiPresentationEmoji(fallback_priority)) {
    // FIXME crbug.com/591346: We're overriding the fallback character here
    // with the FAMILY emoji in the hope to find a suitable emoji font.
    // This should be improved by supporting fallback for character
    // sequences like DIGIT ONE + COMBINING keycap etc.
    c = kFamilyCharacter;
  }

  // First try the specified font with standard style & weight.
  if (!IsEmojiPresentationEmoji(fallback_priority) &&
      (font_description.Style() == kItalicSlopeValue ||
       font_description.Weight() >= kBoldThreshold)) {
    const SimpleFontData* font_data =
        FallbackOnStandardFontStyle(font_description, c);
    if (font_data)
      return font_data;
  }

  gfx::FallbackFontData fallback_font;
  if (!FontCache::GetFontForCharacter(
          c,
          IsEmojiPresentationEmoji(fallback_priority)
              ? kColorEmojiLocale
              : font_description.LocaleOrDefault().Ascii().c_str(),
          &fallback_font)) {
    return nullptr;
  }

  FontFaceCreationParams creation_params;
  creation_params = FontFaceCreationParams(
      fallback_font.filepath.value(), fallback_font.fontconfig_interface_id,
      fallback_font.ttc_index);

  // Changes weight and/or italic of given FontDescription depends on
  // the result of fontconfig so that keeping the correct font mapping
  // of the given character. See http://crbug.com/32109 for details.
  bool should_set_synthetic_bold = false;
  bool should_set_synthetic_italic = false;
  FontDescription description(font_description);
  if (fallback_font.is_bold && description.Weight() < kBoldThreshold) {
    description.SetWeight(kBoldWeightValue);
  }
  if (!fallback_font.is_bold && description.Weight() >= kBoldThreshold &&
      font_description.SyntheticBoldAllowed()) {
    should_set_synthetic_bold = true;
    description.SetWeight(kNormalWeightValue);
  }
  if (fallback_font.is_italic && description.Style() == kNormalSlopeValue) {
    description.SetStyle(kItalicSlopeValue);
  }
  if (!fallback_font.is_italic && (description.Style() == kItalicSlopeValue) &&
      font_description.SyntheticItalicAllowed()) {
    should_set_synthetic_italic = true;
    description.SetStyle(kNormalSlopeValue);
  }

  const FontPlatformData* substitute_platform_data =
      GetFontPlatformData(description, creation_params);
  if (!substitute_platform_data)
    return nullptr;

  FontPlatformData* platform_data =
      MakeGarbageCollected<FontPlatformData>(*substitute_platform_data);
  platform_data->SetSyntheticBold(should_set_synthetic_bold);
  platform_data->SetSyntheticItalic(should_set_synthetic_italic);
  return FontDataFromFontPlatformData(platform_data);
}

}  // namespace blink
