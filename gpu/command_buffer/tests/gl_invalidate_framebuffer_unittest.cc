// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GLInvalidateFramebufferTest : public testing::Test {
 protected:
  GLManager::Options GetGlManagerOptions() {
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    return options;
  }

  void SetUp() override { gl_.Initialize(GetGlManagerOptions()); }

  void TearDown() override { gl_.Destroy(); }

  GLManager gl_;
};

// Port of the POC for b/507508103.
// Creates an incomplete framebuffer and calls glInvalidateFramebuffer.
// Should not crash.
TEST_F(GLInvalidateFramebufferTest, IncompleteFBOInvalidate) {
  // If ES3 is not supported, skip the test.
  if (!gl_.decoder() || !gl_.decoder()->GetContextGroup()) {
    GTEST_SKIP() << "ES3 not supported";
  }

  GLuint tex, fbo;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
  // 2 levels, GL_RGBA8 (not depth/stencil renderable), 4x4, 1 layer
  glTexStorage3D(GL_TEXTURE_2D_ARRAY, 2, GL_RGBA8, 4, 4, 1);

  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  // Try to attach level 2 (out of range, levels are 0 and 1) to
  // GL_DEPTH_STENCIL_ATTACHMENT. This should fail validation and generate
  // GL_INVALID_VALUE in validating decoder.
  glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, tex, 2,
                            0);

  GLenum att[] = {GL_DEPTH_STENCIL_ATTACHMENT};
  // This call triggered the crash on vulnerable Qualcomm drivers.
  // With the workaround, it should be skipped or handled safely without
  // crashing.
  glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, att);

  // We call glFinish to ensure the GPU has processed the commands and we don't
  // crash asynchronously.
  glFinish();

  // Clean up
  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &tex);

  // Consume expected errors (GL_INVALID_VALUE from glFramebufferTextureLayer,
  // and potentially GL_INVALID_OPERATION from glInvalidateFramebuffer if not
  // skipped by ANGLE) to avoid failing CheckGLError.
  while (glGetError() != GL_NO_ERROR) {
  }

  GLTestHelper::CheckGLError("no errors at the end of test", __LINE__);
}

}  // namespace gpu
