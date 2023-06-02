// Copyright 2018 The Chromium Authors
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

}  // namespace gpu
