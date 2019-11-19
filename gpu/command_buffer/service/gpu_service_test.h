// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_SERVICE_TEST_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_SERVICE_TEST_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

namespace gl {
class GLContextStub;
class GLSurface;
class GLSurfaceStub;
}

namespace gpu {
namespace gles2 {

// Base class for tests that need mock GL bindings.
class GpuServiceTest : public testing::Test {
 public:
  GpuServiceTest();
  ~GpuServiceTest() override;

 protected:
  void SetUpWithGLVersion(const char* gl_version, const char* gl_extensions);
  void SetUp() override;
  void TearDown() override;
  gl::GLContext* GetGLContext();
  gl::GLSurface* GetGLSurface();

  std::unique_ptr<::testing::StrictMock<::gl::MockGLInterface>> gl_;

 private:
  bool ran_setup_;
  bool ran_teardown_;
  scoped_refptr<gl::GLContextStub> context_;
  scoped_refptr<gl::GLSurfaceStub> surface_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_SERVICE_TEST_H_
