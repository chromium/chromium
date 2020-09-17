// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_FENCE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_FENCE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "gpu/gpu_gles2_export.h"

namespace gfx {
struct GpuFenceHandle;
class GpuFence;
}  // namespace gfx

namespace gl {
class GLFence;
}

namespace gpu {
namespace gles2 {

// This class keeps track of GpuFence objects and their state. As GpuFence
// objects are not shared there is one GpuFenceManager per context.
class GPU_GLES2_EXPORT GpuFenceManager {
  class GPU_GLES2_EXPORT GpuFenceEntry {
   public:
    GpuFenceEntry();
    ~GpuFenceEntry();

    GpuFenceEntry(GpuFenceEntry&& other);
    GpuFenceEntry& operator=(GpuFenceEntry&& other);

   private:
    friend class GpuFenceManager;
    std::unique_ptr<gl::GLFence> gl_fence_;

    DISALLOW_COPY_AND_ASSIGN(GpuFenceEntry);
  };

 public:
  GpuFenceManager();
  ~GpuFenceManager();

  bool CreateGpuFence(uint32_t client_id);

  bool CreateGpuFenceFromHandle(uint32_t client_id, gfx::GpuFenceHandle handle);

  bool IsValidGpuFence(uint32_t client_id);

  std::unique_ptr<gfx::GpuFence> GetGpuFence(uint32_t client_id);

  bool GpuFenceServerWait(uint32_t client_id);

  bool RemoveGpuFence(uint32_t client_id);

  // Must call before destruction.
  void Destroy(bool have_context);

 private:
  using GpuFenceEntryMap =
      base::flat_map<uint32_t, std::unique_ptr<GpuFenceEntry>>;
  GpuFenceEntryMap gpu_fence_entries_;

  DISALLOW_COPY_AND_ASSIGN(GpuFenceManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_FENCE_MANAGER_H_
