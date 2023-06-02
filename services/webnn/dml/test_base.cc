// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/test_base.h"

#include "base/command_line.h"
#include "ui/gl/init/gl_factory.h"

namespace webnn::dml {

bool UseGPUInTests() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseGpuInTests);
}

bool TestBase::InitializeGLDisplay() {
  display_ = gl::init::InitializeGLNoExtensionsOneOff(
      /*init_bindings=*/true,
      /*gpu_preference=*/gl::GpuPreference::kDefault);
  return display_ != nullptr;
}

void TestBase::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  ASSERT_TRUE(InitializeGLDisplay());
}

void TestBase::TearDown() {
  if (display_) {
    gl::init::ShutdownGL(display_, /*due_to_fallback=*/false);
  }
}

}  // namespace webnn::dml
