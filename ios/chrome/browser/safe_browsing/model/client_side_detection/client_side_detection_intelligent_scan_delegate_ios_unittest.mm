// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_intelligent_scan_delegate_ios.h"

#import <string_view>

#import "base/functional/callback_helpers.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/protobuf_matchers.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "components/optimization_guide/core/model_execution/feature_keys.h"
#import "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#import "components/optimization_guide/core/model_execution/remote_model_executor.h"
#import "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#import "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#import "components/optimization_guide/core/optimization_guide_proto_util.h"
#import "components/optimization_guide/proto/features/scam_detection.pb.h"
#import "components/optimization_guide/proto/model_execution.pb.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace safe_browsing {

namespace {

using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::NiceMock;
using IntelligentScanResult = IntelligentScanDelegate::IntelligentScanResult;
using ModelType = IntelligentScanDelegate::ModelType;
using RemoteModelExecutionCallback =
    optimization_guide::OptimizationGuideModelExecutionResultCallback;

constexpr std::string_view kTestRenderedText = "test rendered text";
constexpr std::string_view kTestBrand = "test_brand";
constexpr std::string_view kTestIntent = "test_intent";
constexpr int kMaxScansPerDay = 5;

}  // namespace

class ClientSideDetectionIntelligentScanDelegateIOSTestBase
    : public PlatformTest {
 protected:
  ClientSideDetectionIntelligentScanDelegateIOSTestBase() = default;
  ~ClientSideDetectionIntelligentScanDelegateIOSTestBase() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    RegisterProfilePrefs(pref_service_.registry());
  }

  void CreateDelegate(bool is_enhanced_protection_enabled = true) {
    if (is_enhanced_protection_enabled) {
      SetSafeBrowsingState(&pref_service_,
                           SafeBrowsingState::ENHANCED_PROTECTION);
    } else {
      SetSafeBrowsingState(&pref_service_,
                           SafeBrowsingState::STANDARD_PROTECTION);
    }
    delegate_ = std::make_unique<ClientSideDetectionIntelligentScanDelegateIOS>(
        pref_service_, &remote_model_executor_);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
  NiceMock<optimization_guide::MockRemoteModelExecutor> remote_model_executor_;
  std::unique_ptr<ClientSideDetectionIntelligentScanDelegateIOS> delegate_;
};

class ClientSideDetectionIntelligentScanDelegateIOSTest
    : public ClientSideDetectionIntelligentScanDelegateIOSTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateIOSTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kClientSideDetectionImageEmbeddingMatch,
          {{"CsdImageEmbeddingMatchWithIntelligentScan", "true"}}},
         {kClientSideDetectionServerModelForScamDetectionIos,
          {{"MaxIntelligentScansPerDayIos",
            base::NumberToString(kMaxScansPerDay)}}}},
        /*disabled_features=*/{kClientSideDetectionKillswitch});
  }
};

// Tests ShouldRequestIntelligentScan returns false when the killswitch is
// enabled.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       ShouldRequestIntelligentScan_ReturnsFalseWhenKillswitchEnabled) {
  base::test::ScopedFeatureList killswitch_feature_list;
  killswitch_feature_list.InitAndEnableFeature(kClientSideDetectionKillswitch);

  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

// Tests ShouldRequestIntelligentScan returns false when enhanced protection is
// disabled.
TEST_F(
    ClientSideDetectionIntelligentScanDelegateIOSTest,
    ShouldRequestIntelligentScan_ReturnsFalseWhenEnhancedProtectionDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);

  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

// Tests ShouldRequestIntelligentScan returns true when the force request
// trigger enables intelligent scan.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       ShouldRequestIntelligentScan_ForceRequest) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  // Intelligent scan explicitly enabled in forced trigger info.
  {
    ClientPhishingRequest verdict;
    verdict.set_client_side_detection_type(
        ClientSideDetectionType::FORCE_REQUEST);
    verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
    EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
  }

  // Intelligent scan disabled in forced trigger info.
  {
    ClientPhishingRequest verdict;
    verdict.set_client_side_detection_type(
        ClientSideDetectionType::FORCE_REQUEST);
    verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(false);
    EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
  }

  // No llama forced trigger info.
  {
    ClientPhishingRequest verdict;
    verdict.set_client_side_detection_type(
        ClientSideDetectionType::FORCE_REQUEST);
    EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
  }
}

// Tests ShouldRequestIntelligentScan returns false for a null verdict pointer.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       ShouldRequestIntelligentScan_ReturnsFalseForNullVerdict) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(nullptr));
}

// Tests ShouldRequestIntelligentScan with image embedding match trigger.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       ShouldRequestIntelligentScan_ImageEmbeddingMatch) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  // Phishing verdict with intelligent scan param enabled.
  {
    ClientPhishingRequest verdict;
    verdict.set_client_side_detection_type(
        ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);
    verdict.set_is_phishing(true);
    EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
  }

  // Non-phishing verdict.
  {
    ClientPhishingRequest verdict;
    verdict.set_client_side_detection_type(
        ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);
    verdict.set_is_phishing(false);
    EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
  }
}

// Tests `ShouldRequestIntelligentScan` returns `false` for a phishing
// `IMAGE_EMBEDDING_MATCH` verdict when the
// `CsdImageEmbeddingMatchWithIntelligentScan` feature parameter is disabled.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       ShouldRequestIntelligentScan_ImageEmbeddingMatchParamDisabled) {
  base::test::ScopedFeatureList image_feature_list;
  image_feature_list.InitAndEnableFeatureWithParameters(
      kClientSideDetectionImageEmbeddingMatch,
      {{"CsdImageEmbeddingMatchWithIntelligentScan", "false"}});

  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);
  verdict.set_is_phishing(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

// Tests `ShouldRequestIntelligentScan` returns `false` for
// `ClientSideDetectionType` values that do not trigger intelligent scans (e.g.,
// `TRIGGER_MODELS`).
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       ShouldRequestIntelligentScan_OtherTriggers) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::TRIGGER_MODELS);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

// Tests `GetIntelligentScanModelType` when the delegate is live versus after
// `delegate_->Shutdown()` is called.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       GetIntelligentScanModelType) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kServerSide);

  delegate_->Shutdown();
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedServerSide);
}

// Tests GetIntelligentScanModelType returns false when the killswitch is
// enabled.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       GetIntelligentScanModelType_ReturnsFalseWhenKillswitchEnabled) {
  base::test::ScopedFeatureList killswitch_feature_list;
  killswitch_feature_list.InitAndEnableFeature(kClientSideDetectionKillswitch);

  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedServerSide);
}

// Tests successful model response processing and metric logging.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       StartIntelligentScan_ModelResponseSuccessful) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  optimization_guide::proto::ScamDetectionRequest expected_request;
  expected_request.set_rendered_text(kTestRenderedText);
  optimization_guide::ModelExecutionOptions expected_options{};

  optimization_guide::proto::ScamDetectionResponse returned_response;
  returned_response.set_brand(kTestBrand);
  returned_response.set_intent(kTestIntent);
  returned_response.set_scam_score(0.85f);

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   EqualsProto(expected_request),
                   ::testing::Eq(expected_options),
                   ::testing::A<RemoteModelExecutionCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(returned_response),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  base::test::TestFuture<IntelligentScanResult> future;
  std::optional<base::UnguessableToken> token = delegate_->StartIntelligentScan(
      std::string(kTestRenderedText), future.GetCallback());
  ASSERT_TRUE(token.has_value());

  IntelligentScanResult result = future.Get();
  EXPECT_TRUE(result.execution_success);
  EXPECT_EQ(result.brand, kTestBrand);
  EXPECT_EQ(result.intent, kTestIntent);
  ASSERT_TRUE(result.scam_score.has_value());
  EXPECT_FLOAT_EQ(*result.scam_score, 0.85f);
  EXPECT_EQ(result.model_version,
            IntelligentScanResult::kDefaultServerModelVersion);
  EXPECT_EQ(result.model_type, ModelType::kServerSide);
  EXPECT_EQ(result.no_info_reason,
            IntelligentScanInfo::NO_INFO_REASON_UNSPECIFIED);
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0u);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelExecutionSuccess", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.ServerSideModelExecutionDuration", 1);
}

// Tests unsuccessful model execution response from remote executor.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       StartIntelligentScan_ModelResponseUnsuccessful) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  optimization_guide::proto::ScamDetectionRequest expected_request;
  expected_request.set_rendered_text(kTestRenderedText);
  optimization_guide::ModelExecutionOptions expected_options{};

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   EqualsProto(expected_request),
                   ::testing::Eq(expected_options),
                   ::testing::A<RemoteModelExecutionCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              base::unexpected(
                  optimization_guide::OptimizationGuideModelExecutionError::
                      FromModelExecutionError(
                          optimization_guide::
                              OptimizationGuideModelExecutionError::
                                  ModelExecutionError::kGenericFailure)),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  base::test::TestFuture<IntelligentScanResult> future;
  std::optional<base::UnguessableToken> token = delegate_->StartIntelligentScan(
      std::string(kTestRenderedText), future.GetCallback());
  ASSERT_TRUE(token.has_value());

  IntelligentScanResult result = future.Get();
  EXPECT_FALSE(result.execution_success);
  EXPECT_EQ(result.brand, "");
  EXPECT_EQ(result.intent, "");
  EXPECT_FALSE(result.scam_score.has_value());
  EXPECT_EQ(result.model_type, ModelType::kServerSide);
  EXPECT_EQ(result.no_info_reason,
            IntelligentScanInfo::SERVER_SIDE_MODEL_OUTPUT_MISSING);
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0u);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelExecutionSuccess", false, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelExecutionError",
      optimization_guide::OptimizationGuideModelExecutionError::
          ModelExecutionError::kGenericFailure,
      1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.ServerSideModelExecutionDuration", 1);
}

// Tests invalid proto payload returned in Any metadata.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       StartIntelligentScan_ModelResponseInvalidProto) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  optimization_guide::proto::Any invalid_any;
  invalid_any.set_type_url("type.googleapis.com/invalid.type");
  invalid_any.set_value("invalid_data");

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   _, _, ::testing::A<RemoteModelExecutionCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              invalid_any, /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  base::test::TestFuture<IntelligentScanResult> future;
  std::optional<base::UnguessableToken> token = delegate_->StartIntelligentScan(
      std::string(kTestRenderedText), future.GetCallback());
  ASSERT_TRUE(token.has_value());

  IntelligentScanResult result = future.Get();
  EXPECT_FALSE(result.execution_success);
  EXPECT_EQ(result.model_type, ModelType::kServerSide);
  EXPECT_EQ(result.no_info_reason,
            IntelligentScanInfo::SERVER_SIDE_MODEL_OUTPUT_MISSING);
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0u);
}

// Tests that disabling Enhanced Protection cancels in-flight inquiries.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       OnPrefsUpdated_EnhancedProtectionDisabledCancelsInquiries) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  RemoteModelExecutionCallback saved_callback;
  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   _, _, ::testing::A<RemoteModelExecutionCallback>()))
      .WillOnce([&](optimization_guide::ModelBasedCapabilityKey,
                    const google::protobuf::MessageLite&,
                    const optimization_guide::ModelExecutionOptions&,
                    RemoteModelExecutionCallback callback) {
        saved_callback = std::move(callback);
      });

  base::test::TestFuture<IntelligentScanResult> future;
  std::optional<base::UnguessableToken> token = delegate_->StartIntelligentScan(
      std::string(kTestRenderedText), future.GetCallback());
  ASSERT_TRUE(token.has_value());
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1u);

  // Disable Enhanced Protection.
  SetSafeBrowsingState(&pref_service_, SafeBrowsingState::STANDARD_PROTECTION);

  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0u);
}

// Tests StartIntelligentScan when the model is unavailable (e.g. after
// Shutdown).
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       StartIntelligentScan_ModelNotAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  delegate_->Shutdown();

  base::test::TestFuture<IntelligentScanResult> future;
  std::optional<base::UnguessableToken> token = delegate_->StartIntelligentScan(
      std::string(kTestRenderedText), future.GetCallback());
  EXPECT_FALSE(token.has_value());

  IntelligentScanResult result = future.Get();
  EXPECT_FALSE(result.execution_success);
  EXPECT_EQ(result.model_type, ModelType::kNotSupportedServerSide);
  EXPECT_EQ(result.no_info_reason,
            IntelligentScanInfo::SERVER_SIDE_MODEL_UNAVAILABLE);
}

// Tests CancelIntelligentScan behaviour.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       CancelIntelligentScan) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  base::test::TestFuture<IntelligentScanResult> future;
  std::optional<base::UnguessableToken> token = delegate_->StartIntelligentScan(
      std::string(kTestRenderedText), future.GetCallback());
  ASSERT_TRUE(token.has_value());

  EXPECT_TRUE(delegate_->CancelIntelligentScan(*token));
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0u);

  // Cancelling an unknown token returns false.
  EXPECT_FALSE(
      delegate_->CancelIntelligentScan(base::UnguessableToken::Create()));
}

// Tests that cancelling an in-flight inquiry prevents the callback from firing
// when remote model execution completes.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       CancelIntelligentScan_PendingRemoteExecutionDoesNotInvokeCallback) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  RemoteModelExecutionCallback saved_callback;
  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   _, _, ::testing::A<RemoteModelExecutionCallback>()))
      .WillOnce([&](optimization_guide::ModelBasedCapabilityKey,
                    const google::protobuf::MessageLite&,
                    const optimization_guide::ModelExecutionOptions&,
                    RemoteModelExecutionCallback callback) {
        saved_callback = std::move(callback);
      });

  base::test::TestFuture<IntelligentScanResult> future;
  std::optional<base::UnguessableToken> token = delegate_->StartIntelligentScan(
      std::string(kTestRenderedText), future.GetCallback());
  ASSERT_TRUE(token.has_value());
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 1u);
  ASSERT_TRUE(saved_callback);

  EXPECT_TRUE(delegate_->CancelIntelligentScan(*token));
  EXPECT_EQ(delegate_->GetAliveInquiryCountForTesting(), 0u);

  optimization_guide::proto::ScamDetectionResponse returned_response;
  returned_response.set_brand(kTestBrand);
  returned_response.set_intent(kTestIntent);

  std::move(saved_callback)
      .Run(optimization_guide::OptimizationGuideModelExecutionResult(
               optimization_guide::AnyWrapProto(returned_response),
               /*execution_info=*/nullptr),
           /*log_entry=*/nullptr);

  EXPECT_FALSE(future.IsReady());
}

// Tests daily quota lookup and enforcement over a sliding 24-hour window.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       StartIntelligentScan_QuotaChecks) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   _, _, _))
      .Times(kMaxScansPerDay + 1);

  for (int i = 0; i < kMaxScansPerDay; ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    delegate_->StartIntelligentScan(std::string(kTestRenderedText),
                                    base::DoNothing());
    histogram_tester_.ExpectBucketCount(
        "SBClientPhishing.ServerSideModelQuotaCountOnLookup", i + 1, 1);
  }

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", false,
      kMaxScansPerDay);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", true, 0);

  // At quota: scan fails immediately without executing model.
  {
    base::test::TestFuture<IntelligentScanResult> future;
    std::optional<base::UnguessableToken> token =
        delegate_->StartIntelligentScan(std::string(kTestRenderedText),
                                        future.GetCallback());
    EXPECT_FALSE(token.has_value());
    ASSERT_TRUE(future.IsReady());
    IntelligentScanResult result = future.Get();
    EXPECT_FALSE(result.execution_success);
    EXPECT_EQ(result.model_type, ModelType::kServerSide);
    EXPECT_EQ(result.no_info_reason,
              IntelligentScanInfo::SERVER_SIDE_MODEL_EXCEED_QUOTA);
  }
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", true, 1);

  // Fast forward by 2 days: quota is reset.
  task_environment_.FastForwardBy(base::Days(2));

  // Scan succeeds again.
  {
    base::test::TestFuture<IntelligentScanResult> future;
    std::optional<base::UnguessableToken> token =
        delegate_->StartIntelligentScan(std::string(kTestRenderedText),
                                        future.GetCallback());
    EXPECT_TRUE(token.has_value());
  }
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", false,
      kMaxScansPerDay + 1);
}

// Tests that failed model executions still consume quota.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       StartIntelligentScan_QuotaConsumedOnModelFailure) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  for (int i = 0; i < kMaxScansPerDay; ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    EXPECT_CALL(remote_model_executor_,
                ExecuteModel(
                    optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                    _, _, _))
        .WillOnce(base::test::RunOnceCallback<3>(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::unexpected(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        FromModelExecutionError(
                            optimization_guide::
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                /*execution_info=*/nullptr),
            /*log_entry=*/nullptr));

    base::test::TestFuture<IntelligentScanResult> future;
    std::optional<base::UnguessableToken> token =
        delegate_->StartIntelligentScan(std::string(kTestRenderedText),
                                        future.GetCallback());
    ASSERT_TRUE(token.has_value());
    EXPECT_FALSE(future.Get().execution_success);
  }

  // Next scan exceeds quota.
  {
    base::test::TestFuture<IntelligentScanResult> future;
    std::optional<base::UnguessableToken> token =
        delegate_->StartIntelligentScan(std::string(kTestRenderedText),
                                        future.GetCallback());
    EXPECT_FALSE(token.has_value());
    ASSERT_TRUE(future.IsReady());
    EXPECT_FALSE(future.Get().execution_success);
    EXPECT_EQ(future.Get().no_info_reason,
              IntelligentScanInfo::SERVER_SIDE_MODEL_EXCEED_QUOTA);
  }
}

// Tests that showing a scam warning refunds scan quota.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       OnScamWarningShown_RefundsQuota) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   _, _, _))
      .Times(kMaxScansPerDay + 1);

  for (int i = 0; i < kMaxScansPerDay; ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    delegate_->StartIntelligentScan(std::string(kTestRenderedText),
                                    base::DoNothing());
  }

  // Reached quota.
  {
    base::test::TestFuture<IntelligentScanResult> future;
    std::optional<base::UnguessableToken> token =
        delegate_->StartIntelligentScan(std::string(kTestRenderedText),
                                        future.GetCallback());
    EXPECT_FALSE(token.has_value());
  }

  // Warning shown: refunds quota.
  delegate_->OnScamWarningShown();
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelQuotaCountOnScamWarningShown",
      kMaxScansPerDay, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ServerSideModelPrefEmptyWhenRemovingQuota", false, 1);

  // Now scan succeeds because quota was refunded.
  {
    base::test::TestFuture<IntelligentScanResult> future;
    std::optional<base::UnguessableToken> token =
        delegate_->StartIntelligentScan(std::string(kTestRenderedText),
                                        future.GetCallback());
    EXPECT_TRUE(token.has_value());
  }
}

// Tests ShouldShowScamWarning evaluation for all verdict types.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSTest,
       ShouldShowScamWarning) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  EXPECT_FALSE(delegate_->ShouldShowScamWarning(std::nullopt));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_UNSPECIFIED));
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_TELEMETRY));

  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_3));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_4));
  EXPECT_TRUE(delegate_->ShouldShowScamWarning(
      IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT));

  // Unknown enum values do not show warning.
  EXPECT_FALSE(delegate_->ShouldShowScamWarning(
      static_cast<IntelligentScanVerdict>(12345)));
}

class ClientSideDetectionIntelligentScanDelegateIOSRolloutTest
    : public ClientSideDetectionIntelligentScanDelegateIOSTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateIOSRolloutTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kClientSideDetectionImageEmbeddingMatch,
          {{"CsdImageEmbeddingMatchWithIntelligentScan", "true"}}},
         {kClientSideDetectionServerModelForScamDetectionIos,
          {{"MaxIntelligentScansPerDayIos",
            base::NumberToString(kMaxScansPerDay)}}},
         {kClientSideDetectionServerModelRolloutIos,
          {{"ModelVersion", "2000"}}}},
        /*disabled_features=*/{kClientSideDetectionKillswitch});
  }
};

// Tests server model rollout version flag overrides default version.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSRolloutTest,
       UsesRolloutVersionWhenFlagIsEnabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);

  optimization_guide::proto::ScamDetectionRequest expected_request;
  expected_request.set_rendered_text(kTestRenderedText);
  optimization_guide::ModelExecutionOptions expected_options{};

  optimization_guide::proto::ScamDetectionResponse returned_response;
  returned_response.set_brand(kTestBrand);
  returned_response.set_intent(kTestIntent);

  EXPECT_CALL(
      remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kScamDetection,
                   EqualsProto(expected_request),
                   ::testing::Eq(expected_options),
                   ::testing::A<RemoteModelExecutionCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(returned_response),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  base::test::TestFuture<IntelligentScanResult> future;
  std::optional<base::UnguessableToken> token = delegate_->StartIntelligentScan(
      std::string(kTestRenderedText), future.GetCallback());
  ASSERT_TRUE(token.has_value());

  IntelligentScanResult result = future.Get();
  EXPECT_TRUE(result.execution_success);
  EXPECT_EQ(result.model_version, 2000);
  EXPECT_FALSE(result.scam_score.has_value());
}

class ClientSideDetectionIntelligentScanDelegateIOSServerModelDisabledTest
    : public ClientSideDetectionIntelligentScanDelegateIOSTestBase {
 protected:
  ClientSideDetectionIntelligentScanDelegateIOSServerModelDisabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            kClientSideDetectionKillswitch,
            kClientSideDetectionServerModelForScamDetectionIos});
  }
};

// Tests delegate behavior when server model flag is disabled.
TEST_F(ClientSideDetectionIntelligentScanDelegateIOSServerModelDisabledTest,
       GetIntelligentScanModelType_ServerModelDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_EQ(delegate_->GetIntelligentScanModelType(
                /*log_failed_eligibility_reason=*/false),
            ModelType::kNotSupportedOnDevice);
}

}  // namespace safe_browsing
