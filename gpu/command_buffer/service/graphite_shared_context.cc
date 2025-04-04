// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_shared_context.h"

namespace gpu {

// Helper class used by subclasses to acquire |lock_| if it exists.
class SCOPED_LOCKABLE GraphiteSharedContext::AutoLock {
  STACK_ALLOCATED();

 public:
  explicit AutoLock(const GraphiteSharedContext* context)
      EXCLUSIVE_LOCK_FUNCTION(context->lock_)
      : auto_lock_(context->lock_ ? &context->lock_.value() : nullptr) {}
  ~AutoLock() UNLOCK_FUNCTION() = default;

  AutoLock(const AutoLock&) = delete;
  AutoLock& operator=(const AutoLock&) = delete;

 private:
  base::AutoLockMaybe auto_lock_;
};

GraphiteSharedContext::GraphiteSharedContext(
    std::unique_ptr<skgpu::graphite::Context> graphite_context,
    bool is_thread_safe)
    : graphite_context_(std::move(graphite_context)) {
  CHECK(graphite_context_);

  if (is_thread_safe) {
    lock_.emplace();
  }
}

GraphiteSharedContext::~GraphiteSharedContext() = default;

}  // namespace gpu
