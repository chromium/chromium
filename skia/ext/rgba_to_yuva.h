// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_RGBA_TO_YUVA_H_
#define SKIA_EXT_RGBA_TO_YUVA_H_

#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"

namespace skia {

// Copy `src_image` from RGBA to the YUVA planes specified in `dst_surfaces`,
// using the color space and plane configuration information specified in
// `dst_yuva_info`. `dst_yuva_info` describes the entire destination image - the
// results of the blit operation will be placed in its subregion, described by
// `dst_region`. If a default-constructed `dst_region` is passed in, the entire
// destination image will be written to. If `clear_destination` is true, the
// entire destination image will be cleared with black before the blit.
SK_API void BlitRGBAToYUVA(SkImage* src_image,
                           SkSurface* dst_surfaces[SkYUVAInfo::kMaxPlanes],
                           const SkYUVAInfo& dst_yuva_info,
                           const SkRect& dst_region = SkRect::MakeEmpty(),
                           bool clear_destination = false);

}  // namespace skia

#endif  // SKIA_EXT_RGBA_TO_YUVA_H_
