// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/web_font_typeface_factory.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
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

namespace blink {

bool WebFontTypefaceFactory::CreateTypeface(sk_sp<SkData> data,
                                            sk_sp<SkTypeface>& typeface) {
  CHECK(!typeface);

  FontFormatCheck format_check(data);

  if (!format_check.IsVariableFont() && !format_check.IsColorFont()) {
    typeface = MakeTypefaceDefaultFontMgr(data);
    if (typeface) {
      ReportInstantiationResult(
          InstantiationResult::kSuccessConventionalWebFont);
      return true;
    }
    // Not UMA reporting general decoding errors as these are already recorded
    // as kPackageFormatUnknown in FontResource.cpp.
    return false;
  }

  // We don't expect variable CBDT/CBLC or Sbix variable fonts for now.
  if (format_check.IsCbdtCblcColorFont()) {
    typeface = MakeTypefaceFreeType(data);
    if (typeface) {
      ReportInstantiationResult(InstantiationResult::kSuccessCbdtCblcColorFont);
    }
    return typeface.get();
  }

  if (format_check.IsColrCpalColorFontV1()) {
    typeface = MakeTypefaceFreeType(data);
    if (typeface) {
      ReportInstantiationResult(InstantiationResult::kSuccessColrV1Font);
    }
    return typeface.get();
  }

  if (format_check.IsSbixColorFont()) {
    typeface = MakeSbixTypeface(data);
    if (typeface) {
      ReportInstantiationResult(InstantiationResult::kSuccessSbixFont);
    }
    return typeface.get();
  }

  // CFF2 must always go through the FreeTypeFontManager, even on Mac OS, as it
  // is not natively supported.
  if (format_check.IsCff2OutlineFont()) {
    typeface = MakeTypefaceFreeType(data);
    if (typeface)
      ReportInstantiationResult(InstantiationResult::kSuccessCff2Font);
    return typeface.get();
  }

  // We need to have a separate method for retrieving the COLRv0 compatible font
  // manager with platform specific decisions. This is because: If we would
  // always use the FontManagerForVariations(), then on Mac COLRv0 fonts would
  // not have variation parameters applied. If we would always prefer the COLRv0
  // font manager, then this may lack variations support on Windows if we are on
  // a Windows versions that did not support variations yet. Windows supported
  // COLRv0 before variations.
  if (format_check.IsVariableFont() && format_check.IsColrCpalColorFontV0()) {
    typeface = MakeColrV0VariationsTypeface(data);
    if (typeface)
      ReportInstantiationResult(InstantiationResult::kSuccessColrCpalFont);
    return typeface.get();
  }

  if (format_check.IsVariableFont()) {
    typeface = MakeVariationsTypeface(data);
    if (typeface) {
      ReportInstantiationResult(InstantiationResult::kSuccessVariableWebFont);
    } else {
      ReportInstantiationResult(
          InstantiationResult::kErrorInstantiatingVariableFont);
    }
    return typeface.get();
  }

  if (format_check.IsColrCpalColorFontV0()) {
    typeface = MakeColrV0Typeface(data);
    if (typeface) {
      ReportInstantiationResult(InstantiationResult::kSuccessColrCpalFont);
    }
    return typeface.get();
  }

  return false;
}

sk_sp<SkTypeface> WebFontTypefaceFactory::MakeTypefaceDefaultFontMgr(
    sk_sp<SkData> data) {
#if BUILDFLAG(USE_FONTATIONS_BACKEND)
  if (RuntimeEnabledFeatures::FontationsFontBackendEnabled()) {
    return MakeTypefaceFontations(data);
  }
#endif

  sk_sp<SkFontMgr> font_manager;
#if BUILDFLAG(IS_WIN)
  font_manager = FontCache::Get().FontManager();
#else
  font_manager = SkFontMgr::RefDefault();
#endif
  return font_manager->makeFromData(data, 0);
}

sk_sp<SkTypeface> WebFontTypefaceFactory::MakeTypefaceFreeType(
    sk_sp<SkData> data) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
  return SkFontMgr_New_Custom_Empty()->makeFromData(data, 0);
#else
  return MakeTypefaceDefaultFontMgr(data);
#endif
}

#if BUILDFLAG(USE_FONTATIONS_BACKEND)
sk_sp<SkTypeface> WebFontTypefaceFactory::MakeTypefaceFontations(
    sk_sp<SkData> data) {
  std::unique_ptr<SkStreamAsset> stream(new SkMemoryStream(data));
  return SkTypeface_Make_Fontations(std::move(stream), SkFontArguments());
}
#endif

sk_sp<SkTypeface> WebFontTypefaceFactory::MakeVariationsTypeface(
    sk_sp<SkData> data) {
#if BUILDFLAG(IS_WIN)
  if (!DWriteVersionSupportsVariations()) {
    return MakeTypefaceFreeType(data);
  }
#endif
  return MakeTypefaceDefaultFontMgr(data);
}

sk_sp<SkTypeface> WebFontTypefaceFactory::MakeSbixTypeface(sk_sp<SkData> data) {
#if BUILDFLAG(IS_MAC)
  return MakeTypefaceDefaultFontMgr(data);
#else
  return MakeTypefaceFreeType(data);
#endif
}

sk_sp<SkTypeface> WebFontTypefaceFactory::MakeColrV0Typeface(
    sk_sp<SkData> data) {
#if BUILDFLAG(IS_APPLE)
  return MakeTypefaceFreeType(data);
#else
  return MakeTypefaceDefaultFontMgr(data);
#endif
}

sk_sp<SkTypeface> WebFontTypefaceFactory::MakeColrV0VariationsTypeface(
    sk_sp<SkData> data) {
#if BUILDFLAG(IS_WIN)
  if (DWriteVersionSupportsVariations()) {
    return MakeTypefaceDefaultFontMgr(data);
  }
#endif
  return MakeTypefaceFreeType(data);
}

void WebFontTypefaceFactory::ReportInstantiationResult(
    InstantiationResult result) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Fonts.VariableFontsRatio", result);
}

}  // namespace blink
