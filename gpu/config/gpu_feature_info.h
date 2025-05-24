// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_FEATURE_INFO_H_
#define GPU_CONFIG_GPU_FEATURE_INFO_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/gpu_export.h"

namespace gfx {
enum class BufferFormat : uint8_t;
}

namespace gl {
class GLContext;
}  // namespace gl

namespace gpu {

// Flags indicating the status of a GPU feature (see gpu_feature_type.h).
enum GpuFeatureStatus {
  kGpuFeatureStatusEnabled,
  kGpuFeatureStatusBlocklisted,
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

  // An array of GpuFeatureStatus values, one per GpuFeatureType.
  // By default, all features are disabled.
  std::array<GpuFeatureStatus, NUMBER_OF_GPU_FEATURE_TYPES> status_values;
  // Active gpu driver bug workaround IDs.
  // See gpu/config/gpu_driver_bug_workaround_type.h for ID mappings.
  std::vector<int32_t> enabled_gpu_driver_bug_workarounds;
  // Disabled extensions separated by whitespaces.
  std::string disabled_extensions;
  // Disabled WebGL extensions separated by whitespaces.
  std::string disabled_webgl_extensions;
  // Applied gpu blocklist entry indices.
  std::vector<uint32_t> applied_gpu_blocklist_entries;
  // Applied gpu driver bug list entry indices.
  std::vector<uint32_t> applied_gpu_driver_bug_list_entries;

  // BufferFormats that can be allocated and then bound, if known and provided
  // by the platform.
  std::vector<gfx::BufferFormat>
      supported_buffer_formats_for_allocation_and_texturing;
#if BUILDFLAG(IS_OZONE)
  // BufferFormats of native pixmaps that can be imported in GL context.
  std::vector<gfx::BufferFormat>
      supported_buffer_formats_for_gl_native_pixmap_import;
#endif  // BUILDFLAG(IS_OZONE)
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_FEATURE_INFO_H_
