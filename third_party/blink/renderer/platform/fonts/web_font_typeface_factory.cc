// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/web_font_typeface_factory.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"
#include "third_party/freetype_buildflags.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkTypeface_fontations.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/renderer/platform/fonts/win/dwrite_font_format_support.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
#include "third_party/skia/include/ports/SkFontMgr_empty.h"
#endif


#include <functional>

namespace blink {

namespace {

bool IsWin() {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

bool IsApple() {
#if BUILDFLAG(IS_APPLE)
  return true;
#else
  return false;
#endif
}

bool IsFreeTypeSystemRasterizer() {
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_APPLE)
  return true;
#else
  return false;
#endif
}

sk_sp<SkTypeface> MakeTypefaceDefaultFontMgr(sk_sp<SkData> data) {
#if !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE))
  if (RuntimeEnabledFeatures::FontationsFontBackendEnabled()) {
    std::unique_ptr<SkStreamAsset> stream(new SkMemoryStream(data));
    return SkTypeface_Make_Fontations(std::move(stream), SkFontArguments());
  }
#endif

  sk_sp<SkFontMgr> font_manager;
#if BUILDFLAG(IS_WIN)
  font_manager = FontCache::Get().FontManager();
#else
  font_manager = skia::DefaultFontMgr();
#endif
  return font_manager->makeFromData(data, 0);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
sk_sp<SkTypeface> MakeTypefaceFallback(sk_sp<SkData> data) {
#if BUILDFLAG(ENABLE_FREETYPE)
  if (!RuntimeEnabledFeatures::FontationsFontBackendEnabled()) {
    return SkFontMgr_New_Custom_Empty()->makeFromData(data, 0);
  }
#endif
  std::unique_ptr<SkStreamAsset> stream(new SkMemoryStream(data));
  return SkTypeface_Make_Fontations(std::move(stream), SkFontArguments());
}
#endif

sk_sp<SkTypeface> MakeTypefaceFontations(sk_sp<SkData> data) {
  std::unique_ptr<SkStreamAsset> stream(new SkMemoryStream(data));
  return SkTypeface_Make_Fontations(std::move(stream), SkFontArguments());
}

sk_sp<SkTypeface> MakeVariationsTypeface(
    sk_sp<SkData> data,
    const WebFontTypefaceFactory::FontInstantiator& instantiator) {
#if BUILDFLAG(IS_WIN)
  if (DWriteVersionSupportsVariations()) {
    return instantiator.make_system(data);
  } else {
    return instantiator.make_fallback(data);
  }
#else
  return instantiator.make_system(data);
#endif
}

sk_sp<SkTypeface> MakeSbixTypeface(
    sk_sp<SkData> data,
    const WebFontTypefaceFactory::FontInstantiator& instantiator) {
  // If we're on a OS with FreeType as backend, or on Windows, where we used to
  // use FreeType for SBIX, switch to Fontations for SBIX.
  if ((IsFreeTypeSystemRasterizer() || IsWin()) &&
      (RuntimeEnabledFeatures::FontationsForSelectedFormatsEnabled() ||
       RuntimeEnabledFeatures::FontationsFontBackendEnabled())) {
    return instantiator.make_fontations(data);
  }
#if BUILDFLAG(IS_WIN)
  return instantiator.make_fallback(data);
#else
  // Remaining case, on Mac, CoreText can handle creating SBIX fonts.
  return instantiator.make_system(data);
#endif
}

sk_sp<SkTypeface> MakeColrV0Typeface(
    sk_sp<SkData> data,
    const WebFontTypefaceFactory::FontInstantiator& instantiator) {
  // On FreeType systems, move to Fontations for COLRv0.
  if ((IsApple() || IsFreeTypeSystemRasterizer()) &&
      (RuntimeEnabledFeatures::FontationsForSelectedFormatsEnabled() ||
       RuntimeEnabledFeatures::FontationsFontBackendEnabled())) {
    return instantiator.make_fontations(data);
  }

#if BUILDFLAG(IS_APPLE)
  return instantiator.make_fallback(data);
#else

  // Remaining cases, Fontations is off, then on Windows Skia's DirectWrite
  // backend handles COLRv0, on FreeType systems, FT handles COLRv0.
  return instantiator.make_system(data);
#endif
}

sk_sp<SkTypeface> MakeColrV0VariationsTypeface(
    sk_sp<SkData> data,
    const WebFontTypefaceFactory::FontInstantiator& instantiator) {
#if BUILDFLAG(IS_WIN)
  if (DWriteVersionSupportsVariations()) {
    return instantiator.make_system(data);
  }
#endif

  if ((RuntimeEnabledFeatures::FontationsForSelectedFormatsEnabled() ||
       RuntimeEnabledFeatures::FontationsFontBackendEnabled())) {
    return instantiator.make_fontations(data);
  } else {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
    return instantiator.make_fallback(data);
#else
    return instantiator.make_system(data);
#endif
  }
}

sk_sp<SkTypeface> MakeUseFallbackIfNeeded(
    sk_sp<SkData> data,
    const WebFontTypefaceFactory::FontInstantiator& instantiator) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
  return instantiator.make_fallback(data);
#else
  return instantiator.make_system(data);
#endif
}

sk_sp<SkTypeface> MakeFontationsFallbackPreferred(
    sk_sp<SkData> data,
    const WebFontTypefaceFactory::FontInstantiator& instantiator) {
  if (RuntimeEnabledFeatures::FontationsForSelectedFormatsEnabled() ||
      RuntimeEnabledFeatures::FontationsFontBackendEnabled()) {
    return instantiator.make_fontations(data);
  }
  return MakeUseFallbackIfNeeded(data, instantiator);
}

}  // namespace

bool WebFontTypefaceFactory::CreateTypeface(sk_sp<SkData> data,
                                            sk_sp<SkTypeface>& typeface) {
  const FontFormatCheck format_check(data);
  const FontInstantiator instantiator = {
      MakeTypefaceDefaultFontMgr,
      MakeTypefaceFontations,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
      MakeTypefaceFallback,
#endif
  };

  return CreateTypeface(data, typeface, format_check, instantiator);
}

bool WebFontTypefaceFactory::CreateTypeface(
    sk_sp<SkData> data,
    sk_sp<SkTypeface>& typeface,
    const FontFormatCheck& format_check,
    const FontInstantiator& instantiator) {
  CHECK(!typeface);

  if (!format_check.IsVariableFont() && !format_check.IsColorFont() &&
      !format_check.IsCff2OutlineFont()) {
    typeface = instantiator.make_system(data);
    if (typeface) {
      ReportInstantiationResult(
          InstantiationResult::kSuccessConventionalWebFont);
      return true;
    }
    // Not UMA reporting general decoding errors as these are already recorded
    // as kPackageFormatUnknown in FontResource.cpp.
    return false;
  }

  // The order of instantiation rules listed in this ruleset is important.
  // That's because variable COLRv0 fonts need to be special cased and go
  // through the fallback in order to avoid incompatibilities on Mac and Window.
  using CheckFunction = bool (FontFormatCheck::*)() const;
  using InstantionFunctionWithInstantiator = sk_sp<SkTypeface> (*)(
      sk_sp<SkData>, const FontInstantiator& instantiator);

  struct {
    CheckFunction check_function;
    InstantionFunctionWithInstantiator instantiation_function;
    std::optional<InstantiationResult> reportSuccess;
    std::optional<InstantiationResult> reportFailure;
  } instantiation_rules[] = {
      // We don't expect variable CBDT/CBLC or Sbix variable fonts for now.
      {&FontFormatCheck::IsCbdtCblcColorFont, &MakeFontationsFallbackPreferred,
       InstantiationResult::kSuccessCbdtCblcColorFont, std::nullopt},
      {&FontFormatCheck::IsColrCpalColorFontV1,
       &MakeFontationsFallbackPreferred,
       InstantiationResult::kSuccessColrV1Font, std::nullopt},
      {&FontFormatCheck::IsSbixColorFont, &MakeSbixTypeface,
       InstantiationResult::kSuccessSbixFont, std::nullopt},
      {&FontFormatCheck::IsCff2OutlineFont, &MakeFontationsFallbackPreferred,
       InstantiationResult::kSuccessCff2Font, std::nullopt},
      // We need to special case variable COLRv0 for backend instantiation as
      // certain Mac and Windows versions supported COLRv0 only without
      // variations.
      {&FontFormatCheck::IsVariableColrV0Font, &MakeColrV0VariationsTypeface,
       InstantiationResult::kSuccessColrCpalFont, std::nullopt},
      {&FontFormatCheck::IsVariableFont, &MakeVariationsTypeface,
       InstantiationResult::kSuccessVariableWebFont,
       InstantiationResult::kErrorInstantiatingVariableFont},
      {&FontFormatCheck::IsColrCpalColorFontV0, &MakeColrV0Typeface,
       InstantiationResult::kSuccessColrCpalFont, std::nullopt}};

  for (auto& rule : instantiation_rules) {
    if (std::invoke(rule.check_function, format_check)) {
      typeface = rule.instantiation_function(data, instantiator);
      if (typeface && rule.reportSuccess.has_value()) {
        ReportInstantiationResult(*rule.reportSuccess);
      } else if (!typeface && rule.reportFailure.has_value()) {
        ReportInstantiationResult(*rule.reportFailure);
      }
      return typeface.get();
    }
  }

  return false;
}

void WebFontTypefaceFactory::ReportInstantiationResult(
    InstantiationResult result) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Fonts.VariableFontsRatio", result);
}

}  // namespace blink
