// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gr_cache_controller.h"

#include <chrono>

#include "base/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

namespace gpu {
namespace raster {

GrCacheController::GrCacheController(SharedContextState* context_state)
    : context_state_(context_state),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

GrCacheController::GrCacheController(
    SharedContextState* context_state,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_state_(context_state), task_runner_(std::move(task_runner)) {}

GrCacheController::~GrCacheController() = default;

void GrCacheController::ScheduleGrContextCleanup() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(context_state_->IsCurrent(nullptr));

  if (!context_state_->gr_context())
    return;

  current_idle_id_++;
  if (!purge_gr_cache_cb_.IsCancelled())
    return;

  constexpr int kOldResourceCleanupDelaySeconds = 5;
  // Here we ask GrContext to free any resources that haven't been used in
  // a long while even if it is under budget. Below we set a call back to
  // purge all possible GrContext resources if the context itself is not being
  // used.
  context_state_->set_need_context_state_reset(true);
  context_state_->gr_context()->performDeferredCleanup(
      std::chrono::seconds(kOldResourceCleanupDelaySeconds));

  constexpr int kIdleCleanupDelaySeconds = 1;
  purge_gr_cache_cb_.Reset(base::BindOnce(&GrCacheController::PurgeGrCache,
                                          base::Unretained(this),
                                          current_idle_id_));
  task_runner_->PostDelayedTask(FROM_HERE, purge_gr_cache_cb_.callback(),
                                base::Seconds(kIdleCleanupDelaySeconds));
}

void GrCacheController::PurgeGrCache(uint64_t idle_id) {
  purge_gr_cache_cb_.Cancel();

  // We don't care which surface is current. This improves
  // performance. https://crbug.com/457431
  if (!context_state_->MakeCurrent(nullptr))
    return;

  // If the idle id changed, the context was used after this callback was
  // posted. Schedule another one.
  if (idle_id != current_idle_id_) {
    ScheduleGrContextCleanup();
    return;
  }

  context_state_->set_need_context_state_reset(true);

  // Force Skia to check fences to determine what can be freed.
  context_state_->gr_context()->checkAsyncWorkCompletion();
  context_state_->gr_context()->freeGpuResources();

  // Skia may have released resources, but the driver may not process that
  // without a flush.
  if (context_state_->GrContextIsGL()) {
    auto* api = gl::g_current_gl_context;
    api->glFlushFn();
  }

  // Skia store VkPipeline cache only on demand. We do it when we're idle idle
  // as it might take time.
  context_state_->StoreVkPipelineCacheIfNeeded();
}

}  // namespace raster
}  // namespace gpu
