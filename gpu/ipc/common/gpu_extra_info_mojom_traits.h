// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_EXTRA_INFO_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_GPU_EXTRA_INFO_MOJOM_TRAITS_H_

#include "gpu/config/gpu_extra_info.h"
#include "gpu/ipc/common/gpu_extra_info.mojom.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::ANGLEFeatureDataView, gpu::ANGLEFeature> {
  static bool Read(gpu::mojom::ANGLEFeatureDataView data,
                   gpu::ANGLEFeature* out);

  static const std::string& name(const gpu::ANGLEFeature& input) {
    return input.name;
  }

  static const std::string& category(const gpu::ANGLEFeature& input) {
    return input.category;
  }

  static const std::string& description(const gpu::ANGLEFeature& input) {
    return input.description;
  }

  static const std::string& bug(const gpu::ANGLEFeature& input) {
    return input.bug;
  }

  static const std::string& status(const gpu::ANGLEFeature& input) {
    return input.status;
  }

  static const std::string& condition(const gpu::ANGLEFeature& input) {
    return input.condition;
  }
};

template <>
struct StructTraits<gpu::mojom::GpuExtraInfoDataView, gpu::GpuExtraInfo> {
  static bool Read(gpu::mojom::GpuExtraInfoDataView data,
                   gpu::GpuExtraInfo* out);

  static const std::vector<gpu::ANGLEFeature>& angle_features(
      const gpu::GpuExtraInfo& input) {
    return input.angle_features;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_GPU_EXTRA_INFO_MOJOM_TRAITS_H_
