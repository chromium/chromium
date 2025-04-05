// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_SHARED_CONTEXT_H_
#define GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_SHARED_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/gpu/graphite/Context.h"

namespace gpu {

// This is a thread safe wrapper class to skgpu::graphite::Context. In order to
// support multi-threading, locks are used to ensure thread safety in
// skgpu::graphite::Context. All clients need to call the wrapper functions in
// GraphiteSharedContext. Only GraphiteSharedContext can communicate with
// skgpu::graphite::Context directly. If |is_thread_safe| is false, the locks
// are equivalent to no-op.
class GPU_GLES2_EXPORT GraphiteSharedContext
    : public base::RefCountedThreadSafe<GraphiteSharedContext> {
 public:
  GraphiteSharedContext(
      std::unique_ptr<skgpu::graphite::Context> graphite_context,
      bool is_thread_safe);

  GraphiteSharedContext(const GraphiteSharedContext&) = delete;
  GraphiteSharedContext(GraphiteSharedContext&&) = delete;
  GraphiteSharedContext& operator=(const GraphiteSharedContext&) = delete;
  GraphiteSharedContext& operator=(GraphiteSharedContext&&) = delete;

 private:
  friend class base::RefCountedThreadSafe<GraphiteSharedContext>;
  ~GraphiteSharedContext();

  class AutoLock;

  // The lock for protecting skgpu::graphite::Context.
  mutable std::optional<base::Lock> lock_;

  const std::unique_ptr<skgpu::graphite::Context> graphite_context_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_SHARED_CONTEXT_H_
