// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_tool_factory.h"

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ActuationToolFactoryTest = PlatformTest;

TEST_F(ActuationToolFactoryTest, CreateKnownTool_DefaultProto) {
  ActuationToolFactory factory;
  optimization_guide::proto::Action tool;
  std::unique_ptr<ActuationTool> result = factory.CreateTool(tool);
  EXPECT_EQ(nullptr, result);
}
