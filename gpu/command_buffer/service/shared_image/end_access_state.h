// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_END_ACCESS_STATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_END_ACCESS_STATE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/gpu/graphite/BackendSemaphore.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"

namespace gpu {

class GraphiteSharedContext;

// Holds Vulkan texture state and signal semaphores that needs to be applied
// before ending graphite access to a shared image. This will be empty when
// external synchronization is not needed, eg. single-device access in
// Graphite/Vulkan or always for Graphite/Dawn.
class GPU_GLES2_EXPORT EndAccessState {
 public:
  EndAccessState();
  // When Apply() is called `signal_semaphore` will be enqueued to be signaled
  // and `texture_state` will be applied against `texture`.
  EndAccessState(GraphiteSharedContext* context,
                 skgpu::graphite::BackendTexture texture,
                 std::unique_ptr<skgpu::MutableTextureState> texture_state,
                 const skgpu::graphite::BackendSemaphore& signal_semaphore);
  EndAccessState(EndAccessState&& other);
  EndAccessState& operator=(EndAccessState&& other);
  ~EndAccessState();

  // Returns true if Apply() needs to be called.
  bool NeedsApply() const;

  // Must be called after all Graphite recordings that use the image have been
  // inserted into GraphiteSharedContext and before the submit that precedes the
  // end of access. No more work using the image may be inserted after this is
  // called. Does nothing if already applied.
  void Apply();

 private:
  raw_ptr<GraphiteSharedContext> context_ = nullptr;
  skgpu::graphite::BackendTexture texture_;
  std::unique_ptr<skgpu::MutableTextureState> texture_state_;
  // Set until the signal semaphore has been enqueued by Apply().
  std::optional<skgpu::graphite::BackendSemaphore> signal_semaphore_;
  // GraphiteSharedContext submit generation when the signal semaphore was
  // enqueued, if it has been enqueued.
  std::optional<uint64_t> signal_generation_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_END_ACCESS_STATE_H_
