// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_DRAW_GAINMAP_IMAGE_H_
#define SKIA_EXT_DRAW_GAINMAP_IMAGE_H_

#include "skia/config/SkUserConfig.h"
#include "third_party/skia/include/core/SkImage.h"

struct SkGainmapInfo;
class SkCanvas;
class SkImage;
class SkPaint;

namespace skia {

// Function to perform the equivalent of SkCanvas::drawImageRect for a gainmap
// image. This will tile `base_image` and `gainmap_image` as needed.
SK_API void DrawGainmapImageRect(SkCanvas* canvas,
                                 sk_sp<SkImage> base_image,
                                 sk_sp<SkImage> gainmap_image,
                                 const SkGainmapInfo& gainmap_info,
                                 float hdr_headroom,
                                 const SkRect& source_rect,
                                 const SkRect& dest_rect,
                                 const SkSamplingOptions& sampling,
                                 const SkPaint& paint);

// Function to perform the equivalent of SkCanvas::drawImage for a gainmap
// image. This will tile `base_image` and `gainmap_image` as needed.
SK_API void DrawGainmapImage(SkCanvas* canvas,
                             sk_sp<SkImage> base_image,
                             sk_sp<SkImage> gainmap_image,
                             const SkGainmapInfo& gainmap_info,
                             float hdr_headroom,
                             SkScalar left,
                             SkScalar top,
                             const SkSamplingOptions& sampling,
                             const SkPaint& paint);

}  // namespace skia

#endif  // SKIA_EXT_DRAW_GAINMAP_IMAGE_H_
