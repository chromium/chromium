// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/command_buffer/tests/texture_image_factory.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"

#if BUILDFLAG(IS_MAC)
#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"
#endif

namespace gpu {

class GLNativeGMBTest : public testing::Test {
 protected:
  void SetUp() override {
    gl_.Initialize(GLManager::Options());
  }

  void TearDown() override {
    gl_.Destroy();
  }

  // Runs a simple battery of tests.
  void RunBackbufferTestWithOptions(const GLManager::Options& options) {
    GLManager gl;
    gl.Initialize(options);
    gl.MakeCurrent();

    // Clear the back buffer and check that it has the right values.
    glClearColor(0.0f, 0.25f, 0.5f, 0.7f);
    glClear(GL_COLOR_BUFFER_BIT);
    uint8_t pixel[4];
    memset(pixel, 0, 4);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
    EXPECT_NEAR(0u, pixel[0], 2);
    EXPECT_NEAR(64u, pixel[1], 2);
    EXPECT_NEAR(127u, pixel[2], 2);
    uint8_t alpha = options.backbuffer_alpha ? 178 : 255;
    EXPECT_NEAR(alpha, pixel[3], 2);

    // Resize, then clear the back buffer and check its contents.
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    glResizeCHROMIUM(10, 10, 1, color_space.AsGLColorSpace(), true);
    glClearColor(0.5f, 0.6f, 0.7f, 0.8f);
    glClear(GL_COLOR_BUFFER_BIT);
    memset(pixel, 0, 4);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
    EXPECT_NEAR(128u, pixel[0], 2);
    EXPECT_NEAR(153u, pixel[1], 2);
    EXPECT_NEAR(178u, pixel[2], 2);
    uint8_t alpha2 = options.backbuffer_alpha ? 204 : 255;
    EXPECT_NEAR(alpha2, pixel[3], 2);

    // Swap buffers, then clear the back buffer and check its contents.
    ::gles2::GetGLContext()->SwapBuffers(0, 1);
    glClearColor(0.1f, 0.2f, 0.3f, 0.4f);
    glClear(GL_COLOR_BUFFER_BIT);
    memset(pixel, 0, 4);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
    EXPECT_NEAR(25u, pixel[0], 2);
    EXPECT_NEAR(51u, pixel[1], 2);
    EXPECT_NEAR(76u, pixel[2], 2);
    uint8_t alpha3 = options.backbuffer_alpha ? 102 : 255;
    EXPECT_NEAR(alpha3, pixel[3], 2);

    gl.Destroy();
  }

  GLManager gl_;
};

TEST_F(GLNativeGMBTest, TestNativeGMBBackbufferWithDifferentConfigurations) {
  if (!GLTestHelper::HasExtension("GL_ARB_texture_rectangle")) {
    LOG(INFO) << "GL_ARB_texture_rectangle not supported. Skipping test...";
    return;
  }

  // TODO(jonahr): Test fails on Linux/Mac with ANGLE/passthrough
  // (crbug.com/1099768)
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("linux mac passthrough")) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  GpuMemoryBufferFactoryIOSurface image_factory;
#else
  TextureImageFactory image_factory;
  image_factory.SetRequiredTextureType(GL_TEXTURE_RECTANGLE_ARB);
#endif

  for (int has_alpha = 0; has_alpha <= 1; ++has_alpha) {
    for (int msaa = 0; msaa <= 1; ++msaa) {
      for (int preserve_backbuffer = 0; preserve_backbuffer <= 1;
           ++preserve_backbuffer) {
        GLManager::Options options;
        options.image_factory = &image_factory;
        options.multisampled = msaa == 1;
        options.backbuffer_alpha = has_alpha == 1;
        options.preserve_backbuffer = preserve_backbuffer;

        RunBackbufferTestWithOptions(options);
    }
    }
  }
}

}  // namespace gpu
