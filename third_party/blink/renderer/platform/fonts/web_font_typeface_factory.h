// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WEB_FONT_TYPEFACE_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WEB_FONT_TYPEFACE_FACTORY_H_

#include "third_party/skia/include/core/SkFontMgr.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/fontations_buildflags.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Decides which Skia backend to use for instantiating a web font. In the
// regular case, this would be default font manager used for the platform.
// However, for variable fonts, color bitmap font formats and CFF2 fonts we want
// to use FreeType on Windows and Mac.
class WebFontTypefaceFactory {
  STACK_ALLOCATED();

 public:
  static bool CreateTypeface(const sk_sp<SkData>, sk_sp<SkTypeface>&);

 private:
  static sk_sp<SkTypeface> MakeTypefaceDefaultFontMgr(sk_sp<SkData>);
  static sk_sp<SkTypeface> MakeTypefaceFreeType(sk_sp<SkData>);
#if BUILDFLAG(USE_FONTATIONS_BACKEND)
  static sk_sp<SkTypeface> MakeTypefaceFontations(sk_sp<SkData>);
#endif

  static sk_sp<SkTypeface> MakeVariationsTypeface(sk_sp<SkData>);
  static sk_sp<SkTypeface> MakeSbixTypeface(sk_sp<SkData>);
  static sk_sp<SkTypeface> MakeColrV0Typeface(sk_sp<SkData>);
  static sk_sp<SkTypeface> MakeColrV0VariationsTypeface(sk_sp<SkData>);

  // These values are written to logs.  New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum class InstantiationResult {
    kErrorInstantiatingVariableFont = 0,
    kSuccessConventionalWebFont = 1,
    kSuccessVariableWebFont = 2,
    kSuccessCbdtCblcColorFont = 3,
    kSuccessCff2Font = 4,
    kSuccessSbixFont = 5,
    kSuccessColrCpalFont = 6,
    kSuccessColrV1Font = 7,
    kMaxValue = kSuccessColrV1Font
  };

  static void ReportInstantiationResult(InstantiationResult);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WEB_FONT_TYPEFACE_FACTORY_H_
