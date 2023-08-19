// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_DISCARDABLE_TEXTURE_MANAGER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_DISCARDABLE_TEXTURE_MANAGER_H_

#include <map>

#include "base/synchronization/lock.h"
#include "gpu/command_buffer/client/client_discardable_manager.h"
#include "gpu/gpu_export.h"

namespace gpu {

// A helper class used to manage discardable textures. Makes use of
// ClientDiscardableManager. Used by the GLES2 Implementation.
//
// NOTE: The presence of locking on this class does not make it threadsafe.
// The underlying locking *only* allows calling TextureIsValid,
// LockTexture, and TextureIsDeletedForTracing without holding the GL context
// lock. All other calls still require that the context lock be held.
class GPU_EXPORT ClientDiscardableTextureManager {
 public:
  ClientDiscardableTextureManager();

  ClientDiscardableTextureManager(const ClientDiscardableTextureManager&) =
      delete;
  ClientDiscardableTextureManager& operator=(
      const ClientDiscardableTextureManager&) = delete;

  ~ClientDiscardableTextureManager();
  ClientDiscardableHandle InitializeTexture(CommandBuffer* command_buffer,
                                            uint32_t texture_id);
  bool LockTexture(uint32_t texture_id);
  void UnlockTexture(uint32_t texture_id, bool* should_unbind_texture);
  // Must be called by the GLES2Implementation when a texture is being deleted
  // to allow tracking memory to be reclaimed.
  void FreeTexture(uint32_t texture_id);
  bool TextureIsValid(uint32_t texture_id) const;

  // Tracing only functions.
  bool TextureIsDeletedForTracing(uint32_t texture_id) const;

  // Test only functions.
  ClientDiscardableManager* DiscardableManagerForTesting() {
    return &discardable_manager_;
  }
  ClientDiscardableHandle GetHandleForTesting(uint32_t texture_id);

 private:
  struct TextureEntry {
    TextureEntry(ClientDiscardableHandle::Id id);
    TextureEntry(const TextureEntry& other);
    TextureEntry& operator=(const TextureEntry& other);

    ClientDiscardableHandle::Id id;
    // Tracks the lock count of the given texture. Used to unbind texture
    // the texture when fully unlocked.
    uint32_t client_lock_count = 1;
  };

  mutable base::Lock lock_;
  std::map<uint32_t, TextureEntry> texture_entries_ GUARDED_BY(lock_);
  ClientDiscardableManager discardable_manager_ GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_DISCARDABLE_TEXTURE_MANAGER_H_
