// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stddef.h>
#include <stdint.h>

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_share_group.h"

namespace gpu {

namespace {
GLcolorSpace GetGLColorSpace() {
  static gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  return color_space.AsGLColorSpace();
}

uint32_t ReadTexel(GLuint id, GLint x, GLint y) {
  GLint old_fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);

  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         id,
                         0);
  // Some drivers (NVidia/SGX) require texture settings to be a certain way or
  // they won't report FRAMEBUFFER_COMPLETE.
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  uint32_t texel = 0;
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &texel);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);

  glDeleteFramebuffers(1, &fbo);

  return texel;
}
}

class GLTextureMailboxTest : public testing::Test {
 protected:
  void SetUpContexts() {
    gl1_.Initialize(GLManager::Options());
    GLManager::Options options;
    options.share_mailbox_manager = &gl1_;
    gl2_.Initialize(options);
  }

  void TearDown() override {
    gl1_.Destroy();
    gl2_.Destroy();
  }

  // The second GL context takes and consumes a mailbox from the first GL
  // context. Assumes that |gl1_| is current.
  Mailbox TakeAndConsumeMailbox() {
    glResizeCHROMIUM(10, 10, 1, GetGLColorSpace(), true);
    glClearColor(0, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ::gles2::GetGLContext()->SwapBuffers(1);

    Mailbox mailbox = Mailbox::Generate();
    gl1_.decoder()->TakeFrontBuffer(mailbox);

    gl2_.MakeCurrent();
    GLuint tex = glCreateAndConsumeTextureCHROMIUM(mailbox.name);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDeleteTextures(1, &tex);
    glFlush();
    gl1_.MakeCurrent();
    return mailbox;
  }

  void AllocateTextureBackedFramebuffer(GLuint* tex, GLuint* fbo) {
    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           *tex, 0);
    ASSERT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatus(GL_FRAMEBUFFER));
  }

  GLManager gl1_;
  GLManager gl2_;
};

TEST_F(GLTextureMailboxTest, ProduceAndConsumeTexture) {
  SetUpContexts();
  gl1_.MakeCurrent();

  GLuint tex1;
  glGenTextures(1, &tex1);

  glBindTexture(GL_TEXTURE_2D, tex1);
  uint32_t source_pixel = 0xFF0000FF;
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               1, 1,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               &source_pixel);

  GLbyte mailbox1[GL_MAILBOX_SIZE_CHROMIUM];
  glProduceTextureDirectCHROMIUM(tex1, mailbox1);
  glFlush();

  gl2_.MakeCurrent();

  GLuint tex2 = glCreateAndConsumeTextureCHROMIUM(mailbox1);
  glBindTexture(GL_TEXTURE_2D, tex2);
  EXPECT_EQ(source_pixel, ReadTexel(tex2, 0, 0));
  GLbyte mailbox2[GL_MAILBOX_SIZE_CHROMIUM];
  glProduceTextureDirectCHROMIUM(tex2, mailbox2);
  glFlush();

  gl1_.MakeCurrent();

  tex1 = glCreateAndConsumeTextureCHROMIUM(mailbox2);
  glBindTexture(GL_TEXTURE_2D, tex1);
  EXPECT_EQ(source_pixel, ReadTexel(tex1, 0, 0));
}

TEST_F(GLTextureMailboxTest, ProduceAndConsumeTextureRGB) {
  SetUpContexts();
  gl1_.MakeCurrent();

  GLuint tex1;
  glGenTextures(1, &tex1);

  glBindTexture(GL_TEXTURE_2D, tex1);
  uint32_t source_pixel = 0xFF000000;
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGB,
               1, 1,
               0,
               GL_RGB,
               GL_UNSIGNED_BYTE,
               &source_pixel);

  GLbyte mailbox1[GL_MAILBOX_SIZE_CHROMIUM];
  glProduceTextureDirectCHROMIUM(tex1, mailbox1);
  glFlush();

  gl2_.MakeCurrent();

  GLuint tex2 = glCreateAndConsumeTextureCHROMIUM(mailbox1);
  glBindTexture(GL_TEXTURE_2D, tex2);
  EXPECT_EQ(source_pixel, ReadTexel(tex2, 0, 0));

  GLbyte mailbox2[GL_MAILBOX_SIZE_CHROMIUM];
  glProduceTextureDirectCHROMIUM(tex2, mailbox2);
  glFlush();

  gl1_.MakeCurrent();

  tex1 = glCreateAndConsumeTextureCHROMIUM(mailbox2);
  glBindTexture(GL_TEXTURE_2D, tex1);
  EXPECT_EQ(source_pixel, ReadTexel(tex1, 0, 0));
}

TEST_F(GLTextureMailboxTest, SharedTextures) {
  SetUpContexts();
  gl1_.MakeCurrent();
  GLuint tex1;
  glGenTextures(1, &tex1);

  glBindTexture(GL_TEXTURE_2D, tex1);
  uint32_t source_pixel = 0xFF0000FF;
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               1, 1,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               &source_pixel);
  GLbyte mailbox[GL_MAILBOX_SIZE_CHROMIUM];
  glProduceTextureDirectCHROMIUM(tex1, mailbox);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glFlush();

  gl2_.MakeCurrent();
  GLuint tex2 = glCreateAndConsumeTextureCHROMIUM(mailbox);
  glBindTexture(GL_TEXTURE_2D, tex2);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Change texture in context 2.
  source_pixel = 0xFF00FF00;
  glTexSubImage2D(GL_TEXTURE_2D,
                  0,
                  0, 0,
                  1, 1,
                  GL_RGBA,
                  GL_UNSIGNED_BYTE,
                  &source_pixel);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glFlush();

  // Check it in context 1.
  gl1_.MakeCurrent();
  EXPECT_EQ(source_pixel, ReadTexel(tex1, 0, 0));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Change parameters (note: ReadTexel will reset those).
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_NEAREST);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glFlush();

  // Check in context 2.
  gl2_.MakeCurrent();
  GLint parameter = 0;
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &parameter);
  EXPECT_EQ(GL_REPEAT, parameter);
  parameter = 0;
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &parameter);
  EXPECT_EQ(GL_LINEAR, parameter);
  parameter = 0;
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &parameter);
  EXPECT_EQ(GL_LINEAR_MIPMAP_NEAREST, parameter);

  // Delete texture in context 1.
  gl1_.MakeCurrent();
  glDeleteTextures(1, &tex1);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Check texture still exists in context 2.
  gl2_.MakeCurrent();
  EXPECT_EQ(source_pixel, ReadTexel(tex2, 0, 0));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // The mailbox should still exist too.
  GLuint tex3 = glCreateAndConsumeTextureCHROMIUM(mailbox);
  glBindTexture(GL_TEXTURE_2D, tex3);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Delete both textures.
  glDeleteTextures(1, &tex2);
  glDeleteTextures(1, &tex3);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Mailbox should be gone now.
  tex2 = glCreateAndConsumeTextureCHROMIUM(mailbox);
  glBindTexture(GL_TEXTURE_2D, tex2);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  glDeleteTextures(1, &tex2);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

TEST_F(GLTextureMailboxTest, OrderingBarrierImpliesFlush) {
  SetUpContexts();
  gl2_.MakeCurrent();

  // If the current platform requires the use of virtualized GL contexts
  // for correctness, this harness does not respect that flag, so skip
  // this test.
  if (gl2_.workarounds().use_virtualized_gl_contexts) {
    SUCCEED() << "Platform requires virtualized GL contexts; skipping.";
    return;
  }

  GLuint fbo2 = 0;
  glGenFramebuffers(1, &fbo2);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo2);
  gl1_.MakeCurrent();
  GLuint tex1 = 0;
  GLuint fbo1 = 0;
  AllocateTextureBackedFramebuffer(&tex1, &fbo1);

  // Many times:
  //  - Set the texture to one color in context 1
  //  - Mailbox the texture to context 2
  //  - Set the texture to another color in context 2
  //  - Mailbox the texture to context 1
  //  - Read back the framebuffer in context 1
  //  - Assert it's the color set by context 2

  GLbyte mailbox[GL_MAILBOX_SIZE_CHROMIUM];
  glProduceTextureDirectCHROMIUM(tex1, mailbox);

  for (int i = 0; i < 1000; ++i) {
    // Assume context 1 is current.
    // Clear to red
    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    // Enforce ordering with respect to context 2.
    gl1_.gles2_implementation()->OrderingBarrierCHROMIUM();
    // Consume texture in context 2.
    gl2_.MakeCurrent();
    GLuint tex2 = glCreateAndConsumeTextureCHROMIUM(mailbox);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    // Set up framebuffer in context 2 (strictly, not necessary, could
    // do this just once).
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex2, 0);
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatus(GL_FRAMEBUFFER));
    // Clear to green.
    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDeleteTextures(1, &tex2);
    // Enforce ordering with respect to context 2.
    gl2_.gles2_implementation()->OrderingBarrierCHROMIUM();
    // Consume texture in context 1.
    gl1_.MakeCurrent();
    // Read back framebuffer.
    GLubyte pixel[4];
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel[0]);
    EXPECT_EQ(0, pixel[0]);
    EXPECT_EQ(255, pixel[1]);
    EXPECT_EQ(0, pixel[2]);
    EXPECT_EQ(255, pixel[3]);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &fbo1);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDeleteTextures(1, &tex1);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  gl2_.MakeCurrent();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &fbo2);
  glBindTexture(GL_TEXTURE_2D, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

TEST_F(GLTextureMailboxTest, TakeFrontBuffer) {
  SetUpContexts();

  gl2_.MakeCurrent();
  glResizeCHROMIUM(10, 10, 1, GetGLColorSpace(), true);
  glClearColor(0, 1, 1, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  ::gles2::GetGLContext()->SwapBuffers(1);
  Mailbox mailbox = Mailbox::Generate();
  gl2_.decoder()->TakeFrontBuffer(mailbox);

  gl1_.MakeCurrent();
  GLuint tex1 = glCreateAndConsumeTextureCHROMIUM(mailbox.name);
  glBindTexture(GL_TEXTURE_2D, tex1);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_EQ(0xFFFFFF00u, ReadTexel(tex1, 0, 0));

  gl2_.MakeCurrent();
  glClearColor(1, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  ::gles2::GetGLContext()->SwapBuffers(1);

  gl1_.MakeCurrent();
  EXPECT_EQ(0xFFFFFF00u, ReadTexel(tex1, 0, 0));

  glDeleteTextures(1, &tex1);

  gl2_.MakeCurrent();
  gl2_.decoder()->ReturnFrontBuffer(mailbox, false);

  // Flushing doesn't matter, only SwapBuffers(1).
  glClearColor(0, 1, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glFlush();

  Mailbox mailbox2 = Mailbox::Generate();
  gl2_.decoder()->TakeFrontBuffer(mailbox2);

  gl1_.MakeCurrent();
  tex1 = glCreateAndConsumeTextureCHROMIUM(mailbox2.name);
  glBindTexture(GL_TEXTURE_2D, tex1);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_EQ(0xFF0000FFu, ReadTexel(tex1, 0, 0));

  gl2_.MakeCurrent();
  gl2_.Destroy();

  gl1_.MakeCurrent();
  EXPECT_EQ(0xFF0000FFu, ReadTexel(tex1, 0, 0));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glDeleteTextures(1, &tex1);
}

// The client, represented by |gl2_|, will request 5 frontbuffers, and then
// start returning them.
TEST_F(GLTextureMailboxTest, FrontBufferCache) {
  SetUpContexts();
  gl1_.MakeCurrent();

  std::vector<Mailbox> mailboxes;
  for (int i = 0; i < 5; ++i) {
    Mailbox mailbox = TakeAndConsumeMailbox();
    mailboxes.push_back(mailbox);
  }
  EXPECT_EQ(5u, gl1_.decoder()->GetSavedBackTextureCountForTest());
  EXPECT_EQ(5u, gl1_.decoder()->GetCreatedBackTextureCountForTest());

  // If the textures aren't lost, they're reused.
  for (int i = 0; i < 100; ++i) {
    gl1_.decoder()->ReturnFrontBuffer(mailboxes[0], false);
    mailboxes.erase(mailboxes.begin());

    Mailbox mailbox = TakeAndConsumeMailbox();
    mailboxes.push_back(mailbox);
  }

  EXPECT_EQ(5u, gl1_.decoder()->GetSavedBackTextureCountForTest());
  EXPECT_EQ(5u, gl1_.decoder()->GetCreatedBackTextureCountForTest());

  // If the textures are lost, they're not reused.
  for (int i = 0; i < 100; ++i) {
    gl1_.decoder()->ReturnFrontBuffer(mailboxes[0], true);
    mailboxes.erase(mailboxes.begin());

    Mailbox mailbox = TakeAndConsumeMailbox();
    mailboxes.push_back(mailbox);
  }

  EXPECT_EQ(5u, gl1_.decoder()->GetSavedBackTextureCountForTest());
  EXPECT_EQ(105u, gl1_.decoder()->GetCreatedBackTextureCountForTest());
}

// The client, represented by |gl2_|, will request and return 5 frontbuffers.
// Then the size of the buffer will be changed. All cached frontbuffers should
// be discarded.
TEST_F(GLTextureMailboxTest, FrontBufferChangeSize) {
  SetUpContexts();
  gl1_.MakeCurrent();

  std::vector<Mailbox> mailboxes;
  for (int i = 0; i < 5; ++i) {
    Mailbox mailbox = TakeAndConsumeMailbox();
    mailboxes.push_back(mailbox);
  }
  EXPECT_EQ(5u, gl1_.decoder()->GetSavedBackTextureCountForTest());

  for (int i = 0; i < 5; ++i) {
    gl1_.decoder()->ReturnFrontBuffer(mailboxes[i], false);
  }
  mailboxes.clear();
  EXPECT_EQ(5u, gl1_.decoder()->GetSavedBackTextureCountForTest());

  glResizeCHROMIUM(21, 31, 1, GetGLColorSpace(), true);
  ::gles2::GetGLContext()->SwapBuffers(1);
  EXPECT_EQ(0u, gl1_.decoder()->GetSavedBackTextureCountForTest());
}

// The client, represented by |gl2_|, will request and return 5 frontbuffers.
// Then |gl1_| will start drawing with a different color. The returned
// frontbuffers should pick up the new color.
TEST_F(GLTextureMailboxTest, FrontBufferChangeColor) {
  GLManager::Options options1;
  options1.multisampled = true;
  gl1_.Initialize(options1);

  GLManager::Options options2;
  options2.share_mailbox_manager = &gl1_;
  gl2_.Initialize(options2);

  gl1_.MakeCurrent();
  std::vector<Mailbox> mailboxes;
  for (int i = 0; i < 5; ++i) {
    Mailbox mailbox = TakeAndConsumeMailbox();
    mailboxes.push_back(mailbox);
  }

  for (int i = 0; i < 5; ++i) {
    gl1_.decoder()->ReturnFrontBuffer(mailboxes[i], false);
  }
  mailboxes.clear();

  for (int i = 0; i < 5; ++i) {
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ::gles2::GetGLContext()->SwapBuffers(1);

    Mailbox mailbox = Mailbox::Generate();
    gl1_.decoder()->TakeFrontBuffer(mailbox);

    // Normally, consumers of TakeFrontBuffer() must supply their own
    // synchronization mechanism. For this test, just use a glFinish().
    glFinish();

    gl2_.MakeCurrent();
    GLuint tex = glCreateAndConsumeTextureCHROMIUM(mailbox.name);
    glBindTexture(GL_TEXTURE_2D, tex);

    EXPECT_EQ(0xFF0000FFu, ReadTexel(tex, 0, 0));

    glDeleteTextures(1, &tex);
    glFlush();
    gl1_.MakeCurrent();
  }
}

// Verify that front buffer textures have sampler parameters that will not cause
// them to be incomplete when sampled
TEST_F(GLTextureMailboxTest, FrontBufferSamplerParameters) {
  SetUpContexts();

  gl2_.MakeCurrent();
  glResizeCHROMIUM(10, 10, 1, GetGLColorSpace(), true);
  glClearColor(0, 1, 1, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  ::gles2::GetGLContext()->SwapBuffers(1);
  Mailbox mailbox = Mailbox::Generate();
  gl2_.decoder()->TakeFrontBuffer(mailbox);

  gl1_.MakeCurrent();
  GLuint tex1 = glCreateAndConsumeTextureCHROMIUM(mailbox.name);
  glBindTexture(GL_TEXTURE_2D, tex1);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  constexpr std::pair<GLenum, GLint> expected_parameters[] = {
      {GL_TEXTURE_MAG_FILTER, GL_LINEAR},
      {GL_TEXTURE_MIN_FILTER, GL_LINEAR},
      {GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE},
      {GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE},
  };
  for (const auto& expected_parameter : expected_parameters) {
    GLint value = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, expected_parameter.first, &value);
    EXPECT_EQ(expected_parameter.second, value);
  }
}

// http://crbug.com/281565
#if !defined(OS_ANDROID)
TEST_F(GLTextureMailboxTest, TakeFrontBufferMultipleContexts) {
  SetUpContexts();
  Mailbox mailbox[2];
  GLuint tex[2];

  GLManager::Options options;
  options.share_mailbox_manager = &gl1_;
  GLManager other_gl[2];
  for (size_t i = 0; i < 2; ++i) {
    other_gl[i].Initialize(options);
    other_gl[i].MakeCurrent();
    glResizeCHROMIUM(10, 10, 1, GetGLColorSpace(), true);
    glClearColor(1 - i % 2, i % 2, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ::gles2::GetGLContext()->SwapBuffers(0, 1);
    mailbox[i] = Mailbox::Generate();
    other_gl[i].decoder()->TakeFrontBuffer(mailbox[i]);
    // Make sure both "other gl" are in the same share group.
    if (!options.share_group_manager)
      options.share_group_manager = other_gl+i;
  }

  gl1_.MakeCurrent();
  for (size_t i = 0; i < 2; ++i) {
    tex[i] = glCreateAndConsumeTextureCHROMIUM(mailbox[i].name);
    glBindTexture(GL_TEXTURE_2D, tex[i]);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }

  gl1_.MakeCurrent();
  EXPECT_EQ(0xFF0000FFu, ReadTexel(tex[0], 0, 0));
  EXPECT_EQ(0xFF00FF00u, ReadTexel(tex[1], 9, 9));

  for (size_t i = 0; i < 2; ++i) {
    other_gl[i].MakeCurrent();
    other_gl[i].Destroy();
  }

  gl1_.MakeCurrent();
  glDeleteTextures(2, tex);
}
#endif

}  // namespace gpu

