// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/backend_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/test/task_environment.h"
#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/on_device_model/android/native_j_unittests_jni_headers/OnDeviceModelBridgeNativeUnitTestHelper_jni.h"

namespace on_device_model {
namespace {

using ::testing::ElementsAre;

class BackendImplAndroidTest : public testing::Test {
 public:
  BackendImplAndroidTest() = default;
  ~BackendImplAndroidTest() override = default;

  void SetUp() override {
    env_ = base::android::AttachCurrentThread();
    java_helper_ = Java_OnDeviceModelBridgeNativeUnitTestHelper_create(env_);

    auto model_result = BackendImplAndroid().CreateWithResult(
        /*params=*/nullptr, /*on_complete=*/base::DoNothing());
    ASSERT_TRUE(model_result.has_value());
    model_ = std::move(model_result.value());
  }

  mojom::AppendOptionsPtr MakeInput(std::vector<ml::InputPiece> input) {
    auto options = mojom::AppendOptions::New();
    options->input = mojom::Input::New(std::move(input));
    return options;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<JNIEnv> env_;
  base::android::ScopedJavaGlobalRef<jobject> java_helper_;
  BackendImplAndroid backend_;
  std::unique_ptr<BackendModel> model_;
};

TEST_F(BackendImplAndroidTest, GenerateWithDefaultFactory) {
  std::unique_ptr<BackendSession> session =
      model_->CreateSession(/*adaptation=*/nullptr, /*params=*/nullptr);

  TestResponseHolder response_holder;
  session->Generate(mojom::GenerateOptions::New(), response_holder.BindRemote(),
                    /*on_complete=*/base::DoNothing());
  response_holder.WaitForCompletion();
  EXPECT_THAT(response_holder.responses(), ElementsAre("AiCore response"));
}

TEST_F(BackendImplAndroidTest, AppendAndGenerate) {
  Java_OnDeviceModelBridgeNativeUnitTestHelper_setMockAiCoreSessionFactory(
      env_, java_helper_);

  std::unique_ptr<BackendSession> session =
      model_->CreateSession(/*adaptation=*/nullptr, /*params=*/nullptr);

  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back(ml::Token::kSystem);
    pieces.push_back("mock system input");
    pieces.push_back(ml::Token::kEnd);
    session->Append(MakeInput(std::move(pieces)), /*client=*/{},
                    /*on_complete=*/base::DoNothing());
  }
  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back(ml::Token::kUser);
    pieces.push_back("mock user input");
    pieces.push_back(ml::Token::kEnd);
    session->Append(MakeInput(std::move(pieces)), /*client=*/{},
                    /*on_complete=*/base::DoNothing());
  }
  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back(ml::Token::kModel);
    session->Append(MakeInput(std::move(pieces)), /*client=*/{},
                    /*on_complete=*/base::DoNothing());
  }

  TestResponseHolder response_holder;
  session->Generate(mojom::GenerateOptions::New(), response_holder.BindRemote(),
                    /*on_complete=*/base::DoNothing());
  response_holder.WaitForCompletion();
  EXPECT_THAT(
      response_holder.responses(),
      ElementsAre(
          "<system>mock system input<end><user>mock user input<end><model>"));
}

TEST_F(BackendImplAndroidTest, ContextIsNotClearedOnNewGenerate) {
  Java_OnDeviceModelBridgeNativeUnitTestHelper_setMockAiCoreSessionFactory(
      env_, java_helper_);

  std::unique_ptr<BackendSession> session =
      model_->CreateSession(/*adaptation=*/nullptr, /*params=*/nullptr);

  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back("mock input");
    session->Append(MakeInput(std::move(pieces)), /*client=*/{},
                    /*on_complete=*/base::DoNothing());
  }

  {
    TestResponseHolder response_holder;
    session->Generate(mojom::GenerateOptions::New(),
                      response_holder.BindRemote(),
                      /*on_complete=*/base::DoNothing());
    response_holder.WaitForCompletion();
    EXPECT_THAT(response_holder.responses(), ElementsAre("mock input"));
  }

  {
    TestResponseHolder response_holder;
    session->Generate(mojom::GenerateOptions::New(),
                      response_holder.BindRemote(),
                      /*on_complete=*/base::DoNothing());
    response_holder.WaitForCompletion();
    EXPECT_THAT(response_holder.responses(), ElementsAre("mock input"));
  }
}

}  // namespace
}  // namespace on_device_model
