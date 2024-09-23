// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_service_test.h"

#include <memory>

#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gpu {
namespace gles2 {

GpuServiceTest::GpuServiceTest() : ran_setup_(false), ran_teardown_(false) {}

GpuServiceTest::~GpuServiceTest() {
  DCHECK(ran_teardown_);
}

void GpuServiceTest::SetUpWithGLVersion(const char* gl_version,
                                        const char* gl_extensions) {
  testing::Test::SetUp();

  gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
  display_ = gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
  gl_ = std::make_unique<::testing::StrictMock<::gl::MockGLInterface>>();
  ::gl::MockGLInterface::SetGLInterface(gl_.get());

  context_ = new gl::GLContextStub;
  context_->SetExtensionsString(gl_extensions);
  context_->SetGLVersionString(gl_version);
  surface_ = new gl::GLSurfaceStub;
  context_->MakeCurrent(surface_.get());
  ran_setup_ = true;
}

void GpuServiceTest::SetUp() {
  SetUpWithGLVersion("OpenGL ES 2.0", "GL_EXT_framebuffer_object");
}

void GpuServiceTest::TearDown() {
  DCHECK(ran_setup_);
  context_ = nullptr;
  surface_ = nullptr;
  ::gl::MockGLInterface::SetGLInterface(nullptr);
  gl_.reset();
  gl::GLSurfaceTestSupport::ShutdownGL(display_);
  ran_teardown_ = true;

  testing::Test::TearDown();
}

gl::GLContext* GpuServiceTest::GetGLContext() {
  return context_.get();
}

gl::GLSurface* GpuServiceTest::GetGLSurface() {
  return surface_.get();
}

}  // namespace gles2
}  // namespace gpu
