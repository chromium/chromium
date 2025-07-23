// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/backend_impl_android.h"

#include "base/test/task_environment.h"
#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_model {
namespace {

using ::testing::ElementsAre;

class BackendImplAndroidTest : public testing::Test {
 public:
  BackendImplAndroidTest() = default;
  ~BackendImplAndroidTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BackendImplAndroidTest, Generate) {
  BackendImplAndroid backend;
  auto model_result = backend.CreateWithResult(
      /*params=*/nullptr, /*on_complete=*/base::DoNothing());
  ASSERT_TRUE(model_result.has_value());
  std::unique_ptr<BackendModel> model = std::move(model_result.value());

  std::unique_ptr<BackendSession> session =
      model->CreateSession(/*adaptation=*/nullptr, /*params=*/nullptr);

  TestResponseHolder response_holder;
  session->Generate(mojom::GenerateOptions::New(), response_holder.BindRemote(),
                    /*on_complete=*/base::DoNothing());
  response_holder.WaitForCompletion();
  EXPECT_THAT(response_holder.responses(), ElementsAre("AiCore response"));
}

}  // namespace
}  // namespace on_device_model
