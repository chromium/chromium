// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_discardable_texture_manager.h"

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"

namespace gpu {

ClientDiscardableTextureManager::TextureEntry::TextureEntry(
    ClientDiscardableHandle::Id id)
    : id(id) {}
ClientDiscardableTextureManager::TextureEntry::TextureEntry(
    const TextureEntry& other) = default;
ClientDiscardableTextureManager::TextureEntry&
ClientDiscardableTextureManager::TextureEntry::operator=(
    const TextureEntry& other) = default;

ClientDiscardableTextureManager::ClientDiscardableTextureManager() = default;
ClientDiscardableTextureManager::~ClientDiscardableTextureManager() = default;

ClientDiscardableHandle ClientDiscardableTextureManager::InitializeTexture(
    CommandBuffer* command_buffer,
    uint32_t texture_id) {
  base::AutoLock hold(lock_);
  DCHECK(!base::Contains(texture_entries_, texture_id));
  ClientDiscardableHandle::Id handle_id =
      discardable_manager_.CreateHandle(command_buffer);
  if (handle_id.is_null())
    return ClientDiscardableHandle();

  // We must have a valid handle here, since the id was generated above and
  // should be in locked state.
  texture_entries_.emplace(texture_id, TextureEntry(handle_id));
  return discardable_manager_.GetHandle(handle_id);
}

bool ClientDiscardableTextureManager::LockTexture(uint32_t texture_id) {
  base::AutoLock hold(lock_);
  auto found = texture_entries_.find(texture_id);
  if (found == texture_entries_.end())
    return false;

  TextureEntry& entry = found->second;
  if (!discardable_manager_.LockHandle(entry.id)) {
    DCHECK_EQ(0u, entry.client_lock_count);
    return false;
  }

  ++entry.client_lock_count;
  return true;
}

void ClientDiscardableTextureManager::UnlockTexture(
    uint32_t texture_id,
    bool* should_unbind_texture) {
  base::AutoLock hold(lock_);
  auto found = texture_entries_.find(texture_id);
  CHECK(found != texture_entries_.end(), base::NotFatalUntil::M130);
  TextureEntry& entry = found->second;
  DCHECK_GT(entry.client_lock_count, 0u);
  --entry.client_lock_count;
  *should_unbind_texture = (0u == entry.client_lock_count);
}

void ClientDiscardableTextureManager::FreeTexture(uint32_t texture_id) {
  base::AutoLock hold(lock_);
  auto found = texture_entries_.find(texture_id);
  if (found == texture_entries_.end())
    return;
  ClientDiscardableHandle::Id discardable_id = found->second.id;
  texture_entries_.erase(found);
  return discardable_manager_.FreeHandle(discardable_id);
}

bool ClientDiscardableTextureManager::TextureIsValid(
    uint32_t texture_id) const {
  base::AutoLock hold(lock_);
  return base::Contains(texture_entries_, texture_id);
}

bool ClientDiscardableTextureManager::TextureIsDeletedForTracing(
    uint32_t texture_id) const {
  base::AutoLock hold(lock_);
  auto found = texture_entries_.find(texture_id);
  if (found == texture_entries_.end())
    return true;
  return discardable_manager_.HandleIsDeletedForTracing(found->second.id);
}

ClientDiscardableHandle ClientDiscardableTextureManager::GetHandleForTesting(
    uint32_t texture_id) {
  base::AutoLock hold(lock_);
  auto found = texture_entries_.find(texture_id);
  CHECK(found != texture_entries_.end());
  return discardable_manager_.GetHandle(found->second.id);
}

}  // namespace gpu
