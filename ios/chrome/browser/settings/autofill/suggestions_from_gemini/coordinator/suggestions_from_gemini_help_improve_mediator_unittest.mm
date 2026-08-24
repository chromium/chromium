// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_help_improve_mediator.h"

#import "components/optimization_guide/core/feature_registry/feature_registration.h"
#import "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_help_improve_consumer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class SuggestionsFromGeminiHelpImproveMediatorTest : public PlatformTest {
 protected:
  SuggestionsFromGeminiHelpImproveMediatorTest() {
    pref_service_.registry()->RegisterIntegerPref(
        optimization_guide::prefs::kFindAndFillWithGeminiSettings, 0);
    mediator_ = [[SuggestionsFromGeminiHelpImproveMediator alloc]
        initWithPrefService:&pref_service_];
    mock_consumer_ =
        OCMProtocolMock(@protocol(SuggestionsFromGeminiHelpImproveConsumer));
    mediator_.consumer = mock_consumer_;
  }

  ~SuggestionsFromGeminiHelpImproveMediatorTest() override {}

  web::WebTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  SuggestionsFromGeminiHelpImproveMediator* mediator_;
  id mock_consumer_;
};

// Tests that the mediator is correctly initialized.
TEST_F(SuggestionsFromGeminiHelpImproveMediatorTest, TestInitialization) {
  EXPECT_NE(nil, mediator_);
}

// Tests that setting the consumer updates it with the current preference state.
TEST_F(SuggestionsFromGeminiHelpImproveMediatorTest,
       TestInitializationUpdatesConsumer) {
  mediator_.consumer = nil;

  OCMExpect(
      [mock_consumer_ setSuggestionsFromGeminiPolicyState:
                          SuggestionsFromGeminiPolicyState::kFullyAllowed]);
  mediator_.consumer = mock_consumer_;
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that setting the policy status to disabled propagates to the consumer.
TEST_F(SuggestionsFromGeminiHelpImproveMediatorTest,
       TestPolicyDisablePropagatesToConsumer) {
  mediator_.consumer = nil;

  pref_service_.SetManagedPref(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings,
      std::make_unique<base::Value>(
          static_cast<int>(optimization_guide::model_execution::prefs::
                               ModelExecutionEnterprisePolicyValue::kDisable)));

  OCMExpect(
      [mock_consumer_ setSuggestionsFromGeminiPolicyState:
                          SuggestionsFromGeminiPolicyState::kFullyDisabled]);
  mediator_.consumer = mock_consumer_;
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that setting the policy status to allow-without-logging propagates to
// the consumer.
TEST_F(SuggestionsFromGeminiHelpImproveMediatorTest,
       TestPolicyAllowWithoutLoggingPropagatesToConsumer) {
  mediator_.consumer = nil;

  pref_service_.SetManagedPref(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings,
      std::make_unique<base::Value>(static_cast<int>(
          optimization_guide::model_execution::prefs::
              ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging)));

  OCMExpect(
      [mock_consumer_ setSuggestionsFromGeminiPolicyState:
                          SuggestionsFromGeminiPolicyState::kLoggingDisabled]);
  mediator_.consumer = mock_consumer_;
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

}  // namespace
