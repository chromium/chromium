// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_cache_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "gpu/command_buffer/service/graphite_image_provider.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"

namespace gpu::raster {
namespace {
// Any resources not used in the last 5 seconds should be purged.
constexpr base::TimeDelta kResourceNotUsedSinceDelay = base::Seconds(5);

// All unused resources should be purged after an idle time delay of 5 seconds.
constexpr base::TimeDelta kPerformCleanupDelay = base::Seconds(5);
}

GraphiteCacheController::GraphiteCacheController(
    skgpu::graphite::Recorder* recorder,
    skgpu::graphite::Context* context)
    : recorder_(recorder), context_(context) {
  CHECK(recorder_);
  timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, kPerformCleanupDelay,
      base::BindRepeating(&GraphiteCacheController::PerformCleanup,
                          weak_ptr_factory_.GetWeakPtr()));
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
  // Reset the timer, so PerformCleanup() will be called until ScheduleCleanup()
  // is not called for 5 seconds.
  timer_->Reset();
}

void GraphiteCacheController::PerformCleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cleanup all unused resources.
  if (context_) {
    context_->freeGpuResources();
  }
  auto* image_provider =
      static_cast<GraphiteImageProvider*>(recorder_->clientImageProvider());
  image_provider->ClearImageCache();
  recorder_->freeGpuResources();
}

}  // namespace gpu::raster
