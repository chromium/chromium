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

struct SharedImageInfo : public SharedImageMetadata {
  SharedImageInfo(const viz::SharedImageFormat& format_in,
                  gfx::Size size_in,
                  const gfx::ColorSpace& color_space_in,
                  GrSurfaceOrigin surface_origin_in,
                  SkAlphaType alpha_type_in,
                  SharedImageUsageSet usage_in,
                  std::string_view debug_label)
      : SharedImageMetadata{format_in,         size_in,       color_space_in,
                            surface_origin_in, alpha_type_in, usage_in},
        debug_label(debug_label) {}
  SharedImageInfo(const viz::SharedImageFormat& format_in,
                  gfx::Size size_in,
                  const gfx::ColorSpace& color_space_in,
                  SharedImageUsageSet usage_in,
                  std::string_view debug_label)
      : SharedImageMetadata{format_in,           size_in,
                            color_space_in,      kTopLeft_GrSurfaceOrigin,
                            kPremul_SkAlphaType, usage_in},
        debug_label(debug_label) {}
  SharedImageInfo(const SharedImageMetadata& meta, std::string_view debug_label)
      : SharedImageMetadata(meta), debug_label(debug_label) {}

  std::string debug_label;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_INFO_H_
