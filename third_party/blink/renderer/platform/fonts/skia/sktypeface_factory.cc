// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/skia/sktypeface_factory.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

namespace blink {

// static
sk_sp<SkTypeface> SkTypeface_Factory::FromFontConfigInterfaceIdAndTtcIndex(
    int config_id,
    int ttc_index) {
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_WIN) && \
    !BUILDFLAG(IS_FUCHSIA)
  sk_sp<SkFontConfigInterface> fci(SkFontConfigInterface::RefGlobal());
  SkFontConfigInterface::FontIdentity font_identity;
  font_identity.fID = config_id;
  font_identity.fTTCIndex = ttc_index;
  return fci->makeTypeface(font_identity, skia::DefaultFontMgr());
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif
}

// static
sk_sp<SkTypeface> SkTypeface_Factory::FromFilenameAndTtcIndex(
    const std::string& filename,
    int ttc_index) {
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA) && \
    !BUILDFLAG(IS_APPLE)
  return skia::DefaultFontMgr()->makeFromFile(filename.c_str(), ttc_index);
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif
}

}  // namespace blink
