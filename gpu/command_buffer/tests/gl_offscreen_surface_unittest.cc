// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

// Test that an offscreen surface with zero size initializes correctly
TEST(OffscreenSurfaceTest, ZeroInitialSize) {
  GLManager::Options options;
  options.size = gfx::Size(0, 0);
  options.context_type = CONTEXT_TYPE_OPENGLES2;

  GLManager gl;
  gl.Initialize(options);
  ASSERT_TRUE(gl.IsInitialized());

  gl.Destroy();
}

// Test that an offscreen surface can be resized to zero
TEST(OffscreenSurfaceTest, ResizeToZero) {
  GLManager::Options options;
  options.size = gfx::Size(4, 4);
  options.context_type = CONTEXT_TYPE_OPENGLES2;

  GLManager gl;
  gl.Initialize(options);
  ASSERT_TRUE(gl.IsInitialized());
  gl.MakeCurrent();

  // If losing the context will cause the process to exit, do not perform this
  // test as it will cause all subsequent tests to not run.
  if (gl.workarounds().exit_on_context_lost) {
    gl.Destroy();
    return;
  }

  // Generates context loss and fails the test if the resize fails.
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  glResizeCHROMIUM(0, 0, 1.0f, color_space.AsGLColorSpace(), GL_TRUE);

  gl.Destroy();
}

// Resize to a number between maximum int and uint
TEST(OffscreenSurfaceTest, ResizeOverflow) {
  GLManager::Options options;
  options.size = gfx::Size(4, 4);
  options.context_type = CONTEXT_TYPE_OPENGLES2;
  options.context_lost_allowed = true;

  GLManager gl;
  gl.Initialize(options);
  ASSERT_TRUE(gl.IsInitialized());
  gl.MakeCurrent();

  // If losing the context will cause the process to exit, do not perform this
  // test as it will cause all subsequent tests to not run.
  if (gl.workarounds().exit_on_context_lost) {
    gl.Destroy();
    return;
  }

  // Context loss is allowed trying to resize to such a huge value but make sure
  // that no asserts or undefined behavior is triggered
  static const GLuint kLargeSize =
      static_cast<GLuint>(std::numeric_limits<int>::max()) + 10;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  glResizeCHROMIUM(kLargeSize, 1, 1.0f, color_space.AsGLColorSpace(), GL_TRUE);

  gl.Destroy();
}

}  // namespace gpu
