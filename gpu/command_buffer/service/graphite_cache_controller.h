// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_CACHE_CONTROLLER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_CACHE_CONTROLLER_H_

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "gpu/gpu_gles2_export.h"

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

namespace gpu {
class DawnContextProvider;
class GraphiteSharedContext;

namespace raster {
// GraphiteCacheController is not thread-safe; it can be created on any thread,
// but it must be destroyed on the same thread that ScheduleCleanup is called.
class GPU_GLES2_EXPORT GraphiteCacheController final
    : public base::RefCounted<GraphiteCacheController> {
 public:
  // |context| and |dawn_context_provider| are optional e.g. Viz thread
  // GraphiteCacheController won't cleanup the Graphite context or
  // DawnContextProvider which live on GPU main thread.
  explicit GraphiteCacheController(
      skgpu::graphite::Recorder* recorder,
      bool can_handle_context_resources = false,
      DawnContextProvider* dawn_context_provider = nullptr);

  GraphiteCacheController(const GraphiteCacheController&) = delete;
  GraphiteCacheController& operator=(const GraphiteCacheController&) = delete;

  // Schedule cleanup for the graphite cache; the cleanup will be performed
  // after ScheduleCleanup() is not called for a while after going idle.
  void ScheduleCleanup();

  base::WeakPtr<GraphiteCacheController> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Cleans up all Skia-owned scratch resources.
  void CleanUpScratchResources();

  // Cleans up all resources.
  void CleanUpAllResources();

 private:
  friend class base::RefCounted<GraphiteCacheController>;
  ~GraphiteCacheController();

  // If the controller is only for a recorder, aka for viz thread recorer, then
  // it operates in local mode where it only considers if current controller is
  // idle. If the controller is for recorder+context then it operates in global
  // mode where it waits for all global controllers to be idle.
  bool UseGlobalIdleId() const;
  uint32_t GetIdleId() const;

  void ScheduleCleanUpAllResources(uint32_t idle_id);
  void MaybeCleanUpAllResources(uint32_t posted_idle_id);
  void CleanUpAllResourcesImpl();
  GraphiteSharedContext* GetGraphiteSharedContext();

  const raw_ptr<skgpu::graphite::Recorder> recorder_;
  const raw_ptr<DawnContextProvider> dawn_context_provider_;

  uint32_t local_idle_id_ = 0;
  base::CancelableOnceClosure idle_cleanup_cb_;

  // If multiple DawnContextProviders share the same graphite::Context, only one
  // controller is responsible for cleaning up the graphite context resources.
  // TODO(crbug.com/407874799): Fix the issue that dawn cleanup won't happen if
  // no work is done on CrGpuMain to call into ScheduleCleanup() and start the
  // timer.
  const bool can_handle_context_resources_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GraphiteCacheController> weak_ptr_factory_{this};
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_CACHE_CONTROLLER_H_
