// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_QUEUE_LOCK_H_
#define GPU_VULKAN_VULKAN_QUEUE_LOCK_H_

#include "base/component_export.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace gpu {

// A wrapper around eglLockVulkanQueueANGLE and eglUnlockVulkanQueueANGLE to
// make it look like base::Lock.  When VulkanFromANGLE is enabled, those EGL
// calls are made if locking is necessary. Otherwise a base::Lock is used.
class COMPONENT_EXPORT(VULKAN) LOCKABLE VulkanQueueLock {
 public:
  VulkanQueueLock(const VulkanQueueLock&) = delete;
  VulkanQueueLock& operator=(const VulkanQueueLock&) = delete;

  VulkanQueueLock() = default;

  // EGLDisplay is a void *, that's defined here to avoid having to include the
  // EGL headers in this file.
  using EGLDisplay = void*;
  explicit VulkanQueueLock(EGLDisplay display);
  ~VulkanQueueLock() = default;

  void Acquire() EXCLUSIVE_LOCK_FUNCTION();
  void Release() UNLOCK_FUNCTION();

  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {
    if (display_ == nullptr) {
      lock_.AssertAcquired();
    }
  }

 private:
  // If display_ is set (it is not EGL_NO_DISPLAY),
  // eglLockVulkanQueueANGLE/eglUnlockVulkanQueueANGLE will be used for locking.
  // Otherwise lock_ will be used.
  EGLDisplay display_;
  base::Lock lock_;
};

class SCOPED_LOCKABLE VulkanQueueAutoLockMaybe {
  STACK_ALLOCATED();

 public:
  explicit VulkanQueueAutoLockMaybe(VulkanQueueLock* lock)
      EXCLUSIVE_LOCK_FUNCTION(lock)
      : lock_(lock) {
    if (lock_) {
      lock_->Acquire();
    }
  }

  VulkanQueueAutoLockMaybe(const VulkanQueueAutoLockMaybe&) = delete;
  VulkanQueueAutoLockMaybe& operator=(const VulkanQueueAutoLockMaybe&) = delete;

  ~VulkanQueueAutoLockMaybe() UNLOCK_FUNCTION() {
    if (lock_) {
      lock_->AssertAcquired();
      lock_->Release();
    }
  }

 private:
  VulkanQueueLock* const lock_;
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_QUEUE_LOCK_H_
