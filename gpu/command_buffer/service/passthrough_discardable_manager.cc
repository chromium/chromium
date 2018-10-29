// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/passthrough_discardable_manager.h"

#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"

namespace gpu {

PassthroughDiscardableManager::DiscardableCacheValue::DiscardableCacheValue() =
    default;
PassthroughDiscardableManager::DiscardableCacheValue::DiscardableCacheValue(
    const DiscardableCacheValue& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue::DiscardableCacheValue(
    DiscardableCacheValue&& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue&
PassthroughDiscardableManager::DiscardableCacheValue::operator=(
    const DiscardableCacheValue& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue&
PassthroughDiscardableManager::DiscardableCacheValue::operator=(
    DiscardableCacheValue&& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue::~DiscardableCacheValue() =
    default;

PassthroughDiscardableManager::PassthroughDiscardableManager()
    : cache_(DiscardableCache::NO_AUTO_EVICT),
      cache_size_limit_(DiscardableCacheSizeLimit()) {}

PassthroughDiscardableManager::~PassthroughDiscardableManager() {
  DCHECK(cache_.empty());
}

void PassthroughDiscardableManager::InitializeTexture(
    uint32_t client_id,
    const gles2::ContextGroup* context_group,
    size_t texture_size,
    ServiceDiscardableHandle handle) {
  DCHECK(cache_.Get({client_id, context_group}) == cache_.end());

  total_size_ += texture_size;

  DiscardableCacheValue entry;
  entry.handle = std::move(handle);
  entry.size = texture_size;

  cache_.Put({client_id, context_group}, std::move(entry));
  EnforceCacheSizeLimit(cache_size_limit_);
}
bool PassthroughDiscardableManager::UnlockTexture(
    uint32_t client_id,
    const gles2::ContextGroup* context_group,
    gles2::TexturePassthrough** texture_to_unbind) {
  *texture_to_unbind = nullptr;

  auto iter = cache_.Get({client_id, context_group});
  if (iter == cache_.end())
    return false;

  iter->second.handle.Unlock();
  DCHECK(iter->second.lock_count > 0);
  if (--iter->second.lock_count == 0) {
    // Get the texture from this context group and store it.  Remove it from the
    // ID maps and tell the decoder to unbind it.
    gles2::PassthroughResources* resources =
        context_group->passthrough_resources();
    resources->texture_object_map.GetServiceID(client_id,
                                               &iter->second.unlocked_texture);
    resources->texture_id_map.RemoveClientID(client_id);
    resources->texture_object_map.RemoveClientID(client_id);
    *texture_to_unbind = iter->second.unlocked_texture.get();
  }

  return true;
}

bool PassthroughDiscardableManager::LockTexture(
    uint32_t client_id,
    const gles2::ContextGroup* context_group) {
  auto iter = cache_.Get({client_id, context_group});
  if (iter == cache_.end())
    return false;

  ++iter->second.lock_count;
  if (iter->second.unlocked_texture != nullptr) {
    scoped_refptr<gles2::TexturePassthrough> texture =
        std::move(iter->second.unlocked_texture);

    // Re-insert the texture back into the context group's ID maps.
    // If we've generated a replacement texture due to "bind generates
    // resource", behavior, just delete the resource being returned.
    gles2::PassthroughResources* resources =
        context_group->passthrough_resources();
    GLuint service_id = 0;
    if (!resources->texture_id_map.GetServiceID(client_id, &service_id)) {
      resources->texture_id_map.SetIDMapping(client_id, texture->service_id());
      resources->texture_object_map.SetIDMapping(client_id, std::move(texture));
    }
  }

  return true;
}

void PassthroughDiscardableManager::DeleteContextGroup(
    const gles2::ContextGroup* context_group) {
  auto iter = cache_.begin();
  while (iter != cache_.end()) {
    if (iter->first.second == context_group) {
      iter->second.handle.ForceDelete();
      total_size_ -= iter->second.size;
      iter = cache_.Erase(iter);
    } else {
      iter++;
    }
  }
}

void PassthroughDiscardableManager::DeleteTexture(
    uint32_t client_id,
    const gles2::ContextGroup* context_group) {
  auto iter = cache_.Get({client_id, context_group});
  if (iter == cache_.end())
    return;

  iter->second.handle.ForceDelete();
  total_size_ -= iter->second.size;
  cache_.Erase(iter);
}

void PassthroughDiscardableManager::UpdateTextureSize(
    uint32_t client_id,
    const gles2::ContextGroup* context_group,
    size_t new_size) {
  auto iter = cache_.Get({client_id, context_group});
  if (iter == cache_.end())
    return;

  total_size_ -= iter->second.size;
  iter->second.size = new_size;
  total_size_ += iter->second.size;

  EnforceCacheSizeLimit(cache_size_limit_);
}

void PassthroughDiscardableManager::HandleMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  size_t limit = DiscardableCacheSizeLimitForPressure(cache_size_limit_,
                                                      memory_pressure_level);
  EnforceCacheSizeLimit(limit);
}

bool PassthroughDiscardableManager::IsEntryLockedForTesting(
    uint32_t client_id,
    const gles2::ContextGroup* context_group) const {
  auto iter = cache_.Peek({client_id, context_group});
  DCHECK(iter != cache_.end());
  return iter->second.unlocked_texture == nullptr;
}

bool PassthroughDiscardableManager::IsEntryTrackedForTesting(
    uint32_t client_id,
    const gles2::ContextGroup* context_group) const {
  return cache_.Peek({client_id, context_group}) != cache_.end();
}

scoped_refptr<gles2::TexturePassthrough>
PassthroughDiscardableManager::UnlockedTextureForTesting(
    uint32_t client_id,
    const gles2::ContextGroup* context_group) const {
  auto iter = cache_.Peek({client_id, context_group});
  DCHECK(iter != cache_.end());
  return iter->second.unlocked_texture;
}

void PassthroughDiscardableManager::EnforceCacheSizeLimit(size_t limit) {
  for (auto it = cache_.rbegin(); it != cache_.rend();) {
    if (total_size_ <= limit) {
      return;
    }
    if (!it->second.handle.Delete()) {
      ++it;
      continue;
    }

    total_size_ -= it->second.size;

    GLuint client_id = it->first.first;
    const gles2::ContextGroup* context_group = it->first.second;
    gles2::PassthroughResources* resources =
        context_group->passthrough_resources();

    resources->texture_id_map.RemoveClientID(client_id);
    resources->texture_object_map.RemoveClientID(client_id);

    // Erase before calling texture_manager->RemoveTexture, to avoid attempting
    // to remove the texture from entries_ twice.
    it = cache_.Erase(it);
  }
}

}  // namespace gpu
