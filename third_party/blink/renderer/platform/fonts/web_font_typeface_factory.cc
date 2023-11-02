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
#include "third_party/blink/public/common/dwrite_rasterizer_support/dwrite_rasterizer_support.h"
#include "third_party/blink/renderer/platform/fonts/win/dwrite_font_format_support.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "third_party/skia/include/ports/SkFontMgr_empty.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "third_party/blink/renderer/platform/fonts/mac/core_text_font_format_support.h"
#endif

namespace blink {

bool WebFontTypefaceFactory::CreateTypeface(sk_sp<SkData> sk_data,
                                            sk_sp<SkTypeface>& typeface) {
  CHECK(!typeface);

  FontFormatCheck format_check(sk_data);

  std::unique_ptr<SkStreamAsset> stream(new SkMemoryStream(sk_data));

  if (!format_check.IsVariableFont() && !format_check.IsColorFont()) {
    typeface = DefaultFontManager()->makeFromStream(std::move(stream));
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
    typeface = FreeTypeFontManager()->makeFromStream(std::move(stream));
    if (typeface) {
      ReportInstantiationResult(InstantiationResult::kSuccessCbdtCblcColorFont);
    }
    return typeface.get();
  }

  if (format_check.IsColrCpalColorFontV1()) {
    typeface = FreeTypeFontManager()->makeFromStream(std::move(stream));
    if (typeface) {
      ReportInstantiationResult(InstantiationResult::kSuccessColrV1Font);
    }
    return typeface.get();
  }

  if (format_check.IsSbixColorFont()) {
    typeface = FontManagerForSbix()->makeFromStream(std::move(stream));
    if (typeface) {
      ReportInstantiationResult(InstantiationResult::kSuccessSbixFont);
    }
    return typeface.get();
  }

  // CFF2 must always go through the FreeTypeFontManager, even on Mac OS, as it
  // is not natively supported.
  if (format_check.IsCff2OutlineFont()) {
    typeface = FreeTypeFontManager()->makeFromStream(std::move(stream));
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
    typeface =
        FontManagerForColrV0Variations()->makeFromStream(std::move(stream));
    if (typeface)
      ReportInstantiationResult(InstantiationResult::kSuccessColrCpalFont);
    return typeface.get();
  }

  if (format_check.IsVariableFont()) {
    typeface = FontManagerForVariations()->makeFromStream(std::move(stream));
    if (typeface) {
      ReportInstantiationResult(InstantiationResult::kSuccessVariableWebFont);
    } else {
      ReportInstantiationResult(
          InstantiationResult::kErrorInstantiatingVariableFont);
    }
    return typeface.get();
  }

  if (format_check.IsColrCpalColorFontV0()) {
    typeface = FontManagerForColrCpal()->makeFromStream(std::move(stream));
    if (typeface) {
      ReportInstantiationResult(InstantiationResult::kSuccessColrCpalFont);
    }
    return typeface.get();
  }

  return false;
}

sk_sp<SkFontMgr> WebFontTypefaceFactory::FontManagerForVariations() {
#if BUILDFLAG(IS_WIN)
  if (DWriteVersionSupportsVariations())
    return DefaultFontManager();
  return FreeTypeFontManager();
#else
#if BUILDFLAG(IS_MAC)
  if (!CoreTextVersionSupportsVariations())
    return FreeTypeFontManager();
#endif
  return DefaultFontManager();
#endif
}

sk_sp<SkFontMgr> WebFontTypefaceFactory::FontManagerForSbix() {
#if BUILDFLAG(IS_MAC)
  return DefaultFontManager();
#else
  return FreeTypeFontManager();
#endif
}

sk_sp<SkFontMgr> WebFontTypefaceFactory::DefaultFontManager() {
#if BUILDFLAG(IS_WIN)
  return FontCache::Get().FontManager();
#else
  return sk_sp<SkFontMgr>(SkFontMgr::RefDefault());
#endif
}

sk_sp<SkFontMgr> WebFontTypefaceFactory::FreeTypeFontManager() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return sk_sp<SkFontMgr>(SkFontMgr_New_Custom_Empty());
#else
  return DefaultFontManager();
#endif
}

sk_sp<SkFontMgr> WebFontTypefaceFactory::FontManagerForColrCpal() {
#if BUILDFLAG(IS_WIN)
  if (!blink::DWriteRasterizerSupport::IsDWriteFactory2Available())
    return FreeTypeFontManager();
#endif

#if BUILDFLAG(IS_MAC)
  return FreeTypeFontManager();
#else
  return DefaultFontManager();
#endif
}

sk_sp<SkFontMgr> WebFontTypefaceFactory::FontManagerForColrV0Variations() {
#if BUILDFLAG(IS_WIN)
  if (DWriteVersionSupportsVariations() &&
      blink::DWriteRasterizerSupport::IsDWriteFactory2Available())
    return DefaultFontManager();
#endif
  return FreeTypeFontManager();
}

void WebFontTypefaceFactory::ReportInstantiationResult(
    InstantiationResult result) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Fonts.VariableFontsRatio", result);
}

}  // namespace blink
