// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_GRAPHICS_3D_H_
#define PPAPI_TESTS_TEST_GRAPHICS_3D_H_

#include <stdint.h>

#include <string>
#include "ppapi/tests/test_case.h"

struct PPB_OpenGLES2;

namespace pp {
class Graphics3D;
}  // namespace pp

class TestGraphics3D : public TestCase {
 public:
  TestGraphics3D(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  // Various tests.
  std::string TestExtensionsGL();
  std::string TestFrameGL();
  std::string TestFramePPAPI();
  std::string TestBadResource();
  std::string TestAttributes();

  // Utils used by various tests.
  int32_t SwapBuffersSync(pp::Graphics3D* context);
  std::string CheckPixelPPAPI(pp::Graphics3D* context,
                             int x, int y, const uint8_t expected_color[4]);
  std::string CheckPixelGL(int x, int y, const uint8_t expected_color[4]);

  // OpenGL ES2 interface.
  const PPB_OpenGLES2* opengl_es2_;
};

#endif  // PPAPI_TESTS_TEST_GRAPHICS_3D_H_
