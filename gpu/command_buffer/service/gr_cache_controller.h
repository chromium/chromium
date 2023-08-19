// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GR_CACHE_CONTROLLER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GR_CACHE_CONTROLLER_H_

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class SharedContextState;

namespace raster {
class GrCacheControllerTest;

// Manages clearing the GrContext cache after a period of inactivity.
// TODO(khushalsagar): This class replicates the ContextCacheController used in
// the renderer with GPU raster but ideally this logic should exist in the
// gpu::Scheduler, since it can better identify when we are in an idle state.
class GPU_GLES2_EXPORT GrCacheController {
 public:
  explicit GrCacheController(SharedContextState* context_state);

  GrCacheController(const GrCacheController&) = delete;
  GrCacheController& operator=(const GrCacheController&) = delete;

  ~GrCacheController();

  // Called to schedule purging the GrCache after a period of inactivity.
  void ScheduleGrContextCleanup();

 private:
  friend class GrCacheControllerTest;

  // Meant to be used by the GrCacheControllerTest.
  GrCacheController(SharedContextState* context_state,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  void PerformDeferredCleanupThrottled();
  void PerformDeferredCleanup();
  void PurgeGrCache(uint64_t idle_id);

  // The |current_idle_id_| is used to avoid continuously posting tasks to clear
  // the GrContext. Each time the context is used this id is incremented and
  // added to the callback. If the context is used between the time the callback
  // is posted and run, it sees a different id and further delays clearing the
  // cache.
  uint64_t current_idle_id_ = 0u;
  base::CancelableOnceClosure purge_gr_cache_cb_;
  raw_ptr<SharedContextState> context_state_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::TimeTicks last_cleanup_timestamp_;
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GR_CACHE_CONTROLLER_H_
