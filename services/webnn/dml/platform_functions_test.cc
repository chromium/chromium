// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/platform_functions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::dml {

class WebNNPlatformFunctionsTest : public testing::Test {};

TEST_F(WebNNPlatformFunctionsTest, AllFunctionsLoaded) {
  PlatformFunctions* platformFunctions = PlatformFunctions::GetInstance();
  ASSERT_NE(platformFunctions, nullptr);
  EXPECT_TRUE(platformFunctions->d3d12_create_device_proc());
  EXPECT_TRUE(platformFunctions->dml_create_device_proc());
}

}  // namespace webnn::dml
