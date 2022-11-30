// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class SetAggressivelyFreeResourcesTest : public testing::Test {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    gl_.Initialize(options);
    if (!gl_.IsInitialized()) {
      options.context_type = CONTEXT_TYPE_OPENGLES2;
      gl_.Initialize(options);
    }
    context_type_ = options.context_type;

    // Make sure we start with a clean slate.
    gl_.gles2_implementation()->FreeEverything();
    EXPECT_EQ(0u, gl_.GetSharedMemoryBytesAllocated());
  }

  void TearDown() override { gl_.Destroy(); }

  GLManager gl_;
  ContextType context_type_ = CONTEXT_TYPE_OPENGLES3;
};

// Tests that SetAggressivelyFreeResources releases command buffer memory.
TEST_F(SetAggressivelyFreeResourcesTest, FreeAllMemory_CommandBuffer) {
  GLuint texture = 0;
  EXPECT_EQ(0u, gl_.GetSharedMemoryBytesAllocated());
  // Basic command that just uses command buffer.
  glGenTextures(1, &texture);
  EXPECT_LT(0u, gl_.GetSharedMemoryBytesAllocated());

  gl_.gles2_implementation()->SetAggressivelyFreeResources(true);
  EXPECT_EQ(0u, gl_.GetSharedMemoryBytesAllocated());
}

// Tests that SetAggressivelyFreeResources releases transfer buffer memory.
TEST_F(SetAggressivelyFreeResourcesTest, FreeAllMemory_TransferBuffer) {
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  const char kPixels[4 * 4 * 4] = {0};
  // Allocates transfer buffer space for the pixels.
  size_t old_size = gl_.GetSharedMemoryBytesAllocated();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               kPixels);
  EXPECT_LT(old_size, gl_.GetSharedMemoryBytesAllocated());

  gl_.gles2_implementation()->SetAggressivelyFreeResources(true);
  EXPECT_EQ(0u, gl_.GetSharedMemoryBytesAllocated());
}

// Tests that SetAggressivelyFreeResources releases mapped memory.
TEST_F(SetAggressivelyFreeResourcesTest, FreeAllMemory_MappedMemory) {
  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  const char kData[256] = {0};
  glBufferData(GL_ARRAY_BUFFER, sizeof(kData), kData, GL_STATIC_DRAW);

  size_t old_size = gl_.GetSharedMemoryBytesAllocated();
  // Allocates mapped memory for data.
  void* data = glMapBufferSubDataCHROMIUM(GL_ARRAY_BUFFER, 0, sizeof(kData),
                                          GL_WRITE_ONLY);
  ASSERT_TRUE(data);
  memcpy(data, kData, sizeof(kData));
  glUnmapBufferSubDataCHROMIUM(data);
  EXPECT_LT(old_size, gl_.GetSharedMemoryBytesAllocated());

  gl_.gles2_implementation()->SetAggressivelyFreeResources(true);
  EXPECT_EQ(0u, gl_.GetSharedMemoryBytesAllocated());
}

// Tests that SetAggressivelyFreeResources releases QuerySyncs.
TEST_F(SetAggressivelyFreeResourcesTest, FreeAllMemory_Queries) {
  GLuint query = 0;
  glGenQueriesEXT(1, &query);

  size_t old_size = gl_.GetSharedMemoryBytesAllocated();
  // Allocates a QuerySync.
  glBeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, query);
  glEndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
  glDeleteQueriesEXT(1, &query);
  EXPECT_LT(old_size, gl_.GetSharedMemoryBytesAllocated());

  gl_.gles2_implementation()->SetAggressivelyFreeResources(true);
  EXPECT_EQ(0u, gl_.GetSharedMemoryBytesAllocated());
}

// Tests that SetAggressivelyFreeResources releases all types of shared memory.
TEST_F(SetAggressivelyFreeResourcesTest, FreeAllMemory) {
  GLuint query = 0;
  glGenQueriesEXT(1, &query);
  // Allocates a QuerySync.
  glBeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, query);

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  const char kPixels[4 * 4 * 4] = {0};
  // Allocates transfer buffer space for the pixels.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               kPixels);

  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  const char kData[256] = {0};
  // Allocates transfer buffer space for kData.
  glBufferData(GL_ARRAY_BUFFER, sizeof(kData), kData, GL_STATIC_DRAW);

  // Allocates mapped memory for data.
  void* data = glMapBufferSubDataCHROMIUM(GL_ARRAY_BUFFER, 0, sizeof(kData),
                                          GL_WRITE_ONLY);
  ASSERT_TRUE(data);
  memcpy(data, kData, sizeof(kData));
  glUnmapBufferSubDataCHROMIUM(data);

  glEndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
  glDeleteQueriesEXT(1, &query);

  EXPECT_LT(0u, gl_.GetSharedMemoryBytesAllocated());
  gl_.gles2_implementation()->SetAggressivelyFreeResources(true);
  EXPECT_EQ(0u, gl_.GetSharedMemoryBytesAllocated());
}

}  // namespace gpu
