// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_cache_controller.h"

#include <atomic>

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/graphite_image_provider.h"
#include "gpu/command_buffer/service/graphite_shared_context.h"
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
// multiple GraphiteCacheControllers used from gpu threads and we want to defer
// cleanup until all those threads are idle.
std::atomic<uint32_t> g_current_idle_id = 0;

}  // namespace

GraphiteCacheController::GraphiteCacheController(
    skgpu::graphite::Recorder* recorder,
    bool can_handle_context_resources,
    DawnContextProvider* dawn_context_provider)
    : recorder_(recorder),
      dawn_context_provider_(dawn_context_provider),
#if BUILDFLAG(SKIA_USE_DAWN)
      can_handle_context_resources_(can_handle_context_resources &&
                                    dawn_context_provider) {
#else
      can_handle_context_resources_(false) {
#endif

  CHECK(recorder_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GraphiteCacheController::~GraphiteCacheController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GraphiteCacheController::ScheduleCleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (can_handle_context_resources_) {
    GetGraphiteSharedContext()->performDeferredCleanup(
        std::chrono::seconds(kResourceNotUsedSinceDelay.InSeconds()));
  }
  auto* image_provider =
      static_cast<GraphiteImageProvider*>(recorder_->clientImageProvider());
  image_provider->PurgeImagesNotUsedSince(kResourceNotUsedSinceDelay);
  recorder_->performDeferredCleanup(
      std::chrono::seconds(kResourceNotUsedSinceDelay.InSeconds()));

  if (UseGlobalIdleId()) {
    g_current_idle_id.fetch_add(1, std::memory_order_relaxed);
  } else {
    ++local_idle_id_;
  }

  if (idle_cleanup_cb_.IsCancelled()) {
    ScheduleCleanUpAllResources(GetIdleId());
  }
}

void GraphiteCacheController::CleanUpScratchResources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (can_handle_context_resources_) {
    GetGraphiteSharedContext()->freeGpuResources();
  }
  recorder_->freeGpuResources();
}

void GraphiteCacheController::CleanUpAllResources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  idle_cleanup_cb_.Cancel();
  CleanUpAllResourcesImpl();
}

bool GraphiteCacheController::UseGlobalIdleId() const {
  return dawn_context_provider_ != nullptr;
}

uint32_t GraphiteCacheController::GetIdleId() const {
  return UseGlobalIdleId() ? g_current_idle_id.load(std::memory_order_relaxed)
                           : local_idle_id_;
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

  uint32_t current_idle_id = GetIdleId();
  if (posted_idle_id != current_idle_id) {
    // If `GetIdleId()` has changed since this task was posted then the
    // GPU process has not been idle. Check again after another delay.
    ScheduleCleanUpAllResources(current_idle_id);
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
  if (can_handle_context_resources_) {
    if (dawn::native::ReduceMemoryUsage(
            dawn_context_provider_->GetDevice().Get())) {
      // There is scheduled work on the GPU that must complete before finishing
      // cleanup. Schedule cleanup to run again after a delay.
      ScheduleCleanUpAllResources(GetIdleId());
    }
    dawn::native::PerformIdleTasks(dawn_context_provider_->GetDevice());
  }
#endif
}

GraphiteSharedContext* GraphiteCacheController::GetGraphiteSharedContext() {
#if BUILDFLAG(SKIA_USE_DAWN)
  if (dawn_context_provider_) {
    return dawn_context_provider_->GetGraphiteSharedContext();
  }
#endif
  return nullptr;
}

}  // namespace gpu::raster
