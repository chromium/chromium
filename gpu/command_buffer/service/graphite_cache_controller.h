// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_CACHE_CONTROLLER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_CACHE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "gpu/gpu_gles2_export.h"

namespace base {
class RetainingOneShotTimer;
}  // namespace base

namespace skgpu::graphite {
class Context;
class Recorder;
}  // namespace skgpu::graphite

namespace gpu {
class DawnContextProvider;
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
      skgpu::graphite::Context* context = nullptr,
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

  const raw_ptr<skgpu::graphite::Recorder> recorder_;
  const raw_ptr<skgpu::graphite::Context> context_;
  const raw_ptr<DawnContextProvider> dawn_context_provider_;
  std::unique_ptr<base::RetainingOneShotTimer> timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GraphiteCacheController> weak_ptr_factory_{this};
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_CACHE_CONTROLLER_H_
