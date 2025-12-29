// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_action_factory.h"

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actions/actuation_command.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ActuationActionFactoryTest = PlatformTest;

TEST_F(ActuationActionFactoryTest, CreateKnownCommand_DefaultProto) {
  ActuationActionFactory factory;
  optimization_guide::proto::Action action;
  std::unique_ptr<ActuationCommand> result =
      factory.CreateActionCommand(action);
  EXPECT_EQ(nullptr, result);
}
