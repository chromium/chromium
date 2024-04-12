// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/platform_functions.h"

#include "services/webnn/dml/test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::dml {

class WebNNPlatformFunctionsTest : public testing::Test {
 public:
  void SetUp() override;
};
void WebNNPlatformFunctionsTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  // Skip tests if the loading platform functions fail.
  SKIP_TEST_IF(!PlatformFunctions::GetInstance());
}

TEST_F(WebNNPlatformFunctionsTest, AllFunctionsLoaded) {
  PlatformFunctions* platformFunctions = PlatformFunctions::GetInstance();
  EXPECT_TRUE(platformFunctions->d3d12_create_device_proc());
  EXPECT_TRUE(platformFunctions->dml_create_device1_proc());
}

}  // namespace webnn::dml
