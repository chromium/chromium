// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_queue_lock.h"

#include "ui/gl/gl_angle_util_vulkan.h"

namespace gpu {
VulkanQueueLock::VulkanQueueLock(EGLDisplay display) : display_(display) {}

void VulkanQueueLock::Acquire() {
  if (display_ != nullptr) {
    gl::LockVkQueueInANGLE(display_);
  } else {
    lock_.Acquire();
  }
}

void VulkanQueueLock::Release() {
  if (display_ != nullptr) {
    gl::UnlockVkQueueInANGLE(display_);
  } else {
    lock_.Release();
  }
}
}  // namespace gpu
