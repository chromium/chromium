// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/font_utils.h"

#include "base/check.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

#if BUILDFLAG(IS_ANDROID)
#include <android/api-level.h>

#include "base/feature_list.h"
#include "third_party/skia/include/ports/SkFontMgr_android.h"
#include "third_party/skia/include/ports/SkFontMgr_android_ndk.h"
#include "third_party/skia/include/ports/SkFontScanner_Fontations.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "third_party/skia/include/ports/SkFontMgr_mac_ct.h"
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "third_party/skia/include/ports/SkFontConfigInterface.h"
#include "third_party/skia/include/ports/SkFontMgr_FontConfigInterface.h"
#include "third_party/skia/include/ports/SkFontScanner_Fontations.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <fuchsia/fonts/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/process_context.h"
#include "third_party/skia/include/ports/SkFontMgr_fuchsia.h"
#include "third_party/skia/include/ports/SkFontScanner_Fontations.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "third_party/skia/include/ports/SkTypeface_win.h"
#endif

#if defined(SK_FONTMGR_FREETYPE_EMPTY_AVAILABLE)
#include "third_party/skia/include/ports/SkFontMgr_empty.h"
#endif

#include <mutex>

namespace {

bool g_factory_called = false;

// This is a purposefully leaky pointer that has ownership of the FontMgr.
SkFontMgr* g_fontmgr_override = nullptr;

#if BUILDFLAG(IS_ANDROID)
// https://crbug.com/461659286 Failure to create any font causes crash.
BASE_FEATURE(kUseAndroidNDKFontAPI, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace

namespace skia {

static sk_sp<SkFontMgr> fontmgr_factory() {
  if (g_fontmgr_override) {
    return sk_ref_sp(g_fontmgr_override);
  }

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(kUseAndroidNDKFontAPI) &&
      android_get_device_api_level() > __ANDROID_API_V__) {
    sk_sp<SkFontMgr> ndk_fontmgr =
        SkFontMgr_New_AndroidNDK(false, SkFontScanner_Make_Fontations());
    if (ndk_fontmgr && ndk_fontmgr->countFamilies()) {
      return ndk_fontmgr;
    }
  }
  return SkFontMgr_New_Android(nullptr, SkFontScanner_Make_Fontations());
#elif BUILDFLAG(IS_APPLE)
  return SkFontMgr_New_CoreText(nullptr);
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  sk_sp<SkFontConfigInterface> fci(SkFontConfigInterface::RefGlobal());
  return fci ? SkFontMgr_New_FCI(std::move(fci),
                                 SkFontScanner_Make_Fontations())
             : nullptr;
#elif BUILDFLAG(IS_FUCHSIA)
  fuchsia::fonts::ProviderSyncPtr provider;
  base::ComponentContextForProcess()->svc()->Connect(provider.NewRequest());
  return SkFontMgr_New_Fuchsia(std::move(provider),
                               SkFontScanner_Make_Fontations());
#elif BUILDFLAG(IS_WIN)
  return SkFontMgr_New_DirectWrite();
#elif defined(SK_FONTMGR_FREETYPE_EMPTY_AVAILABLE)
  return SkFontMgr_New_Custom_Empty();
#else
  return SkFontMgr::RefEmpty();
#endif
}

sk_sp<SkFontMgr> DefaultFontMgr() {
  static std::once_flag flag;
  static SkFontMgr* mgr;
  std::call_once(flag, [] {
    mgr = fontmgr_factory().release();
    g_factory_called = true;
  });
  return sk_ref_sp(mgr);
}

void OverrideDefaultSkFontMgr(sk_sp<SkFontMgr> fontmgr) {
  CHECK(!g_factory_called);

  SkSafeUnref(g_fontmgr_override);
  g_fontmgr_override = fontmgr.release();
}

sk_sp<SkTypeface> MakeTypefaceFromName(const char* name, SkFontStyle style) {
  sk_sp<SkFontMgr> fm = DefaultFontMgr();
  CHECK(fm);
  sk_sp<SkTypeface> face = fm->legacyMakeTypeface(name, style);
  return face;
}

sk_sp<SkTypeface> DefaultTypeface() {
  sk_sp<SkTypeface> face = MakeTypefaceFromName(nullptr, SkFontStyle());
  if (face) {
    return face;
  }
  // Due to how SkTypeface::MakeDefault() used to work, many callers of this
  // depend on the returned SkTypeface being non-null. An empty Typeface is
  // non-null, but has no glyphs.
  face = SkTypeface::MakeEmpty();
  CHECK(face);
  return face;
}

SkFont DefaultFont() {
  return SkFont(DefaultTypeface());
}

}  // namespace skia
