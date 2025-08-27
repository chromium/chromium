// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/backend_model_impl_android.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/android/backend_session_impl_android.h"
#include "services/on_device_model/android/on_device_model_bridge_native_unittest_helper.h"
#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    model_ = std::make_unique<BackendModelImplAndroid>(kFeature);
  }

  mojom::SessionParamsPtr MakeSessionParams(int top_k, float temperature) {
    auto params = mojom::SessionParams::New();
    params->top_k = top_k;
    params->temperature = temperature;
    return params;
  }

  mojom::GenerateOptionsPtr MakeGenerateOptions(int max_output_tokens) {
    auto options = mojom::GenerateOptions::New();
    options->max_output_tokens = max_output_tokens;
    return options;
  }

  mojom::AppendOptionsPtr MakeInput(std::vector<ml::InputPiece> input) {
    auto options = mojom::AppendOptions::New();
    options->input = mojom::Input::New(std::move(input));
    return options;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  OnDeviceModelBridgeNativeUnitTestHelper java_helper_;
  std::unique_ptr<BackendModel> model_;
};

TEST_F(BackendModelImplAndroidTest, GenerateWithDefaultFactory) {
  std::unique_ptr<BackendSession> session = model_->CreateSession(
      /*adaptation=*/nullptr,
      MakeSessionParams(/*top_k=*/3, /*temperature=*/1.0f));

  TestResponseHolder response_holder;
  session->Generate(MakeGenerateOptions(/*max_output_tokens=*/100),
                    response_holder.BindRemote(),
                    /*on_complete=*/base::DoNothing());
  response_holder.WaitForCompletion();
  EXPECT_TRUE(response_holder.responses().empty());
  histogram_tester_.ExpectUniqueSample(
      "OnDeviceModel.Android.GenerateResult",
      BackendSessionImplAndroid::GenerateResult::kApiNotAvailable, 1);
  histogram_tester_.ExpectUniqueSample(
      "OnDeviceModel.Android.GenerateResult.ScamDetection",
      BackendSessionImplAndroid::GenerateResult::kApiNotAvailable, 1);
}

TEST_F(BackendModelImplAndroidTest, AppendAndGenerate) {
  java_helper_.SetMockAiCoreFactory();

  std::unique_ptr<BackendSession> session = model_->CreateSession(
      /*adaptation=*/nullptr,
      MakeSessionParams(/*top_k=*/3, /*temperature=*/1.0f));
  java_helper_.VerifySessionParams(/*index=*/0, kFeature, /*top_k=*/3,
                                   /*temperature=*/1.0f);

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
  session->Generate(MakeGenerateOptions(/*max_output_tokens=*/100),
                    response_holder.BindRemote(),
                    /*on_complete=*/base::DoNothing());
  response_holder.WaitForCompletion();
  EXPECT_THAT(
      response_holder.responses(),
      ElementsAre(
          "<system>mock system input<end><user>mock user input<end><model>"));
  java_helper_.VerifyGenerateOptions(/*index=*/0, /*max_output_tokens=*/100);
  histogram_tester_.ExpectUniqueSample(
      "OnDeviceModel.Android.GenerateResult",
      BackendSessionImplAndroid::GenerateResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample(
      "OnDeviceModel.Android.GenerateResult.ScamDetection",
      BackendSessionImplAndroid::GenerateResult::kSuccess, 1);
}

TEST_F(BackendModelImplAndroidTest, GenerateWithUnknownError) {
  java_helper_.SetMockAiCoreFactory();

  std::unique_ptr<BackendSession> session = model_->CreateSession(
      /*adaptation=*/nullptr,
      MakeSessionParams(/*top_k=*/3, /*temperature=*/1.0f));
  java_helper_.SetGenerateResult(
      BackendSessionImplAndroid::GenerateResult::kUnknownError);

  TestResponseHolder response_holder;
  session->Generate(MakeGenerateOptions(/*max_output_tokens=*/100),
                    response_holder.BindRemote(),
                    /*on_complete=*/base::DoNothing());
  response_holder.WaitForCompletion();
  EXPECT_THAT(response_holder.responses(), ElementsAre(""));
  histogram_tester_.ExpectUniqueSample(
      "OnDeviceModel.Android.GenerateResult",
      BackendSessionImplAndroid::GenerateResult::kUnknownError, 1);
  histogram_tester_.ExpectUniqueSample(
      "OnDeviceModel.Android.GenerateResult.ScamDetection",
      BackendSessionImplAndroid::GenerateResult::kUnknownError, 1);
}

TEST_F(BackendModelImplAndroidTest, ContextIsNotClearedOnNewGenerate) {
  java_helper_.SetMockAiCoreFactory();

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
    session->Generate(MakeGenerateOptions(/*max_output_tokens=*/100),
                      response_holder.BindRemote(),
                      /*on_complete=*/base::DoNothing());
    response_holder.WaitForCompletion();
    EXPECT_THAT(response_holder.responses(), ElementsAre("mock input"));
  }
  histogram_tester_.ExpectUniqueSample(
      "OnDeviceModel.Android.GenerateResult",
      BackendSessionImplAndroid::GenerateResult::kSuccess, 2);
  histogram_tester_.ExpectUniqueSample(
      "OnDeviceModel.Android.GenerateResult.ScamDetection",
      BackendSessionImplAndroid::GenerateResult::kSuccess, 2);
}

TEST_F(BackendModelImplAndroidTest, GenerateCallbacksOnDifferentThread) {
  java_helper_.SetMockAiCoreFactory();

  std::unique_ptr<BackendSession> session = model_->CreateSession(
      /*adaptation=*/nullptr,
      MakeSessionParams(/*top_k=*/3, /*temperature=*/1.0f));

  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back("mock input");
    session->Append(MakeInput(std::move(pieces)), /*client=*/{},
                    /*on_complete=*/base::DoNothing());
  }

  java_helper_.SetCallbackOnDifferentThread();

  TestResponseHolder response_holder;
  session->Generate(MakeGenerateOptions(/*max_output_tokens=*/100),
                    response_holder.BindRemote(),
                    /*on_complete=*/base::DoNothing());
  response_holder.WaitForCompletion();
  EXPECT_THAT(response_holder.responses(), ElementsAre("mock input"));
  histogram_tester_.ExpectUniqueSample(
      "OnDeviceModel.Android.GenerateResult",
      BackendSessionImplAndroid::GenerateResult::kSuccess, 1);
}

TEST_F(BackendModelImplAndroidTest, NativeSessionDeletionIsSafe) {
  java_helper_.SetMockAiCoreFactory();

  std::unique_ptr<BackendSession> session = model_->CreateSession(
      /*adaptation=*/nullptr,
      MakeSessionParams(/*top_k=*/3, /*temperature=*/1.0f));

  java_helper_.SetCompleteAsync();

  TestResponseHolder response_holder;
  session->Generate(MakeGenerateOptions(/*max_output_tokens=*/100),
                    response_holder.BindRemote(),
                    /*on_complete=*/base::DoNothing());

  // Delete the native session manually and ensure async completion doesn't
  // cause a crash.
  session.reset();
  java_helper_.ResumeOnCompleteCallback();
}

TEST_F(BackendModelImplAndroidTest, CloneSession) {
  java_helper_.SetMockAiCoreFactory();

  std::unique_ptr<BackendSession> session = model_->CreateSession(
      /*adaptation=*/nullptr,
      MakeSessionParams(/*top_k=*/3, /*temperature=*/1.0f));
  java_helper_.VerifySessionParams(/*index=*/0, kFeature, /*top_k=*/3,
                                   /*temperature=*/1.0f);

  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back("mock input");
    session->Append(MakeInput(std::move(pieces)), /*client=*/{},
                    /*on_complete=*/base::DoNothing());
  }

  std::unique_ptr<BackendSession> cloned_session = session->Clone();
  // A new Java session should be created for the cloned session.
  java_helper_.VerifySessionParams(/*index=*/1, kFeature, /*top_k=*/3,
                                   /*temperature=*/1.0f);

  // Generate with the cloned session. The context should be cloned too.
  {
    TestResponseHolder response_holder;
    cloned_session->Generate(MakeGenerateOptions(/*max_output_tokens=*/100),
                             response_holder.BindRemote(),
                             /*on_complete=*/base::DoNothing());
    response_holder.WaitForCompletion();
    EXPECT_THAT(response_holder.responses(), ElementsAre("mock input"));
    java_helper_.VerifyGenerateOptions(/*index=*/1, /*max_output_tokens=*/100);
  }

  // Add more context to the original session and generate.
  {
    std::vector<ml::InputPiece> pieces;
    pieces.push_back(" more context");
    session->Append(MakeInput(std::move(pieces)), /*client=*/{},
                    /*on_complete=*/base::DoNothing());
  }
  {
    TestResponseHolder response_holder;
    session->Generate(MakeGenerateOptions(/*max_output_tokens=*/100),
                      response_holder.BindRemote(),
                      /*on_complete=*/base::DoNothing());
    response_holder.WaitForCompletion();
    EXPECT_THAT(response_holder.responses(),
                ElementsAre("mock input more context"));
  }

  // Generate with the cloned session again to ensure its context is unchanged.
  {
    TestResponseHolder response_holder;
    cloned_session->Generate(MakeGenerateOptions(/*max_output_tokens=*/100),
                             response_holder.BindRemote(),
                             /*on_complete=*/base::DoNothing());
    response_holder.WaitForCompletion();
    EXPECT_THAT(response_holder.responses(), ElementsAre("mock input"));
  }
}

}  // namespace
}  // namespace on_device_model
