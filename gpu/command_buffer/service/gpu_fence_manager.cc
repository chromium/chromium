// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_fence_manager.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_fence.h"

namespace gpu {
namespace gles2 {

GpuFenceManager::GpuFenceEntry::GpuFenceEntry() = default;

GpuFenceManager::GpuFenceEntry::~GpuFenceEntry() = default;

GpuFenceManager::GpuFenceManager() = default;

GpuFenceManager::~GpuFenceManager() {
  DCHECK(gpu_fence_entries_.empty());
}

bool GpuFenceManager::CreateGpuFence(uint32_t client_id) {
  // This must be a new entry.
  GpuFenceEntryMap::iterator it = gpu_fence_entries_.find(client_id);
  if (it != gpu_fence_entries_.end())
    return false;

  auto entry = std::make_unique<GpuFenceEntry>();
  entry->gl_fence_ = gl::GLFence::CreateForGpuFence();
  if (!entry->gl_fence_)
    return false;
  std::pair<GpuFenceEntryMap::iterator, bool> result =
      gpu_fence_entries_.emplace(client_id, std::move(entry));
  DCHECK(result.second);
  return true;
}

bool GpuFenceManager::CreateGpuFenceFromHandle(uint32_t client_id,
                                               gfx::GpuFenceHandle handle) {
  // The handle must be valid. The fallback kEmpty type cannot be duplicated.
  if (handle.is_null())
    return false;

  // This must be a new entry.
  GpuFenceEntryMap::iterator it = gpu_fence_entries_.find(client_id);
  if (it != gpu_fence_entries_.end())
    return false;

  auto entry = std::make_unique<GpuFenceEntry>();
  entry->fence_handle_ = std::move(handle);

  std::pair<GpuFenceEntryMap::iterator, bool> result =
      gpu_fence_entries_.emplace(client_id, std::move(entry));
  DCHECK(result.second);
  return true;
}

bool GpuFenceManager::IsValidGpuFence(uint32_t client_id) {
  GpuFenceEntryMap::iterator it = gpu_fence_entries_.find(client_id);
  return it != gpu_fence_entries_.end();
}

std::unique_ptr<gfx::GpuFence> GpuFenceManager::GetGpuFence(
    uint32_t client_id) {
  GpuFenceEntryMap::iterator it = gpu_fence_entries_.find(client_id);
  if (it == gpu_fence_entries_.end())
    return nullptr;

  GpuFenceEntry* entry = it->second.get();
  DCHECK(entry->gl_fence_ || !entry->fence_handle_.is_null());
  DCHECK(!entry->gl_fence_ || entry->fence_handle_.is_null());

  if (!entry->fence_handle_.is_null())
    return std::make_unique<gfx::GpuFence>(entry->fence_handle_.Clone());
  return entry->gl_fence_->GetGpuFence();
}

bool GpuFenceManager::GpuFenceServerWait(uint32_t client_id) {
  GpuFenceEntryMap::iterator it = gpu_fence_entries_.find(client_id);
  if (it == gpu_fence_entries_.end())
    return false;

  GpuFenceEntry* entry = it->second.get();
  DCHECK(entry->gl_fence_ || !entry->fence_handle_.is_null());
  DCHECK(!entry->gl_fence_ || entry->fence_handle_.is_null());

  if (!entry->fence_handle_.is_null()) {
    gfx::GpuFence gpu_fence(entry->fence_handle_.Clone());
    auto gl_fence = gl::GLFence::CreateFromGpuFence(gpu_fence);
    if (!gl_fence)
      return false;
    gl_fence->ServerWait();
    return true;
  }

  entry->gl_fence_->ServerWait();
  return true;
}

bool GpuFenceManager::RemoveGpuFence(uint32_t client_id) {
  return gpu_fence_entries_.erase(client_id) == 1u;
}

void GpuFenceManager::Destroy(bool have_context) {
  if (!have_context) {
    // Invalidate fences on context loss. This is a no-op for EGL fences, but
    // other platforms may want this.
    for (auto& it : gpu_fence_entries_) {
      it.second.get()->gl_fence_->Invalidate();
    }
  }
  gpu_fence_entries_.clear();
}

}  // namespace gles2
}  // namespace gpu
