// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_batch_access_manager.h"

#include "gpu/command_buffer/service/shared_image_backing_egl_image.h"
#include "ui/gl/shared_gl_fence_egl.h"

namespace gpu {

SharedImageBatchAccessManager::SharedImageBatchAccessManager() = default;
SharedImageBatchAccessManager::~SharedImageBatchAccessManager() = default;

bool SharedImageBatchAccessManager::IsDoingBatchReads() {
  base::AutoLock lock(lock_);

  auto it = backings_.find(gl::g_current_gl_context);
  return (it != backings_.end());
}

void SharedImageBatchAccessManager::RegisterEglBackingForEndReadFence(
    SharedImageBackingEglImage* egl_backing) {
  base::AutoLock lock(lock_);

  auto it = backings_.find(gl::g_current_gl_context);
  DCHECK(it != backings_.end());

  it->second.emplace(egl_backing);
}

void SharedImageBatchAccessManager::UnregisterEglBacking(
    SharedImageBackingEglImage* egl_backing) {
  base::AutoLock lock(lock_);

  // Search this backing on all the contexts since the backing could be
  // destroyed from any context.
  for (auto& it : backings_)
    it.second.erase(egl_backing);
}

bool SharedImageBatchAccessManager::BeginBatchReadAccess() {
  base::AutoLock lock(lock_);

  // On a given context, only one batch access should be active. Hence return
  // false if we already have a context here.
  if (backings_.find(gl::g_current_gl_context) != backings_.end())
    return false;

  backings_.emplace(gl::g_current_gl_context, SetOfBackings());
  return true;
}

bool SharedImageBatchAccessManager::EndBatchReadAccess() {
  base::AutoLock lock(lock_);

  // One batch access should be active on this context from the corresponding
  // BeginBatchReadAccess().
  auto it = backings_.find(gl::g_current_gl_context);
  if (it == backings_.end())
    return false;

  // If there are registered backings, create the egl fence and supply to the
  // backings.
  if (!it->second.empty()) {
    // Create a shared egl fence.
    auto shared_egl_fence = base::MakeRefCounted<gl::SharedGLFenceEGL>();

    // Pass it to all the registered backings.
    for (auto* registered_backing : it->second) {
      registered_backing->SetEndReadFence(shared_egl_fence);
    }
  }

  // Remove the entry for this context.
  backings_.erase(it);
  return true;
}

}  // namespace gpu
