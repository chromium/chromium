// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_TEST_BASE_H_
#define SERVICES_WEBNN_DML_TEST_BASE_H_

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

// GTEST_SKIP() will let method return directly.
#define SKIP_TEST_IF(condition)   \
  do {                            \
    if (condition)                \
      GTEST_SKIP() << #condition; \
  } while (0)

namespace gl {
class GLDisplay;
}

namespace webnn::dml {

bool UseGPUInTests();

class TestBase : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  // Initializing the GL display is required for querying D3D11 device by
  // gl::QueryD3D11DeviceObjectFromANGLE().
  bool InitializeGLDisplay();

 private:
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_TEST_BASE_H_
