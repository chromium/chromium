// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gr_cache_controller.h"

#include <chrono>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "ui/gl/gl_context.h"

namespace gpu {
namespace raster {

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

  // Record memory usage periodically.
  RecordGrContextMemory();

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
  task_runner_->PostDelayedTask(
      FROM_HERE, purge_gr_cache_cb_.callback(),
      base::TimeDelta::FromSeconds(kIdleCleanupDelaySeconds));
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
  context_state_->gr_context()->freeGpuResources();
}

void GrCacheController::RecordGrContextMemory() const {
  int resource_count = 0;
  size_t resource_bytes = 0;
  context_state_->gr_context()->getResourceCacheUsage(&resource_count,
                                                      &resource_bytes);
  UMA_HISTOGRAM_MEMORY_KB("GPU.GrContextMemoryKb", resource_bytes / 1000);
}

}  // namespace raster
}  // namespace gpu
