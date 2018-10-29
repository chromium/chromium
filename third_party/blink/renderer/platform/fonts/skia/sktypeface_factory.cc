// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/skia/sktypeface_factory.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

namespace blink {

// static
sk_sp<SkTypeface> SkTypeface_Factory::FromFontConfigInterfaceIdAndTtcIndex(
    int config_id,
    int ttc_index) {
#if !defined(OS_MACOSX) && !defined(OS_ANDROID) && !defined(OS_WIN) && \
    !defined(OS_FUCHSIA)
  sk_sp<SkFontConfigInterface> fci(SkFontConfigInterface::RefGlobal());
  SkFontConfigInterface::FontIdentity font_identity;
  font_identity.fID = config_id;
  font_identity.fTTCIndex = ttc_index;
  return fci->makeTypeface(font_identity);
#else
  NOTREACHED();
  return nullptr;
#endif
}

// static
sk_sp<SkTypeface> SkTypeface_Factory::FromFilenameAndTtcIndex(
    const std::string& filename,
    int ttc_index) {
#if !defined(OS_WIN) && !defined(OS_ANDROID) && !defined(OS_FUCHSIA) && \
    !defined(OS_MACOSX)
  return SkTypeface::MakeFromFile(filename.c_str(), ttc_index);
#else
  NOTREACHED();
  return nullptr;
#endif
}

// static
sk_sp<SkTypeface> SkTypeface_Factory::FromFamilyNameAndFontStyle(
    const std::string& family_name,
    const SkFontStyle& font_style) {
#if !defined(OS_MACOSX)
  auto fm(SkFontMgr::RefDefault());
  return fm->legacyMakeTypeface(family_name.c_str(), font_style);
#else
  NOTREACHED();
  return nullptr;
#endif
}

}  // namespace blink
