// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"

namespace gpu {
namespace gles2 {

TEST_F(GLES2DecoderPassthroughTest, TestInitDiscardableTexture) {
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  EXPECT_EQ(
      0u,
      passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
  DoInitializeDiscardableTextureCHROMIUM(kClientTextureId);
  EXPECT_EQ(
      1u,
      passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
}

TEST_F(GLES2DecoderPassthroughTest, TestInitInvalidDiscardableTexture) {
  EXPECT_EQ(
      0u,
      passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
  DoInitializeDiscardableTextureCHROMIUM(0);
  EXPECT_EQ(
      0u,
      passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderPassthroughTest,
       TestInitDiscardableTextureWithInvalidArguments) {
  EXPECT_EQ(
      0u,
      passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());

  // Manually initialize an init command with an invalid buffer.
  {
    cmds::InitializeDiscardableTextureCHROMIUM cmd;
    cmd.Init(kClientTextureId, kInvalidSharedMemoryId, 0);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
    EXPECT_EQ(
        0u,
        passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
  }

  // Manually initialize an init command with an out of bounds offset.
  {
    cmds::InitializeDiscardableTextureCHROMIUM cmd;
    cmd.Init(kClientTextureId, shared_memory_id_, kInvalidSharedMemoryOffset);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
    EXPECT_EQ(
        0u,
        passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
  }

  // Manually initialize an init command with a non-atomic32-aligned offset.
  {
    cmds::InitializeDiscardableTextureCHROMIUM cmd;
    cmd.Init(kClientTextureId, shared_memory_id_, 1);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
    EXPECT_EQ(
        0u,
        passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
  }
}

TEST_F(GLES2DecoderPassthroughTest, TestUnlockDiscardableTexture) {
  ContextGroup* context_group = group();
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  EXPECT_EQ(
      0u,
      passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
  DoInitializeDiscardableTextureCHROMIUM(kClientTextureId);
  EXPECT_TRUE(
      passthrough_discardable_texture_manager()->IsEntryLockedForTesting(
          kClientTextureId, context_group));
  DoUnlockDiscardableTextureCHROMIUM(kClientTextureId);
  EXPECT_FALSE(
      passthrough_discardable_texture_manager()->IsEntryLockedForTesting(
          kClientTextureId, context_group));
}

TEST_F(GLES2DecoderPassthroughTest, TestDeleteDiscardableTexture) {
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  EXPECT_EQ(
      0u,
      passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
  DoInitializeDiscardableTextureCHROMIUM(kClientTextureId);
  EXPECT_EQ(
      1u,
      passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
  DoDeleteTexture(kClientTextureId);
  EXPECT_EQ(
      0u,
      passthrough_discardable_texture_manager()->NumCacheEntriesForTesting());
}

TEST_F(GLES2DecoderPassthroughTest,
       TestDiscardableTextureUnusableWhileUnlocked) {
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  DoInitializeDiscardableTextureCHROMIUM(kClientTextureId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  DoUnlockDiscardableTextureCHROMIUM(kClientTextureId);

  // Texture should be unbound
  GLint bound_texture = 0;
  DoGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_texture, 1);
  EXPECT_NE(kClientTextureId, static_cast<GLuint>(bound_texture));

  // Texture should not exist
  EXPECT_FALSE(DoIsTexture(kClientTextureId));

  DoLockDiscardableTextureCHROMIUM(kClientTextureId);
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderPassthroughTest, DiscardableTextureLimits) {
  // Each texture will be 32x32 RGBA unsigned byte
  constexpr GLsizei texture_dimension_size = 32;
  constexpr size_t texture_size =
      texture_dimension_size * texture_dimension_size * 4;

  // Size textures so that four fit in the discardable manager.
  constexpr size_t texture_count = 4;
  constexpr size_t cache_size_limit = texture_count * texture_size;

  PassthroughDiscardableManager* discardable_manager =
      passthrough_discardable_texture_manager();
  discardable_manager->SetCacheSizeLimitForTesting(cache_size_limit);

  constexpr GLuint client_textures[texture_count] = {1, 2, 3, 4};
  for (size_t i = 0; i < texture_count; i++) {
    GLint client_texture = client_textures[i];
    DoGenTexture(client_texture);
    DoBindTexture(GL_TEXTURE_2D, client_texture);
    DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_dimension_size,
                 texture_dimension_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
    DoInitializeDiscardableTextureCHROMIUM(client_texture);

    // Verify the expected size of the cache based on how many textures have
    // been added
    EXPECT_EQ((i + 1) * texture_size,
              discardable_manager->TotalSizeForTesting());
  }

  // Unlock the 4 inserted textures in a specific order that will determine
  // their eviction order
  constexpr GLuint unlock_order[texture_count] = {3, 1, 2, 4};
  for (size_t i = 0; i < texture_count; i++) {
    GLint client_texture = unlock_order[i];
    DoUnlockDiscardableTextureCHROMIUM(client_texture);
  }

  // Verify that all the textures are still in the cache and it's still at it's
  // limit
  EXPECT_EQ(cache_size_limit, discardable_manager->TotalSizeForTesting());

  constexpr GLuint new_client_textures[texture_count] = {5, 6, 7, 8};
  for (size_t i = 0; i < texture_count; i++) {
    GLint client_texture = new_client_textures[i];

    DoGenTexture(client_texture);
    DoBindTexture(GL_TEXTURE_2D, client_texture);
    DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_dimension_size,
                 texture_dimension_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
    DoInitializeDiscardableTextureCHROMIUM(client_texture);

    // Expect that a texture has been evicted to fit this new texture
    EXPECT_FALSE(discardable_manager->IsEntryTrackedForTesting(unlock_order[i],
                                                               group()));

    // Verify that The cache is still full
    EXPECT_EQ(cache_size_limit, discardable_manager->TotalSizeForTesting());
  }

  // Unlock all the new textures
  for (size_t i = 0; i < texture_count; i++) {
    GLint client_texture = new_client_textures[i];
    DoUnlockDiscardableTextureCHROMIUM(client_texture);
  }

  // Verify that all the textures are still in the cache and it's still at it's
  // limit
  EXPECT_EQ(cache_size_limit, discardable_manager->TotalSizeForTesting());

  // Insert a larger texture that takes up the place of 3 smaller ones
  constexpr GLuint large_client_texture = 9;
  DoGenTexture(large_client_texture);
  DoBindTexture(GL_TEXTURE_2D, large_client_texture);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_dimension_size * 3,
               texture_dimension_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoInitializeDiscardableTextureCHROMIUM(large_client_texture);

  // Only two textures should be tracked now
  EXPECT_EQ(2u, discardable_manager->NumCacheEntriesForTesting());

  // Verify that all the textures are still in the cache and it's still at it's
  // limit
  EXPECT_EQ(cache_size_limit, discardable_manager->TotalSizeForTesting());
}

TEST_F(GLES2DecoderPassthroughTest, DiscardableTextureSizeChanged) {
  PassthroughDiscardableManager* discardable_manager =
      passthrough_discardable_texture_manager();

  constexpr size_t small_texture_dimension = 32;
  constexpr size_t small_texture_size =
      small_texture_dimension * small_texture_dimension * 4;

  constexpr size_t large_texture_dimension = 128;
  constexpr size_t large_texture_size =
      large_texture_dimension * large_texture_dimension * 4;

  // Initialize the texture to the small size
  DoGenTexture(kClientTextureId);
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, small_texture_dimension,
               small_texture_dimension, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoInitializeDiscardableTextureCHROMIUM(kClientTextureId);

  EXPECT_EQ(small_texture_size, discardable_manager->TotalSizeForTesting());

  // Set the texture to a larger size, cache should automatically update to the
  // new size
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, large_texture_dimension,
               large_texture_dimension, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);

  EXPECT_EQ(large_texture_size, discardable_manager->TotalSizeForTesting());
}

TEST_F(GLES2DecoderPassthroughTest, DiscardableTextureOwnershipOnUnlock) {
  PassthroughDiscardableManager* discardable_manager =
      passthrough_discardable_texture_manager();

  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  DoInitializeDiscardableTextureCHROMIUM(kClientTextureId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Lock the texture again (now locked twice)
  EXPECT_TRUE(discardable_manager->LockTexture(kClientTextureId, group()));

  // Unlocking the first time should not unbind
  TexturePassthrough* texture_to_unbind = nullptr;
  EXPECT_TRUE(discardable_manager->UnlockTexture(kClientTextureId, group(),
                                                 &texture_to_unbind));
  EXPECT_EQ(nullptr, texture_to_unbind);

  // Unlocking again will transfer control of the texture to the manager and
  // unbind
  EXPECT_TRUE(discardable_manager->UnlockTexture(kClientTextureId, group(),
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);

  EXPECT_NE(nullptr, discardable_manager->UnlockedTextureForTesting(
                         kClientTextureId, group()));
}

TEST_F(GLES2DecoderPassthroughTest, DiscardableTextureMemoryPressure) {
  // Each texture will be 32x32 RGBA unsigned byte
  constexpr GLsizei texture_dimension_size = 32;
  constexpr size_t texture_size =
      texture_dimension_size * texture_dimension_size * 4;

  // Size textures so that four fit in the discardable manager.
  constexpr size_t texture_count = 4;
  constexpr size_t cache_size_limit = texture_count * texture_size;

  PassthroughDiscardableManager* discardable_manager =
      passthrough_discardable_texture_manager();
  discardable_manager->SetCacheSizeLimitForTesting(cache_size_limit);

  constexpr GLuint client_textures[texture_count] = {1, 2, 3, 4};
  for (size_t i = 0; i < texture_count; i++) {
    GLint client_texture = client_textures[i];
    DoGenTexture(client_texture);
    DoBindTexture(GL_TEXTURE_2D, client_texture);
    DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_dimension_size,
                 texture_dimension_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
    DoInitializeDiscardableTextureCHROMIUM(client_texture);
  }

  // The cache should be at its limit
  EXPECT_EQ(cache_size_limit, discardable_manager->TotalSizeForTesting());
  EXPECT_EQ(texture_count, discardable_manager->NumCacheEntriesForTesting());

  // A memory pressure call should have no impact, as all textures are locked.
  discardable_manager->HandleMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(cache_size_limit, discardable_manager->TotalSizeForTesting());
  EXPECT_EQ(texture_count, discardable_manager->NumCacheEntriesForTesting());

  // Unlock one texture, should still have no change to the cache
  DoUnlockDiscardableTextureCHROMIUM(client_textures[2]);
  EXPECT_EQ(cache_size_limit, discardable_manager->TotalSizeForTesting());
  EXPECT_EQ(texture_count, discardable_manager->NumCacheEntriesForTesting());

  // Send memory pressure critical again - this should delete the unlocked
  // texture, but not the others.
  discardable_manager->HandleMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(discardable_manager->IsEntryTrackedForTesting(client_textures[2],
                                                             group()));
  EXPECT_EQ(cache_size_limit - texture_size,
            discardable_manager->TotalSizeForTesting());
  EXPECT_EQ(texture_count - 1,
            discardable_manager->NumCacheEntriesForTesting());

  // Unlock the remaining textures
  DoUnlockDiscardableTextureCHROMIUM(client_textures[0]);
  DoUnlockDiscardableTextureCHROMIUM(client_textures[1]);
  DoUnlockDiscardableTextureCHROMIUM(client_textures[3]);

  // Send memory pressure moderate - this should delete all but one texture
  // (cache is capped at 1/4 size).
  discardable_manager->HandleMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(discardable_manager->IsEntryTrackedForTesting(client_textures[0],
                                                             group()));
  EXPECT_FALSE(discardable_manager->IsEntryTrackedForTesting(client_textures[1],
                                                             group()));
  EXPECT_EQ(texture_size, discardable_manager->TotalSizeForTesting());
  EXPECT_EQ(1u, discardable_manager->NumCacheEntriesForTesting());

  // Send memory pressure critical again - this should delete the remaining
  // textures.
  discardable_manager->HandleMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(discardable_manager->IsEntryTrackedForTesting(client_textures[3],
                                                             group()));
  EXPECT_EQ(0u, discardable_manager->TotalSizeForTesting());
  EXPECT_EQ(0u, discardable_manager->NumCacheEntriesForTesting());
}

TEST_F(GLES2DecoderPassthroughTest,
       DiscardableTextureBindGeneratedTextureLock) {
  PassthroughDiscardableManager* discardable_manager =
      passthrough_discardable_texture_manager();
  PassthroughResources* resources = group()->passthrough_resources();

  // Create and insert a new texture.
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  DoInitializeDiscardableTextureCHROMIUM(kClientTextureId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  GLuint service_id = 0;
  EXPECT_TRUE(
      resources->texture_id_map.GetServiceID(kClientTextureId, &service_id));

  // Unlock the texture, the discardable manager should take ownership of the
  // texture
  DoUnlockDiscardableTextureCHROMIUM(kClientTextureId);

  // Generate a new texture for the given client id, similar to "bind generates
  // resource" behavior.
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);

  GLuint generated_service_id = 0;
  EXPECT_TRUE(resources->texture_id_map.GetServiceID(kClientTextureId,
                                                     &generated_service_id));
  EXPECT_NE(service_id, generated_service_id);

  // Re-lock the texture, the original texture should be deleted and the
  // generated one should still exist
  DoLockDiscardableTextureCHROMIUM(kClientTextureId);

  GLuint final_service_id = 0;
  EXPECT_TRUE(resources->texture_id_map.GetServiceID(kClientTextureId,
                                                     &final_service_id));
  EXPECT_EQ(generated_service_id, final_service_id);

  GLuint client_id = 0;
  EXPECT_FALSE(resources->texture_id_map.GetClientID(service_id, &client_id));

  // Delete the texture, it should also be removed from the discardable manager
  DoDeleteTexture(kClientTextureId);
  EXPECT_EQ(0u, discardable_manager->NumCacheEntriesForTesting());
}

TEST_F(GLES2DecoderPassthroughTest,
       DiscardableTextureBindGeneratedTextureSizeChange) {
  PassthroughDiscardableManager* discardable_manager =
      passthrough_discardable_texture_manager();

  constexpr size_t small_texture_dimension = 32;
  constexpr size_t small_texture_size =
      small_texture_dimension * small_texture_dimension * 4;

  constexpr size_t large_texture_dimension = 128;
  constexpr size_t large_texture_size =
      large_texture_dimension * large_texture_dimension * 4;

  // Initialize the texture to the small size
  DoGenTexture(kClientTextureId);
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, small_texture_dimension,
               small_texture_dimension, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoInitializeDiscardableTextureCHROMIUM(kClientTextureId);

  EXPECT_EQ(small_texture_size, discardable_manager->TotalSizeForTesting());

  // Unlock the texture
  DoUnlockDiscardableTextureCHROMIUM(kClientTextureId);

  // Generate a new texture for the given client id, similar to "bind generates
  // resource" behavior.
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);

  // Re-size the generated texture. The tracked size should update.
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, large_texture_dimension,
               large_texture_dimension, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  EXPECT_EQ(large_texture_size, discardable_manager->TotalSizeForTesting());
}

TEST_F(GLES2DecoderPassthroughTest,
       DiscardableTextureBindGeneratedTextureSizeChangeCubeMap) {
  PassthroughDiscardableManager* discardable_manager =
      passthrough_discardable_texture_manager();

  constexpr size_t texture_dimension = 32;
  constexpr size_t texture_size = texture_dimension * texture_dimension * 4;

  // Initialize each face of the texture and check the tracked size
  DoGenTexture(kClientTextureId);
  DoBindTexture(GL_TEXTURE_CUBE_MAP, kClientTextureId);
  DoInitializeDiscardableTextureCHROMIUM(kClientTextureId);
  for (size_t face_idx = 0; face_idx < 6; face_idx++) {
    DoTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_idx, 0, GL_RGBA,
                 texture_dimension, texture_dimension, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, 0, 0);
    EXPECT_EQ(texture_size * (face_idx + 1),
              discardable_manager->TotalSizeForTesting());
  }
}

}  // namespace gles2
}  // namespace gpu
