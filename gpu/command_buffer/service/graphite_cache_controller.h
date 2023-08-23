// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_CACHE_CONTROLLER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_CACHE_CONTROLLER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/gpu_gles2_export.h"

namespace base {
class RetainingOneShotTimer;
}  // namespace base

namespace skgpu::graphite {
class Context;
class Recorder;
}  // namespace skgpu::graphite

namespace gpu::raster {

class GPU_GLES2_EXPORT GraphiteCacheController
    : public base::RefCounted<GraphiteCacheController> {
 public:
  // |context| and |recorder| are optional, GraphiteCacheController only purge
  // resource in non-null |context| and |recorder|.
  GraphiteCacheController(skgpu::graphite::Context* context,
                          skgpu::graphite::Recorder* recorder);
  GraphiteCacheController(const GraphiteCacheController&) = delete;
  GraphiteCacheController& operator=(const GraphiteCacheController&) = delete;

  base::WeakPtr<GraphiteCacheController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Schedule cleanup for the graphite cache, the cleanup will be performed
  // until ScheduleCleanup() is called for a while.
  void ScheduleCleanup();

 private:
  friend class base::RefCounted<GraphiteCacheController>;
  ~GraphiteCacheController();

  void PerformCleanup();

  const raw_ptr<skgpu::graphite::Context> context_;
  const raw_ptr<skgpu::graphite::Recorder> recorder_;
  std::unique_ptr<base::RetainingOneShotTimer> timer_;

  base::WeakPtrFactory<GraphiteCacheController> weak_factory_{this};
};

}  // namespace gpu::raster

#endif  // GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_CACHE_CONTROLLER_H_
