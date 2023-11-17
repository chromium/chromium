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
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/renderer/platform/fonts/win/dwrite_font_format_support.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
#include "third_party/skia/include/ports/SkFontMgr_empty.h"
#endif

#if BUILDFLAG(USE_FONTATIONS_BACKEND)
#include "third_party/skia/include/ports/SkTypeface_fontations.h"
#endif

#include <functional>

namespace blink {

namespace {

sk_sp<SkTypeface> MakeTypefaceDefaultFontMgr(sk_sp<SkData> data) {
#if BUILDFLAG(USE_FONTATIONS_BACKEND) && \
    !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE))
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
#if BUILDFLAG(USE_FONTATIONS_BACKEND) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE))
  if (RuntimeEnabledFeatures::FontationsFontBackendEnabled()) {
    std::unique_ptr<SkStreamAsset> stream(new SkMemoryStream(data));
    return SkTypeface_Make_Fontations(std::move(stream), SkFontArguments());
  }
#endif
  return SkFontMgr_New_Custom_Empty()->makeFromData(data, 0);
}
#endif

sk_sp<SkTypeface> MakeVariationsTypeface(
    sk_sp<SkData> data,
    const WebFontTypefaceFactory::FontInstantiator& instantiator) {
#if BUILDFLAG(IS_WIN)
  if (!DWriteVersionSupportsVariations()) {
    return instantiator.make_fallback(data);
  }
#endif
  return instantiator.make_system(data);
}

sk_sp<SkTypeface> MakeSbixTypeface(
    sk_sp<SkData> data,
    const WebFontTypefaceFactory::FontInstantiator& instantiator) {
#if BUILDFLAG(IS_WIN)
  return instantiator.make_fallback(data);
#else
  // On Mac, CoreText can handle creating SBIX fonts, on Linux-like OSes,
  // FreeType is the default manager and handles SBIX.
  return instantiator.make_system(data);
#endif
}

sk_sp<SkTypeface> MakeColrV0Typeface(
    sk_sp<SkData> data,
    const WebFontTypefaceFactory::FontInstantiator& instantiator) {
#if BUILDFLAG(IS_APPLE)
  return instantiator.make_fallback(data);
#else
  // On Windows, Skia's DirectWrite backend handles COLRv0, on Linux-like OSes,
  // FreeType is the default font manager and handles COLRv0.
  return instantiator.make_system(data);
#endif
}

sk_sp<SkTypeface> MakeColrV0VariationsTypeface(
    sk_sp<SkData> data,
    const WebFontTypefaceFactory::FontInstantiator& instantiator) {
#if BUILDFLAG(IS_WIN)
  if (DWriteVersionSupportsVariations()) {
    return instantiator.make_system(data);
  } else {
    return instantiator.make_fallback(data);
  }
#elif BUILDFLAG(IS_APPLE)
  return instantiator.make_fallback(data);
#else
  return instantiator.make_system(data);
#endif
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

}  // namespace

bool WebFontTypefaceFactory::CreateTypeface(sk_sp<SkData> data,
                                            sk_sp<SkTypeface>& typeface) {
  const FontFormatCheck format_check(data);
  const FontInstantiator instantiator = {
    MakeTypefaceDefaultFontMgr,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
    MakeTypefaceFallback
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
    absl::optional<InstantiationResult> reportSuccess;
    absl::optional<InstantiationResult> reportFailure;
  } instantiation_rules[] = {
      // We don't expect variable CBDT/CBLC or Sbix variable fonts for now.
      {&FontFormatCheck::IsCbdtCblcColorFont, &MakeUseFallbackIfNeeded,
       InstantiationResult::kSuccessCbdtCblcColorFont, absl::nullopt},
      {&FontFormatCheck::IsColrCpalColorFontV1, &MakeUseFallbackIfNeeded,
       InstantiationResult::kSuccessColrV1Font, absl::nullopt},
      {&FontFormatCheck::IsSbixColorFont, &MakeSbixTypeface,
       InstantiationResult::kSuccessSbixFont, absl::nullopt},
      {&FontFormatCheck::IsCff2OutlineFont, &MakeUseFallbackIfNeeded,
       InstantiationResult::kSuccessCff2Font, absl::nullopt},
      // We need to special case variable COLRv0 for backend instantiation as
      // certain Mac and Windows versions supported COLRv0 only without
      // variations.
      {&FontFormatCheck::IsVariableColrV0Font, &MakeColrV0VariationsTypeface,
       InstantiationResult::kSuccessColrCpalFont, absl::nullopt},
      {&FontFormatCheck::IsVariableFont, &MakeVariationsTypeface,
       InstantiationResult::kSuccessVariableWebFont,
       InstantiationResult::kErrorInstantiatingVariableFont},
      {&FontFormatCheck::IsColrCpalColorFontV0, &MakeColrV0Typeface,
       InstantiationResult::kSuccessColrCpalFont, absl::nullopt}};

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
