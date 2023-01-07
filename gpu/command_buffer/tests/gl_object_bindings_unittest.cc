// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class ObjectBindingsTest : public testing::Test {
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

TEST_F(ObjectBindingsTest, Buffers) {
  if (!IsApplicable()) {
    return;
  }

  const std::pair<GLenum, GLenum> buffer_bindings[] = {
      {GL_ARRAY_BUFFER, GL_ARRAY_BUFFER_BINDING},
      {GL_ELEMENT_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER_BINDING},
      {GL_PIXEL_PACK_BUFFER, GL_PIXEL_PACK_BUFFER_BINDING},
      {GL_PIXEL_UNPACK_BUFFER, GL_PIXEL_UNPACK_BUFFER_BINDING},
      {GL_TRANSFORM_FEEDBACK_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER_BINDING},
      {GL_COPY_READ_BUFFER, GL_COPY_READ_BUFFER_BINDING},
      {GL_COPY_WRITE_BUFFER, GL_COPY_WRITE_BUFFER_BINDING},
      {GL_UNIFORM_BUFFER, GL_UNIFORM_BUFFER_BINDING},
  };

  for (auto binding : buffer_bindings) {
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(binding.first, buffer);

    {
      GLint result = 0;
      glGetIntegerv(binding.second, &result);
      EXPECT_EQ(static_cast<GLuint>(result), buffer);
    }

    {
      GLfloat result = 0;
      glGetFloatv(binding.second, &result);
      EXPECT_EQ(static_cast<GLuint>(result), buffer);
    }

    glBindBuffer(binding.first, 0);
    glDeleteBuffers(1, &buffer);
  }

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(ObjectBindingsTest, FramebufferAttachments) {
  if (!IsApplicable()) {
    return;
  }

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);

  GLuint renderbuffer = 0;
  glGenRenderbuffers(1, &renderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);

  GLuint framebuffer = 0;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture, 0);
  {
    GLint result = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                          &result);
    EXPECT_EQ(static_cast<GLuint>(result), texture);
  }

  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, renderbuffer);
  {
    GLint result = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                          &result);
    EXPECT_EQ(static_cast<GLuint>(result), renderbuffer);
  }

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

}  // namespace gpu
