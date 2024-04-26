// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/gl_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GLDynamicConfigTest : public testing::Test {
};

TEST_F(GLDynamicConfigTest, SwitchConfigurationInNonVirtualizedContextMode) {
  // TODO(crbug.com/40245697): Fix (or delete) the test. The previous
  // blocker issue https://crbug.com/527126 was closed as WontFix.
#if 0
  // Disable usage of virtualized GL context.
  GLManager::SetEnableVirtualContext(false);

  GLManager::Options options;
  // TODO(crbug.com/40245697): This modification is untested,
  // GLManager::Options does not currently have a surface_format
  // field.
  options.surface_format = gl::GLSurfaceFormat();
  options.surface_format.SetRGB565();
  GLManager gl_rgb_565;
  gl_rgb_565.Initialize(options);

  // The test is successful if the following command returns without assertion
  // failure. Otherwise it would have stopped in GLManager while initializing
  // the context and making it current.
  GLManager gl_argb_8888;
  gl_argb_8888.Initialize(GLManager::Options());

  gl_rgb_565.Destroy();
  gl_argb_8888.Destroy();
#endif
}

}  // namespace gpu
