// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/callback_registry.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsNull;

class CallbackRegistryTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CallbackRegistryTest, RegisterWithNoParam) {
  ClosureRegistry registry;

  base::MockCallback<base::RepeatingCallback<void()>> callback;
  auto registration = registry.Register(callback.Get());
  EXPECT_TRUE(registration);

  EXPECT_CALL(callback, Run());
  registry.Notify();
  task_environment_.RunUntilIdle();
}

TEST_F(CallbackRegistryTest, RegisterWithOneParam) {
  CallbackRegistry<void(int)> registry;

  base::MockCallback<base::RepeatingCallback<void(int)>> callback;
  auto registration = registry.Register(callback.Get());
  EXPECT_TRUE(registration);

  EXPECT_CALL(callback, Run(1));
  registry.Notify(1);
  task_environment_.RunUntilIdle();
}

TEST_F(CallbackRegistryTest, RegisterWithTwoParams) {
  CallbackRegistry<void(int, int)> registry;

  base::MockCallback<base::RepeatingCallback<void(int, int)>> callback;
  auto registration = registry.Register(callback.Get());
  EXPECT_TRUE(registration);

  EXPECT_CALL(callback, Run(1, 2));
  registry.Notify(1, 2);
  task_environment_.RunUntilIdle();
}

TEST_F(CallbackRegistryTest, RegisterWithMoveOnlyParam) {
  CallbackRegistry<void(std::unique_ptr<int>)> registry;

  base::MockCallback<base::RepeatingCallback<void(std::unique_ptr<int>)>>
      callback;
  auto registration = registry.Register(callback.Get());
  EXPECT_TRUE(registration);

  EXPECT_CALL(callback, Run(_));
  registry.Notify(std::make_unique<int>(1));
  task_environment_.RunUntilIdle();
}

TEST_F(CallbackRegistryTest, RegisterWithPointerParam) {
  CallbackRegistry<void(int*)> registry;

  base::MockCallback<base::RepeatingCallback<void(int*)>> callback;
  auto registration = registry.Register(callback.Get());
  EXPECT_TRUE(registration);

  EXPECT_CALL(callback, Run(IsNull()));
  registry.Notify(nullptr);
  task_environment_.RunUntilIdle();
}

TEST_F(CallbackRegistryTest, RegisterWithReferenceParam) {
  CallbackRegistry<void(const int&)> registry;

  base::MockCallback<base::RepeatingCallback<void(const int&)>> callback;
  auto registration = registry.Register(callback.Get());
  EXPECT_TRUE(registration);

  int i = 1;
  EXPECT_CALL(callback, Run(i));
  registry.Notify(i);
  task_environment_.RunUntilIdle();
}

TEST_F(CallbackRegistryTest, RegisterAfterNotify) {
  ClosureRegistry registry;

  base::MockCallback<base::RepeatingClosure> callback_1;
  auto registration_1 = registry.Register(callback_1.Get());
  EXPECT_TRUE(registration_1);

  EXPECT_CALL(callback_1, Run());
  registry.Notify();
  task_environment_.RunUntilIdle();

  base::MockCallback<base::RepeatingClosure> callback_2;
  auto registration_2 = registry.Register(callback_2.Get());
  EXPECT_TRUE(registration_2);

  EXPECT_CALL(callback_1, Run());
  EXPECT_CALL(callback_2, Run());
  registry.Notify();
  task_environment_.RunUntilIdle();
}

TEST_F(CallbackRegistryTest, EmptyRegistry) {
  ClosureRegistry registry;
  registry.Notify();
}

TEST_F(CallbackRegistryTest, UnregisterCallback) {
  ClosureRegistry registry;

  base::MockCallback<base::RepeatingClosure> callback_1;
  base::MockCallback<base::RepeatingClosure> callback_2;
  auto registration_1 = registry.Register(callback_1.Get());
  auto registration_2 = registry.Register(callback_2.Get());
  EXPECT_TRUE(registration_1);
  EXPECT_TRUE(registration_2);

  EXPECT_CALL(callback_1, Run());
  EXPECT_CALL(callback_2, Run());
  registry.Notify();
  task_environment_.RunUntilIdle();

  registration_1.reset();
  EXPECT_CALL(callback_2, Run());
  registry.Notify();
  task_environment_.RunUntilIdle();

  registration_2.reset();
  registry.Notify();
  task_environment_.RunUntilIdle();
}

TEST_F(CallbackRegistryTest, RegisterDuringNotification) {
  ClosureRegistry registry;

  base::MockCallback<base::RepeatingClosure> callback_1;
  base::MockCallback<base::RepeatingClosure> callback_2;
  auto registration_1 = registry.Register(callback_1.Get());
  std::unique_ptr<CallbackRegistration> registration_2;
  EXPECT_TRUE(registration_1);

  // Register callback_2 during callback_1's notification run.
  EXPECT_CALL(callback_1, Run()).WillOnce(Invoke([&]() {
    registration_2 = registry.Register(callback_2.Get());
  }));
  registry.Notify();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(registration_2);

  EXPECT_CALL(callback_1, Run());
  EXPECT_CALL(callback_2, Run());
  registry.Notify();
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace media
