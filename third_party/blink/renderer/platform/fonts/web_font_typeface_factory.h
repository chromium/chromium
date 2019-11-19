// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WEB_FONT_TYPEFACE_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WEB_FONT_TYPEFACE_FACTORY_H_

#include "third_party/skia/include/core/SkFontMgr.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#if defined(OS_WIN) || defined(OS_MACOSX)
#include "third_party/skia/include/ports/SkFontMgr_empty.h"
#endif

namespace blink {

// Decides which Skia Fontmanager to use for instantiating a web font. In the
// regular case, this would be default font manager used for the platform.
// However, for variable fonts, color bitmap font formats and CFF2 fonts we want
// to use FreeType on Windows and Mac.
class WebFontTypefaceFactory {
  STACK_ALLOCATED();

 public:
  static bool CreateTypeface(const sk_sp<SkData>, sk_sp<SkTypeface>&);

  // TODO(drott): This should be going away in favor of a new API on SkTypeface:
  // https://bugs.chromium.org/p/skia/issues/detail?id=7121
  static sk_sp<SkFontMgr> FontManagerForVariations();
  static sk_sp<SkFontMgr> FontManagerForSbix();
  static sk_sp<SkFontMgr> FreeTypeFontManager();
  static sk_sp<SkFontMgr> FontManagerForColrCpal();

 private:
  // These values are written to logs.  New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum WebFontInstantiationResult {
    kErrorInstantiatingVariableFont = 0,
    kSuccessConventionalWebFont = 1,
    kSuccessVariableWebFont = 2,
    kSuccessCbdtCblcColorFont = 3,
    kSuccessCff2Font = 4,
    kSuccessSbixFont = 5,
    kSuccessColrCpalFont = 6,
    kMaxWebFontInstantiationResult = 7
  };

  static sk_sp<SkFontMgr> DefaultFontManager();

  static void ReportWebFontInstantiationResult(WebFontInstantiationResult);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WEB_FONT_TYPEFACE_FACTORY_H_
