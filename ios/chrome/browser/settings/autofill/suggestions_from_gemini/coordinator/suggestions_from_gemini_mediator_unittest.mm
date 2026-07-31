// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_mediator.h"

#import "components/personal_context/core/personal_context_prefs.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class SuggestionsFromGeminiMediatorTest : public PlatformTest {
 protected:
  SuggestionsFromGeminiMediatorTest() {
    pref_service_.registry()->RegisterBooleanPref(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        true);
    PrefBackedBoolean* personalContextSwitchEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:&pref_service_
                   prefName:personal_context::prefs::
                                kPersonalContextInAutofillSettingsToggleStatus];
    mediator_ = [[SuggestionsFromGeminiMediator alloc]
        initWithPrefBackedBoolean:personalContextSwitchEnabled];
    mock_consumer_ = OCMProtocolMock(@protocol(SuggestionsFromGeminiConsumer));
    mock_delegate_ =
        OCMProtocolMock(@protocol(SuggestionsFromGeminiMediatorDelegate));
    mediator_.consumer = mock_consumer_;
    mediator_.delegate = mock_delegate_;
  }

  ~SuggestionsFromGeminiMediatorTest() override { [mediator_ disconnect]; }

  web::WebTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  SuggestionsFromGeminiMediator* mediator_;
  id mock_consumer_;
  id mock_delegate_;
};

// Tests that the mediator is correctly initialized.
TEST_F(SuggestionsFromGeminiMediatorTest, TestInitialization) {
  EXPECT_NE(nil, mediator_);
}

// Tests that setting the consumer updates it with the current preference state.
TEST_F(SuggestionsFromGeminiMediatorTest, TestInitializationUpdatesConsumer) {
  OCMExpect([mock_consumer_ setSuggestionsFromGeminiSwitchOn:YES]);
  mediator_.consumer = mock_consumer_;
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that user toggle actions update the pref.
TEST_F(SuggestionsFromGeminiMediatorTest, TestToggleSwitchUpdatesPref) {
  [mediator_ didToggleSuggestionsFromGeminiSwitch:NO];
  EXPECT_FALSE(pref_service_.GetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus));

  [mediator_ didToggleSuggestionsFromGeminiSwitch:YES];
  EXPECT_TRUE(pref_service_.GetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus));
}

// Tests that external pref changes propagate to the consumer.
TEST_F(SuggestionsFromGeminiMediatorTest, TestPrefChangeUpdatesConsumer) {
  OCMExpect([mock_consumer_ setSuggestionsFromGeminiSwitchOn:NO]);
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      false);
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that tapping manage connected apps triggers delegate.
TEST_F(SuggestionsFromGeminiMediatorTest,
       TestSelectManageConnectedAppsTriggersDelegate) {
  OCMExpect([mock_delegate_
      suggestionsFromGeminiMediatorOpenConnectedApps:mediator_]);
  [mediator_ didSelectManageConnectedApps];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

}  // namespace
