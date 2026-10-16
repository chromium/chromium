// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SKIA_SKTYPEFACE_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SKIA_SKTYPEFACE_FACTORY_H_

#include <string>

#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

sk_sp<SkTypeface> CreateSkTypefaceFromFontConfigInterfaceIdAndTtcIndex(
    int config_id,
    int ttc_index);
sk_sp<SkTypeface> CreateSkTypefaceFromFilenameAndTtcIndex(
    const std::string& filename,
    int ttc_index);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SKIA_SKTYPEFACE_FACTORY_H_
