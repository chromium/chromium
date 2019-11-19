// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_FEATURE_INFO_H_
#define GPU_CONFIG_GPU_FEATURE_INFO_H_

#include <string>
#include <vector>

#include "gpu/config/gpu_feature_type.h"
#include "gpu/gpu_export.h"

namespace gfx {
enum class BufferFormat;
}

namespace gl {
class GLContext;
}  // namespace gl

namespace gpu {

// Flags indicating the status of a GPU feature (see gpu_feature_type.h).
enum GpuFeatureStatus {
  kGpuFeatureStatusEnabled,
  kGpuFeatureStatusBlacklisted,
  kGpuFeatureStatusDisabled,
  kGpuFeatureStatusSoftware,
  kGpuFeatureStatusUndefined,
  kGpuFeatureStatusMax
};

struct GPU_EXPORT GpuFeatureInfo {
  GpuFeatureInfo();
  GpuFeatureInfo(const GpuFeatureInfo&);
  GpuFeatureInfo(GpuFeatureInfo&&);
  ~GpuFeatureInfo();

  // Set the GL workarounds and disabled GL extensions to the context.
  void ApplyToGLContext(gl::GLContext* context) const;

  bool IsWorkaroundEnabled(int32_t workaround) const;

  // Return true if GpuFeatureInfo is computed.
  bool IsInitialized() const;

  GpuFeatureInfo& operator=(const GpuFeatureInfo&);
  GpuFeatureInfo& operator=(GpuFeatureInfo&&);

  // A vector of GpuFeatureStatus values, one per GpuFeatureType.
  // By default, all features are disabled.
  GpuFeatureStatus status_values[NUMBER_OF_GPU_FEATURE_TYPES];
  // Active gpu driver bug workaround IDs.
  // See gpu/config/gpu_driver_bug_workaround_type.h for ID mappings.
  std::vector<int32_t> enabled_gpu_driver_bug_workarounds;
  // Disabled extensions separated by whitespaces.
  std::string disabled_extensions;
  // Disabled WebGL extensions separated by whitespaces.
  std::string disabled_webgl_extensions;
  // Applied gpu blacklist entry indices.
  std::vector<uint32_t> applied_gpu_blacklist_entries;
  // Applied gpu driver bug list entry indices.
  std::vector<uint32_t> applied_gpu_driver_bug_list_entries;

  // BufferFormats that can be allocated and then bound, if known and provided
  // by the platform.
  std::vector<gfx::BufferFormat>
      supported_buffer_formats_for_allocation_and_texturing;
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_FEATURE_INFO_H_
