// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>

#include <vector>

#include "base/logging.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GLDrawBuffersTest : public testing::Test {
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

// Test that lazy clearing of integer renderbuffers works even if DRAW_BUFFER0
// is GL_NONE.
TEST_F(GLDrawBuffersTest, ClearUnclearedIntegerAttachmentWithDrawBufferNone) {
  if (!IsApplicable()) {
    return;
  }

  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  const size_t kNumElements = kWidth * kHeight * 4;

  // Create renderbuffer
  GLuint rb = 0;
  glGenRenderbuffers(1, &rb);
  glBindRenderbuffer(GL_RENDERBUFFER, rb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8UI, kWidth, kHeight);

  // Create framebuffer
  GLuint fb = 0;
  glGenFramebuffers(1, &fb);
  glBindFramebuffer(GL_FRAMEBUFFER, fb);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, rb);

  ASSERT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  // Fill the storage with a known marker.
  GLenum draw_buffers[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffersEXT(1, draw_buffers);
  GLuint marker[] = {0xDEu, 0xADu, 0xBEu, 0xEFu};
  glClearBufferuiv(GL_COLOR, 0, marker);

  // Verify it was written
  {
    std::vector<GLuint> pixels(kNumElements, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
                 pixels.data());
    ASSERT_GE(pixels.size(), 4u);
    EXPECT_EQ(0xDEu, pixels[0]);
    EXPECT_EQ(0xADu, pixels[1]);
    EXPECT_EQ(0xBEu, pixels[2]);
    EXPECT_EQ(0xEFu, pixels[3]);
  }

  // Re-spec the same renderbuffer to make it uncleared.
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8UI, kWidth, kHeight);

  // Set DRAW_BUFFER0 to NONE.
  GLenum draw_buffers_none[] = {GL_NONE};
  glDrawBuffersEXT(1, draw_buffers_none);

  // Read pixels to trigger lazy clear.
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  std::vector<GLuint> pixels(kNumElements, 0);
  glReadPixels(0, 0, kWidth, kHeight, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
               pixels.data());

  // Verify it was cleared to 0, not containing the marker.
  for (GLuint val : pixels) {
    EXPECT_EQ(0u, val);
  }

  glDeleteFramebuffers(1, &fb);
  glDeleteRenderbuffers(1, &rb);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

// Test that lazy clearing of integer renderbuffers works for attachment 1
// even if DRAW_BUFFER1 is GL_NONE (which is the default).
TEST_F(GLDrawBuffersTest,
       ClearUnclearedIntegerAttachmentAtSlot1WithDefaultDrawBuffers) {
  if (!IsApplicable()) {
    return;
  }

  GLint max_draw_buffers = 0;
  glGetIntegerv(GL_MAX_DRAW_BUFFERS, &max_draw_buffers);
  if (max_draw_buffers < 2) {
    LOG(INFO) << "Skipping test because MAX_DRAW_BUFFERS is "
              << max_draw_buffers;
    return;
  }

  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  const size_t kNumElements = kWidth * kHeight * 4;

  GLuint rb = 0;
  glGenRenderbuffers(1, &rb);
  glBindRenderbuffer(GL_RENDERBUFFER, rb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8UI, kWidth, kHeight);

  GLuint fb = 0;
  glGenFramebuffers(1, &fb);
  glBindFramebuffer(GL_FRAMEBUFFER, fb);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                            GL_RENDERBUFFER, rb);

  // We need to enable it to clear it first.
  GLenum draw_buffers[] = {GL_NONE, GL_COLOR_ATTACHMENT1};
  glDrawBuffersEXT(2, draw_buffers);

  ASSERT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  GLuint marker[] = {0xDEu, 0xADu, 0xBEu, 0xEFu};
  glClearBufferuiv(GL_COLOR, 1, marker);

  // Verify
  {
    std::vector<GLuint> pixels(kNumElements, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
                 pixels.data());
    ASSERT_GE(pixels.size(), 4u);
    EXPECT_EQ(0xDEu, pixels[0]);
  }

  // Re-spec
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8UI, kWidth, kHeight);

  // Reset DRAW_BUFFERS to default: [COLOR_ATTACHMENT0].
  // This means DRAW_BUFFER1 is GL_NONE.
  GLenum default_draw_buffers[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffersEXT(1, default_draw_buffers);

  // Read pixels from ATTACHMENT1 to trigger lazy clear.
  glReadBuffer(GL_COLOR_ATTACHMENT1);
  std::vector<GLuint> pixels(kNumElements, 0);
  glReadPixels(0, 0, kWidth, kHeight, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
               pixels.data());

  for (GLuint val : pixels) {
    EXPECT_EQ(0u, val);
  }

  glDeleteFramebuffers(1, &fb);
  glDeleteRenderbuffers(1, &rb);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

}  // namespace gpu
