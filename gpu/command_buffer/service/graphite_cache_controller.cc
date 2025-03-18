// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_cache_controller.h"

#include <atomic>

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/graphite_image_provider.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

namespace gpu::raster {
namespace {
// Any resources not used in the last 5 seconds should be purged.
constexpr base::TimeDelta kResourceNotUsedSinceDelay = base::Seconds(5);

// All unused resources should be purged after an idle time delay of 1 seconds.
constexpr base::TimeDelta kCleanUpAllResourcesDelay = base::Seconds(1);

// Global atomic idle id shared between all instances of this class. There are
// multiple GraphiteCacheControllers used from gpu/viz threads and we want to
// defer cleanup until all those threads are idle.
std::atomic<uint32_t> g_current_idle_id = 0;

}  // namespace

GraphiteCacheController::GraphiteCacheController(
    skgpu::graphite::Recorder* recorder,
    skgpu::graphite::Context* context,
    DawnContextProvider* dawn_context_provider)
    : recorder_(recorder),
      context_(context),
      dawn_context_provider_(dawn_context_provider) {
  CHECK(recorder_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GraphiteCacheController::~GraphiteCacheController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GraphiteCacheController::ScheduleCleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (context_) {
    context_->performDeferredCleanup(
        std::chrono::seconds(kResourceNotUsedSinceDelay.InSeconds()));
  }
  auto* image_provider =
      static_cast<GraphiteImageProvider*>(recorder_->clientImageProvider());
  image_provider->PurgeImagesNotUsedSince(kResourceNotUsedSinceDelay);
  recorder_->performDeferredCleanup(
      std::chrono::seconds(kResourceNotUsedSinceDelay.InSeconds()));

  uint32_t idle_id = ++g_current_idle_id;

  if (idle_cleanup_cb_.IsCancelled()) {
    ScheduleCleanUpAllResources(idle_id);
  }
}

void GraphiteCacheController::CleanUpScratchResources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (context_) {
    context_->freeGpuResources();
  }
  recorder_->freeGpuResources();
}

void GraphiteCacheController::CleanUpAllResources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  idle_cleanup_cb_.Cancel();
  CleanUpAllResourcesImpl();
}

void GraphiteCacheController::ScheduleCleanUpAllResources(uint32_t idle_id) {
  idle_cleanup_cb_.Reset(
      base::BindOnce(&GraphiteCacheController::MaybeCleanUpAllResources,
                     weak_ptr_factory_.GetWeakPtr(), idle_id));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, idle_cleanup_cb_.callback(), kCleanUpAllResourcesDelay);
}

void GraphiteCacheController::MaybeCleanUpAllResources(
    uint32_t posted_idle_id) {
  idle_cleanup_cb_.Cancel();

  if (posted_idle_id != g_current_idle_id) {
    // If `g_current_idle_id` has changed since this task was posted then the
    // GPU process has not been idle. Check again after another delay.
    ScheduleCleanUpAllResources(g_current_idle_id);
    return;
  }

  CleanUpAllResourcesImpl();
}

void GraphiteCacheController::CleanUpAllResourcesImpl() {
  auto* image_provider =
      static_cast<GraphiteImageProvider*>(recorder_->clientImageProvider());
  image_provider->ClearImageCache();

  CleanUpScratchResources();

#if BUILDFLAG(SKIA_USE_DAWN)
  if (dawn_context_provider_) {
    if (dawn::native::ReduceMemoryUsage(
            dawn_context_provider_->GetDevice().Get())) {
      // There is scheduled work on the GPU that must complete before finishing
      // cleanup.
      ScheduleCleanUpAllResources(g_current_idle_id);
    }
    dawn::native::PerformIdleTasks(dawn_context_provider_->GetDevice());
  }
#endif
}

}  // namespace gpu::raster
