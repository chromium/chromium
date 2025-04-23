// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/platform_functions_ort.h"

#include "services/webnn/ort/test_base_ort.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::ort {

class WebNNOrtPlatformFunctionsTest : public testing::Test {
 public:
  void SetUp() override;
};

void WebNNOrtPlatformFunctionsTest::SetUp() {
  // Skip tests if the loading platform functions fail.
  // In order to be able to run this test suite successfully, the developer
  // needs to place a copy of onnxruntime.dll which supports ORT_API_VERSION
  // defined in onnxruntime_c_api.h into Chromium module folder before.
  SKIP_TEST_IF(!PlatformFunctions::GetInstance());
}

TEST_F(WebNNOrtPlatformFunctionsTest, AllFunctionsLoaded) {
  PlatformFunctions* platformFunctions = PlatformFunctions::GetInstance();
  EXPECT_TRUE(platformFunctions->ort_api());
  EXPECT_TRUE(platformFunctions->ort_model_editor_api());
}

}  // namespace webnn::ort
