// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/test_base.h"

#include "base/command_line.h"
#include "ui/gl/init/gl_factory.h"

namespace webnn::dml {

void TestBase::SetUp() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    // GTEST_SKIP() will let SetUp() method return directly.
    GTEST_SKIP() << "Skipping all tests for this fixture if GPU hardware "
                    "hasn't been used in tests.";
  }
  display_ = gl::init::InitializeGLNoExtensionsOneOff(
      /*init_bindings=*/true,
      /*gpu_preference=*/gl::GpuPreference::kDefault);
}

void TestBase::TearDown() {
  if (display_) {
    gl::init::ShutdownGL(display_, /*due_to_fallback=*/false);
  }
}

}  // namespace webnn::dml
