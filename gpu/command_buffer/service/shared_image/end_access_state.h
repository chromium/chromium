// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_END_ACCESS_STATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_END_ACCESS_STATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"

namespace gpu {

class GraphiteSharedContext;

// Holds state that needs to be applied before ending access. This will be
// empty for Graphite/Dawn as Dawn handles transitions and synchronization.
class GPU_GLES2_EXPORT EndAccessState {
 public:
  EndAccessState();
  EndAccessState(GraphiteSharedContext* context,
                 skgpu::graphite::BackendTexture texture,
                 std::unique_ptr<skgpu::MutableTextureState> end_state);
  EndAccessState(EndAccessState&& other);
  EndAccessState& operator=(EndAccessState&& other);
  ~EndAccessState();

  // Returns true if Apply() needs to be called.
  bool NeedsApply() const;

  void Apply();

 private:
  raw_ptr<GraphiteSharedContext> context_ = nullptr;
  skgpu::graphite::BackendTexture texture_;
  std::unique_ptr<skgpu::MutableTextureState> end_state_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_END_ACCESS_STATE_H_
