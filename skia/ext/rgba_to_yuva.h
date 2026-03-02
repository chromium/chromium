// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_RGBA_TO_YUVA_H_
#define SKIA_EXT_RGBA_TO_YUVA_H_

#include <vector>

#include "base/containers/span.h"
#include "third_party/skia/include/core/SkColor.h"
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
// If `src_image` is null, draw black rect to the `dst_region` instead.
SK_API void BlitRGBAToYUVA(SkImage* src_image,
                           base::span<SkSurface* const> dst_surfaces,
                           const SkYUVAInfo& dst_yuva_info,
                           const SkRect& dst_region = SkRect::MakeEmpty(),
                           bool clear_destination = false,
                           const SkRect& src_region = SkRect::MakeEmpty());

// Perform RGB to YUV conversion and color space conversion. The SkColorSpace
// conversion will be from `src_pm`'s color space to `dst_pixmaps`' color
// spaces (which are CHECKed to be all the same). The `dst_yuva_info` must
// be valid. If `dst_yuva_info` is opaque but `src_pm` isn't, then `src_pm`
// will be blended against black.
SK_API void ConvertRGBAToYUVA(SkPixmap src_pm,
                              const SkYUVAInfo& dst_yuva_info,
                              const std::vector<SkPixmap>& dst_pixmaps);

// Convert the planes in `src_pixmaps` to the RGBA `dst`. If `src_yuva_info`
// is valid, then YUV to RGB conversion will be performed, and `src_bit_depth`
// will be used as the bit depth. Otherwise, an RGBA to RGBA conversion using
// `src_pixmaps[0]` will be performed. The SkColorSpace of all of `src_pixmaps`
// must be equal, and will be used as the source color space.
// WARNING: This is not optimized and is not suitable for use outside of tests.
SK_API void ConvertYUVAToRGBA(const SkYUVAInfo& src_yuva_info,
                              size_t src_bit_depth,
                              const std::vector<SkPixmap>& src_pixmaps,
                              const SkPixmap& dst);

}  // namespace skia

#endif  // SKIA_EXT_RGBA_TO_YUVA_H_
