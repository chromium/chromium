// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/backend_model_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/on_device_model/android/native_j_unittests_jni_headers/OnDeviceModelBridgeNativeUnitTestHelper_jni.h"

namespace on_device_model {
namespace {

using ::testing::ElementsAre;

constexpr optimization_guide::proto::ModelExecutionFeature kFeature =
    optimization_guide::proto::ModelExecutionFeature::
        MODEL_EXECUTION_FEATURE_SCAM_DETECTION;

class BackendModelImplAndroidTest : public testing::Test {
 public:
  BackendModelImplAndroidTest() = default;
  ~BackendModelImplAndroidTest() override = default;

  void SetUp() override {
    env_ = base::android::AttachCurrentThread();
    java_helper_ = Java_OnDeviceModelBridgeNativeUnitTestHelper_create(env_);

    model_ = std::make_unique<BackendModelImplAndroid>(kFeature);
  }

  mojom::SessionParamsPtr MakeSessionParams(int top_k, float temperature) {
    auto params = mojom::SessionParams::New();
    params->top_k = top_k;
    params->temperature = temperature;
    return params;
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
  std::unique_ptr<BackendModel> model_;
};

TEST_F(BackendModelImplAndroidTest, GenerateWithDefaultFactory) {
  std::unique_ptr<BackendSession> session = model_->CreateSession(
      /*adaptation=*/nullptr,
      MakeSessionParams(/*top_k=*/3, /*temperature=*/1.0f));

  TestResponseHolder response_holder;
  session->Generate(mojom::GenerateOptions::New(), response_holder.BindRemote(),
                    /*on_complete=*/base::DoNothing());
  response_holder.WaitForCompletion();
  EXPECT_THAT(response_holder.responses(), ElementsAre("AiCore response"));
}

TEST_F(BackendModelImplAndroidTest, AppendAndGenerate) {
  Java_OnDeviceModelBridgeNativeUnitTestHelper_setMockAiCoreSessionFactory(
      env_, java_helper_);

  std::unique_ptr<BackendSession> session = model_->CreateSession(
      /*adaptation=*/nullptr,
      MakeSessionParams(/*top_k=*/3, /*temperature=*/1.0f));
  Java_OnDeviceModelBridgeNativeUnitTestHelper_verifySessionParams(
      env_, java_helper_, kFeature, /*topK=*/3, /*temperature=*/1.0f);

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

TEST_F(BackendModelImplAndroidTest, ContextIsNotClearedOnNewGenerate) {
  Java_OnDeviceModelBridgeNativeUnitTestHelper_setMockAiCoreSessionFactory(
      env_, java_helper_);

  std::unique_ptr<BackendSession> session = model_->CreateSession(
      /*adaptation=*/nullptr,
      MakeSessionParams(/*top_k=*/3, /*temperature=*/1.0f));

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

TEST_F(BackendModelImplAndroidTest, NativeSessionDeletionIsSafe) {
  Java_OnDeviceModelBridgeNativeUnitTestHelper_setMockAiCoreSessionFactory(
      env_, java_helper_);

  std::unique_ptr<BackendSession> session = model_->CreateSession(
      /*adaptation=*/nullptr,
      MakeSessionParams(/*top_k=*/3, /*temperature=*/1.0f));

  Java_OnDeviceModelBridgeNativeUnitTestHelper_setCompleteAsync(env_,
                                                                java_helper_);

  TestResponseHolder response_holder;
  session->Generate(mojom::GenerateOptions::New(), response_holder.BindRemote(),
                    /*on_complete=*/base::DoNothing());

  // Delete the native session manually and ensure async completion doesn't
  // cause a crash.
  session.reset();
  Java_OnDeviceModelBridgeNativeUnitTestHelper_resumeOnCompleteCallback(
      env_, java_helper_);
}

}  // namespace
}  // namespace on_device_model
