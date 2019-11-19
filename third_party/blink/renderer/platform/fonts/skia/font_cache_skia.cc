/*
 * Copyright (c) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#include <unicode/locid.h>

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/alternate_font_family.h"
#include "third_party/blink/renderer/platform/fonts/bitmap_glyphs_block_list.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/fonts/skia/sktypeface_factory.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

AtomicString ToAtomicString(const SkString& str) {
  return AtomicString::FromUTF8(str.c_str(), str.size());
}

#if defined(OS_ANDROID) || defined(OS_LINUX)
// This function is called on android or when we are emulating android fonts on
// linux and the embedder has overriden the default fontManager with
// WebFontRendering::setSkiaFontMgr.
// static
AtomicString FontCache::GetFamilyNameForCharacter(
    SkFontMgr* fm,
    UChar32 c,
    const FontDescription& font_description,
    FontFallbackPriority fallback_priority) {
  DCHECK(fm);

  Bcp47Vector locales =
      GetBcp47LocaleForRequest(font_description, fallback_priority);
  sk_sp<SkTypeface> typeface(fm->matchFamilyStyleCharacter(
      nullptr, SkFontStyle(), locales.data(), locales.size(), c));
  if (!typeface)
    return g_empty_atom;

  SkString skia_family_name;
  typeface->getFamilyName(&skia_family_name);
  return ToAtomicString(skia_family_name);
}
#endif  // defined(OS_ANDROID) || defined(OS_LINUX)

void FontCache::PlatformInit() {}

scoped_refptr<SimpleFontData> FontCache::FallbackOnStandardFontStyle(
    const FontDescription& font_description,
    UChar32 character) {
  FontDescription substitute_description(font_description);
  substitute_description.SetStyle(NormalSlopeValue());
  substitute_description.SetWeight(NormalWeightValue());

  FontFaceCreationParams creation_params(
      substitute_description.Family().Family());
  FontPlatformData* substitute_platform_data =
      GetFontPlatformData(substitute_description, creation_params);
  if (substitute_platform_data &&
      substitute_platform_data->FontContainsCharacter(character)) {
    FontPlatformData platform_data =
        FontPlatformData(*substitute_platform_data);
    platform_data.SetSyntheticBold(font_description.Weight() >=
                                   BoldThreshold());
    platform_data.SetSyntheticItalic(font_description.Style() ==
                                     ItalicSlopeValue());
    return FontDataFromFontPlatformData(&platform_data, kDoNotRetain);
  }

  return nullptr;
}

scoped_refptr<SimpleFontData> FontCache::GetLastResortFallbackFont(
    const FontDescription& description,
    ShouldRetain should_retain) {
  const FontFaceCreationParams fallback_creation_params(
      GetFallbackFontFamily(description));
  const FontPlatformData* font_platform_data = GetFontPlatformData(
      description, fallback_creation_params, AlternateFontName::kLastResort);

  // We should at least have Sans or Arial which is the last resort fallback of
  // SkFontHost ports.
  if (!font_platform_data) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontFaceCreationParams,
                                    sans_creation_params,
                                    (font_family_names::kSans));
    font_platform_data = GetFontPlatformData(description, sans_creation_params,
                                             AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontFaceCreationParams,
                                    arial_creation_params,
                                    (font_family_names::kArial));
    font_platform_data = GetFontPlatformData(description, arial_creation_params,
                                             AlternateFontName::kLastResort);
  }
#if defined(OS_WIN)
  // Try some more Windows-specific fallbacks.
  if (!font_platform_data) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontFaceCreationParams,
                                    msuigothic_creation_params,
                                    (font_family_names::kMSUIGothic));
    font_platform_data =
        GetFontPlatformData(description, msuigothic_creation_params,
                            AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontFaceCreationParams,
                                    mssansserif_creation_params,
                                    (font_family_names::kMicrosoftSansSerif));
    font_platform_data =
        GetFontPlatformData(description, mssansserif_creation_params,
                            AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontFaceCreationParams,
                                    segoeui_creation_params,
                                    (font_family_names::kSegoeUI));
    font_platform_data = GetFontPlatformData(
        description, segoeui_creation_params, AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontFaceCreationParams,
                                    calibri_creation_params,
                                    (font_family_names::kCalibri));
    font_platform_data = GetFontPlatformData(
        description, calibri_creation_params, AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontFaceCreationParams,
                                    timesnewroman_creation_params,
                                    (font_family_names::kTimesNewRoman));
    font_platform_data =
        GetFontPlatformData(description, timesnewroman_creation_params,
                            AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(const FontFaceCreationParams,
                                    couriernew_creation_params,
                                    (font_family_names::kCourierNew));
    font_platform_data =
        GetFontPlatformData(description, couriernew_creation_params,
                            AlternateFontName::kLastResort);
  }
#endif

  DCHECK(font_platform_data);
  return FontDataFromFontPlatformData(font_platform_data, should_retain);
}

sk_sp<SkTypeface> FontCache::CreateTypeface(
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    std::string& name) {
#if !defined(OS_WIN) && !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  // TODO(fuchsia): Revisit this and other font code for Fuchsia.

  if (creation_params.CreationType() == kCreateFontByFciIdAndTtcIndex) {
    if (Platform::Current()->GetSandboxSupport()) {
      return SkTypeface_Factory::FromFontConfigInterfaceIdAndTtcIndex(
          creation_params.FontconfigInterfaceId(), creation_params.TtcIndex());
    }
    return SkTypeface_Factory::FromFilenameAndTtcIndex(
        creation_params.Filename().data(), creation_params.TtcIndex());
  }
#endif

  AtomicString family = creation_params.Family();
  DCHECK_NE(family, font_family_names::kSystemUi);
  // If we're creating a fallback font (e.g. "-webkit-monospace"), convert the
  // name into the fallback name (like "monospace") that fontconfig understands.
  if (!family.length() || family.StartsWith("-webkit-")) {
    name = GetFallbackFontFamily(font_description).GetString().Utf8();
  } else {
    // convert the name to utf8
    name = family.Utf8();
  }

#if defined(OS_WIN)
  // TODO(vmpstr): Deal with paint typeface here.
  if (sideloaded_fonts_) {
    HashMap<String, sk_sp<SkTypeface>, CaseFoldingHash>::iterator
        sideloaded_font = sideloaded_fonts_->find(name.c_str());
    if (sideloaded_font != sideloaded_fonts_->end())
      return sideloaded_font->value;
  }
#endif

#if defined(OS_LINUX) || defined(OS_WIN)
  // On linux if the fontManager has been overridden then we should be calling
  // the embedder provided font Manager rather than calling
  // SkTypeface::CreateFromName which may redirect the call to the default font
  // Manager.  On Windows the font manager is always present.
  if (font_manager_) {
    auto tf = sk_sp<SkTypeface>(font_manager_->matchFamilyStyle(
        name.c_str(), font_description.SkiaFontStyle()));
    return tf;
  }
#endif

  // FIXME: Use m_fontManager, matchFamilyStyle instead of
  // legacyCreateTypeface on all platforms.
  return SkTypeface_Factory::FromFamilyNameAndFontStyle(
      name.c_str(), font_description.SkiaFontStyle());
}

#if !defined(OS_WIN)
std::unique_ptr<FontPlatformData> FontCache::CreateFontPlatformData(
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    float font_size,
    AlternateFontName alternate_name) {
  std::string name;

  sk_sp<SkTypeface> typeface;
#if defined(OS_ANDROID) || defined(OS_LINUX)
  if (alternate_name == AlternateFontName::kLocalUniqueFace &&
      RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled()) {
    typeface = CreateTypefaceFromUniqueName(creation_params);
  } else {
    typeface = CreateTypeface(font_description, creation_params, name);
  }
#else
  typeface = CreateTypeface(font_description, creation_params, name);
#endif

  if (!typeface)
    return nullptr;

  std::unique_ptr<FontPlatformData> font_platform_data =
      std::make_unique<FontPlatformData>(
          typeface, name, font_size,
          (font_description.Weight() >
               FontSelectionValue(200) +
                   FontSelectionValue(typeface->fontStyle().weight()) ||
           font_description.IsSyntheticBold()),
          ((font_description.Style() == ItalicSlopeValue()) &&
           !typeface->isItalic()) ||
              font_description.IsSyntheticItalic(),
          font_description.Orientation());

  font_platform_data->SetAvoidEmbeddedBitmaps(
      BitmapGlyphsBlockList::ShouldAvoidEmbeddedBitmapsForTypeface(*typeface));

  return font_platform_data;
}
#endif  // !defined(OS_WIN)

}  // namespace blink
