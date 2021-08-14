// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_RGBA_TO_YUVA_H_
#define SKIA_EXT_RGBA_TO_YUVA_H_

#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"

namespace skia {

// Copy `src_image` from RGBA to the YUVA planes specified in `dst_surfaces`,
// using the color space and plane configuration information specified in
// `dst_yuva_info`.
SK_API void BlitRGBAToYUVA(SkImage* src_image,
                           SkSurface* dst_surfaces[SkYUVAInfo::kMaxPlanes],
                           const SkYUVAInfo& dst_yuva_info);

}  // namespace skia

#endif  // SKIA_EXT_RGBA_TO_YUVA_H_
