// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GLES3VertexArraysTest : public testing::Test {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    gl_.Initialize(options);
  }

  void TearDown() override { gl_.Destroy(); }
  bool IsApplicable() const { return gl_.IsInitialized(); }

  GLManager gl_;
};

// Test that deleting a bound vertex array object correctly marks the
// bound element array buffer dirty, preventing UAF when the buffer is
// deleted and subsequently unmapped.
TEST_F(GLES3VertexArraysTest, GenAndDeleteOES) {
  if (!IsApplicable()) {
    return;
  }

  if (!GLTestHelper::HasExtension("GL_OES_vertex_array_object")) {
    return;
  }

  constexpr GLsizeiptr kBufA = 16 * 1024 * 1024;
  GLuint buf[2] = {0u, 0u};
  GLuint vao = 0u;

  glGenBuffers(1, &buf[0]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[0]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, kBufA, nullptr, GL_DYNAMIC_DRAW);

  glGenVertexArraysOES(1, &vao);
  glBindVertexArrayOES(vao);

  glGenBuffers(1, &buf[1]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);

  // Delete the VAO. The driver should revert the ELEMENT_ARRAY binding to
  // buf[0]. The service side passthrough decoder should mark the element array
  // binding dirty.
  glDeleteVertexArraysOES(1, &vao);

  // Driver: VAO->VAO0, EA reverts to buf[0].
  // If bug is present: Decoder cache still thinks buf[1] is bound.

  // Post-CL-7782484 trigger: bind buf[1] to COPY_READ_BUFFER and allocate it,
  // so it is ElementArray-typed and we can use it.
  glBindBuffer(GL_COPY_READ_BUFFER, buf[1]);
  glBufferData(GL_COPY_READ_BUFFER, 4096, nullptr, GL_DYNAMIC_DRAW);

  // Map GL_ELEMENT_ARRAY_BUFFER.
  // Driver maps buf[0].
  // If bug is present: Decoder cache thinks buf[1] is bound, so it inserts
  // entry for buf[1] pointing to buf[0] memory.
  void* shm_a =
      glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, kBufA,
                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
  ASSERT_NE(shm_a, nullptr);

  // SAFETY this span is only created to correspond to the mapped
  // buffer, whose size is known above.
  auto shm_span =
      UNSAFE_BUFFERS(base::span<uint8_t, static_cast<size_t>(kBufA)>(
          static_cast<uint8_t*>(shm_a), static_cast<size_t>(kBufA)));
  std::ranges::fill(shm_span, 0x41);

  // Bind EA to buf[1] on client.
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);

  // Map tiny range to populate client-side tracking for buf[1].
  // If bug is present: Service tries to insert duplicate entry for buf[1].
  // In non-DCHECK builds, this is a no-op on service side.
  void* shm_b =
      glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, 1,
                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
  ASSERT_NE(shm_b, nullptr);

  // Free buf[0]. Driver frees its backing memory.
  // The service entry for buf[1] still points to this freed memory.
  glDeleteBuffers(1, &buf[0]);
  glFinish();

  // Unmap GL_ELEMENT_ARRAY_BUFFER (bound to buf[1] on client, cache says buf[1]
  // on service). If bug is present: Service uses stale entry for buf[1]
  // pointing to freed buf[0] memory, and memcpys to it -> UAF.
  glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
  glFinish();

  glDeleteBuffers(1, &buf[1]);
}

}  // namespace gpu
