// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/skia/sktypeface_factory.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_WIN) && \
    !BUILDFLAG(IS_FUCHSIA)
#include "third_party/skia/include/ports/SkFontMgr_Fontations.h"
#endif

namespace blink {

sk_sp<SkTypeface> CreateSkTypefaceFromFontConfigInterfaceIdAndTtcIndex(
    int config_id,
    int ttc_index) {
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_WIN) && \
    !BUILDFLAG(IS_FUCHSIA)
  sk_sp<SkFontConfigInterface> fci(SkFontConfigInterface::RefGlobal());
  SkFontConfigInterface::FontIdentity font_identity;
  font_identity.fID = config_id;
  font_identity.fTTCIndex = ttc_index;
  return fci->makeTypeface(font_identity, SkFontMgr_New_Fontations_Empty());
#else
  NOTREACHED();
#endif
}

sk_sp<SkTypeface> CreateSkTypefaceFromFilenameAndTtcIndex(
    const std::string& filename,
    int ttc_index) {
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA) && \
    !BUILDFLAG(IS_APPLE)
  return SkFontMgr_New_Fontations_Empty()->makeFromFile(filename.c_str(),
                                                        ttc_index);
#else
  NOTREACHED();
#endif
}

}  // namespace blink
