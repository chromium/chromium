// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_INFO_H_
#define GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_INFO_H_

#include <string>
#include <string_view>

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

struct SharedImageMetadata {
  viz::SharedImageFormat format;
  gfx::Size size;
  gfx::ColorSpace color_space;
  GrSurfaceOrigin surface_origin;
  SkAlphaType alpha_type;
  SharedImageUsageSet usage;
};

struct SharedImageInfo {
  SharedImageInfo(const viz::SharedImageFormat& format,
                  gfx::Size size,
                  const gfx::ColorSpace& color_space,
                  GrSurfaceOrigin surface_origin,
                  SkAlphaType alpha_type,
                  SharedImageUsageSet usage,
                  std::string_view debug_label)
      : meta(format, size, color_space, surface_origin, alpha_type, usage),
        debug_label(debug_label) {}
  SharedImageInfo(const viz::SharedImageFormat& format,
                  gfx::Size size,
                  const gfx::ColorSpace& color_space,
                  SharedImageUsageSet usage,
                  std::string_view debug_label)
      : meta(format,
             size,
             color_space,
             kTopLeft_GrSurfaceOrigin,
             kPremul_SkAlphaType,
             usage),
        debug_label(debug_label) {}
  SharedImageInfo(const SharedImageMetadata& meta, std::string_view debug_label)
      : meta(meta), debug_label(debug_label) {}

  SharedImageMetadata meta;
  std::string debug_label;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_INFO_H_
