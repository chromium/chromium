// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class DiscardableTextureTest : public testing::Test {
 protected:
  void SetUp() override { gl_.Initialize(GLManager::Options()); }

  void TearDown() override { gl_.Destroy(); }

  void SetCacheSizeLimitForTesting(size_t cache_size_limit) {
    gl_.discardable_manager()->SetCacheSizeLimitForTesting(cache_size_limit);
    gl_.passthrough_discardable_manager()->SetCacheSizeLimitForTesting(
        cache_size_limit);
  }

  size_t DiscardableManagerTotalSizeForTesting() {
    size_t discardable_manager_size =
        gl_.discardable_manager()->TotalSizeForTesting();
    size_t passthough_discardable_manager_size =
        gl_.passthrough_discardable_manager()->TotalSizeForTesting();

    // Only one discardable manager is in use at a time
    EXPECT_TRUE(discardable_manager_size == 0 ||
                passthough_discardable_manager_size == 0);

    return discardable_manager_size + passthough_discardable_manager_size;
  }

  size_t DiscardableManagerNumCacheEntriesForTesting() {
    size_t discardable_manager_entries =
        gl_.discardable_manager()->NumCacheEntriesForTesting();
    size_t passthough_discardable_manager_entries =
        gl_.passthrough_discardable_manager()->NumCacheEntriesForTesting();

    // Only one discardable manager is in use at a time
    EXPECT_TRUE(discardable_manager_entries == 0 ||
                passthough_discardable_manager_entries == 0);

    return discardable_manager_entries + passthough_discardable_manager_entries;
  }

  GLManager gl_;
};

TEST_F(DiscardableTextureTest, BasicUsage) {
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  gl_.gles2_implementation()->InitializeDiscardableTextureCHROMIUM(texture);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  EXPECT_GT(DiscardableManagerTotalSizeForTesting(), 0u);

  gl_.gles2_implementation()->UnlockDiscardableTextureCHROMIUM(texture);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(DiscardableTextureTest, Limits) {
  // Each texture will be 32x32 RGBA unsigned byte
  constexpr GLsizei texture_dimension_size = 32;
  constexpr size_t texture_size =
      texture_dimension_size * texture_dimension_size * 4;

  // Set a size limit that should only fit one texture
  constexpr size_t cache_size_limit = texture_size + 10;
  SetCacheSizeLimitForTesting(cache_size_limit);

  constexpr size_t texture_count = 2;
  GLuint textures[texture_count] = {};
  glGenTextures(texture_count, textures);
  for (size_t i = 0; i < texture_count; i++) {
    glBindTexture(GL_TEXTURE_2D, textures[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_dimension_size,
                 texture_dimension_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    GLTestHelper::CheckGLError("no errors", __LINE__);

    gl_.gles2_implementation()->InitializeDiscardableTextureCHROMIUM(
        textures[i]);
    GLTestHelper::CheckGLError("no errors", __LINE__);

    EXPECT_EQ(i + 1, DiscardableManagerNumCacheEntriesForTesting());
  }

  // Make sure the discardable manager has gone over it's cache size limit
  EXPECT_GT(DiscardableManagerTotalSizeForTesting(), cache_size_limit);

  // Unlock the textures, verify that they are still tracked
  for (size_t i = 0; i < texture_count; i++) {
    gl_.gles2_implementation()->UnlockDiscardableTextureCHROMIUM(textures[i]);
    GLTestHelper::CheckGLError("no errors", __LINE__);

    EXPECT_EQ(texture_count, DiscardableManagerNumCacheEntriesForTesting());
  }

  // Add a new, large texture the cache and verify that the original textures
  // have been removed
  constexpr GLsizei large_texture_dimension = texture_dimension_size * 2;
  GLuint large_texture = 0;
  glGenTextures(1, &large_texture);
  glBindTexture(GL_TEXTURE_2D, large_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, large_texture_dimension,
               large_texture_dimension, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  gl_.gles2_implementation()->InitializeDiscardableTextureCHROMIUM(
      large_texture);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  EXPECT_EQ(1u, DiscardableManagerNumCacheEntriesForTesting());
}

TEST_F(DiscardableTextureTest, CompressedTexImage2D) {
  if (!GLTestHelper::HasExtension("GL_ANGLE_texture_compression_dxt1")) {
    LOG(INFO)
        << "GL_ANGLE_texture_compression_dxt1 not supported. Skipping test...";
    return;
  }

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  constexpr GLubyte data[8] = {};
  glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4,
                         4, 0, 8, data);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  gl_.gles2_implementation()->InitializeDiscardableTextureCHROMIUM(texture);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  EXPECT_GT(DiscardableManagerTotalSizeForTesting(), 0u);

  gl_.gles2_implementation()->UnlockDiscardableTextureCHROMIUM(texture);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(DiscardableTextureTest, TexStorage2D) {
  if (!GLTestHelper::HasExtension("GL_EXT_texture_storage")) {
    LOG(INFO) << "GL_EXT_texture_storage not supported. Skipping test...";
    return;
  }

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_ALPHA8_EXT, 1, 1);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  gl_.gles2_implementation()->InitializeDiscardableTextureCHROMIUM(texture);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  EXPECT_GT(DiscardableManagerTotalSizeForTesting(), 0u);

  gl_.gles2_implementation()->UnlockDiscardableTextureCHROMIUM(texture);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(DiscardableTextureTest, CopyTexImage2D) {
  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  GLuint textures[2] = {};
  glGenTextures(2, textures);

  glBindTexture(GL_TEXTURE_2D, textures[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         textures[0], 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glBindTexture(GL_TEXTURE_2D, textures[1]);
  glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1, 1, 0);
  gl_.gles2_implementation()->InitializeDiscardableTextureCHROMIUM(textures[1]);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  EXPECT_GT(DiscardableManagerTotalSizeForTesting(), 0u);

  gl_.gles2_implementation()->UnlockDiscardableTextureCHROMIUM(textures[1]);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

}  // namespace gpu
