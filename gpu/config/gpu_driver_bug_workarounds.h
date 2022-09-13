// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUNDS_H_
#define GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUNDS_H_

#include <vector>

#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/gpu_export.h"

// Forwardly declare a few GL types to avoid including GL header files.
typedef int GLint;

namespace gpu {

class GPU_EXPORT GpuDriverBugWorkarounds {
 public:
  GpuDriverBugWorkarounds();
  explicit GpuDriverBugWorkarounds(const std::vector<int32_t>&);

  GpuDriverBugWorkarounds(const GpuDriverBugWorkarounds& other);

  ~GpuDriverBugWorkarounds();

  // For boolean members, || is applied.
  // For int members, the min() is applied if both are non-zero; if one is
  // zero, then the other is applied.
  void Append(const GpuDriverBugWorkarounds& extra);

  std::vector<int32_t> ToIntSet() const;

#define GPU_OP(type, name) bool name = false;
  GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP

  // Note: 0 here means use driver limit.
  GLint webgl_or_caps_max_texture_size = 0;
  GLint max_3d_array_texture_size = 0;
  GLint max_copy_texture_chromium_size = 0;
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUNDS_H_
