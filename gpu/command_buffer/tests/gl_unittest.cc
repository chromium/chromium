// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include "base/containers/heap_array.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_service_helper.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/buildflags.h"

namespace gpu {

class GLTest : public testing::Test {
 protected:
  void SetUp() override { gl_.Initialize(GLManager::Options()); }

  void TearDown() override { gl_.Destroy(); }

  GLManager gl_;
};

// Test that GL is at least minimally working.
TEST_F(GLTest, Basic) {
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  uint8_t expected[] = {
      0, 255, 0, 255,
  };
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, expected, nullptr));
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(GLTest, BasicFBO) {
  GLuint tex = 0;
  glGenTextures(1, &tex);
  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindTexture(GL_TEXTURE_2D, tex);
  auto pixels = base::HeapArray<uint8_t>::WithSize(16 * 16 * 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  uint8_t expected[] = {
      0, 255, 0, 255,
  };
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 16, 16, 0, expected, nullptr));
  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &tex);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(GLTest, SimpleShader) {
  static const char* v_shader_str =
      "attribute vec4 a_Position;\n"
      "void main()\n"
      "{\n"
      "   gl_Position = a_Position;\n"
      "}\n";
  static const char* f_shader_str =
      "precision mediump float;\n"
      "void main()\n"
      "{\n"
      "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
      "}\n";

  GLuint program = GLTestHelper::LoadProgram(v_shader_str, f_shader_str);
  glUseProgram(program);
  GLuint position_loc = glGetAttribLocation(program, "a_Position");

  GLTestHelper::SetupUnitQuad(position_loc);

  uint8_t expected_clear[] = {
      127, 0, 255, 0,
  };
  glClearColor(0.5f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 1, 1, 1, expected_clear, nullptr));
  uint8_t expected_draw[] = {
      0, 255, 0, 255,
  };
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, expected_draw, nullptr));
}

TEST_F(GLTest, FeatureFlagsMatchCapabilities) {
  scoped_refptr<gles2::FeatureInfo> features =
      new gles2::FeatureInfo(gl_.workarounds(), GpuFeatureInfo());
  features->InitializeForTesting();
  const auto& caps = gl_.GetCapabilities();
  const auto& gl_caps = gl_.GetGLCapabilities();
  const auto& flags = features->feature_flags();
  EXPECT_EQ(caps.egl_image_external, flags.oes_egl_image_external);
  EXPECT_EQ(caps.texture_format_bgra8888, flags.ext_texture_format_bgra8888);
  EXPECT_EQ(gl_caps.sync_query, flags.chromium_sync_query);
  EXPECT_EQ(caps.sync_query, flags.chromium_sync_query);
  EXPECT_EQ(caps.texture_rg, flags.ext_texture_rg);
  EXPECT_EQ(caps.image_ycbcr_420v, flags.chromium_image_ycbcr_420v);
  EXPECT_EQ(caps.image_ar30, flags.chromium_image_ar30);
  EXPECT_EQ(caps.image_ab30, flags.chromium_image_ab30);
  EXPECT_EQ(caps.image_ycbcr_p010, flags.chromium_image_ycbcr_p010);
  EXPECT_EQ(caps.render_buffer_format_bgra8888,
            flags.ext_render_buffer_format_bgra8888);
  EXPECT_EQ(gl_caps.occlusion_query_boolean, flags.occlusion_query_boolean);
}

TEST_F(GLTest, GetString) {
  EXPECT_STREQ(
      "OpenGL ES 2.0 Chromium",
      reinterpret_cast<const char*>(glGetString(GL_VERSION)));
  EXPECT_STREQ(
      "OpenGL ES GLSL ES 1.0 Chromium",
      reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
}

// TODO(crbug.com/513543143): Goldfish GLES emulator driver on 32-bit x86
// Android bots has a known driver bug where it incorrectly rejects
// glCopyTexImage2D on cubemaps with GL_INVALID_ENUM.
#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER) && \
    !(BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86))
class GL3Test : public GLTest {
 protected:
  void SetUp() override {
    if (gl_.gpu_preferences().use_passthrough_cmd_decoder) {
      GTEST_SKIP() << "Test only applies to validating decoder";
    }
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    gl_.Initialize(options);
  }
};

// Tests that glCopyTexImage2D correctly updates the internal service-side
// texture level size when using the workaround path that temporarily clamps
// base/max levels to prevent FBO incompleteness. Specifically, it verifies
// that the workaround uses the correct base target (GL_TEXTURE_CUBE_MAP)
// instead of the specific face target (e.g., GL_TEXTURE_CUBE_MAP_POSITIVE_X)
// which would cause driver-side GL_INVALID_ENUM errors and result in a state
// desynchronization between the decoder and the GPU driver.
TEST_F(GL3Test, CopyTexImage2DWorkaroundStateDesync) {
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

  // We manually specify all mip levels instead of calling glGenerateMipmap
  // because various GPU drivers (e.g., the Android x64 emulator) have bugs
  // when generating cubemap texture mipmaps, which can cause FBO
  // incompleteness.
  auto pixels = base::HeapArray<uint8_t>::WithSize(32 * 32 * 4);
  for (int level = 0; level < 6; ++level) {
    int size = 32 >> level;
    for (int i = 0; i < 6; ++i) {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, level, GL_RGBA8, size,
                   size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }
  }

  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_CUBE_MAP_POSITIVE_Y, tex, 3);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 5, GL_RGBA8, 0, 0, 2, 2, 0);

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Verify that the decoder's internal LevelInfo tracking has been correctly
  // updated to match the new 2x2 size. If the workaround had triggered a driver
  // error, the decoder would have skipped updating this state while the driver-
  // side allocation would still be updated, leading to a security
  // vulnerability.
  int tracked_width = 0;
  int tracked_height = 0;
  bool defined =
      InspectTextureLevelSize(&gl_, tex, GL_TEXTURE_CUBE_MAP_POSITIVE_X, 5,
                              &tracked_width, &tracked_height);
  EXPECT_TRUE(defined);
  EXPECT_EQ(2, tracked_width);
  EXPECT_EQ(2, tracked_height);

  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &tex);
}
#endif

}  // namespace gpu
