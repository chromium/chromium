// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_SHARED_IMAGE_METADATA_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_SHARED_IMAGE_METADATA_MOJOM_TRAITS_H_

#include "base/check.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "gpu/ipc/common/shared_image_metadata.mojom-shared.h"
#include "services/viz/public/cpp/compositing/shared_image_format_mojom_traits.h"
#include "skia/public/mojom/image_info_mojom_traits.h"
#include "skia/public/mojom/surface_origin_mojom_traits.h"

namespace mojo {

template <>
struct GPU_IPC_COMMON_EXPORT StructTraits<
    gpu::mojom::SharedImageMetadataDataView,
    gpu::SharedImageMetadata> {
  static const viz::SharedImageFormat& format(
      const gpu::SharedImageMetadata& metadata) {
    return metadata.format;
  }

  static const gfx::Size& size(const gpu::SharedImageMetadata& metadata) {
    return metadata.size;
  }

  static const gfx::ColorSpace& color_space(
      const gpu::SharedImageMetadata& metadata) {
    return metadata.color_space;
  }

  static const GrSurfaceOrigin& surface_origin(
      const gpu::SharedImageMetadata& metadata) {
    return metadata.surface_origin;
  }

  static const SkAlphaType& alpha_type(
      const gpu::SharedImageMetadata& metadata) {
    return metadata.alpha_type;
  }

  static uint32_t usage(const gpu::SharedImageMetadata& metadata) {
    return uint32_t(metadata.usage);
  }

  static uint32_t array_layers(const gpu::SharedImageMetadata& metadata) {
    CHECK(metadata.array_layers >= 1);
    return metadata.array_layers;
  }

  static bool Read(gpu::mojom::SharedImageMetadataDataView data,
                   gpu::SharedImageMetadata* out) {
    if (data.array_layers() < 1 || !data.ReadFormat(&out->format) ||
        !data.ReadSize(&out->size) || !data.ReadColorSpace(&out->color_space) ||
        !data.ReadSurfaceOrigin(&out->surface_origin) ||
        !data.ReadAlphaType(&out->alpha_type)) {
      return false;
    }
    out->usage = gpu::SharedImageUsageSet(data.usage());
    out->array_layers = data.array_layers();
    return true;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_SHARED_IMAGE_METADATA_MOJOM_TRAITS_H_
