// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_cache_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"

namespace gpu::raster {
namespace {
constexpr base::TimeDelta kCleanupDelay = base::Seconds(5);
}

GraphiteCacheController::GraphiteCacheController(
    skgpu::graphite::Context* context,
    skgpu::graphite::Recorder* recorder)
    : context_(context), recorder_(recorder) {
  timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, kCleanupDelay,
      base::BindRepeating(&GraphiteCacheController::PerformCleanup,
                          GetWeakPtr()));
}

GraphiteCacheController::~GraphiteCacheController() = default;

void GraphiteCacheController::ScheduleCleanup() {
  timer_->Reset();
}

void GraphiteCacheController::PerformCleanup() {
  if (context_) {
    // TODO(crbug.com/1472451): cleanup resources in context_;
  }
  if (recorder_) {
    // TODO(crbug.com/1472451): cleanup resources in recorder_
  }
}

}  // namespace gpu::raster
