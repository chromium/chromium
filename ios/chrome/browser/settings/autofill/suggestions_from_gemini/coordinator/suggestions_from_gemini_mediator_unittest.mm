// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_mediator.h"

#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_consumer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class SuggestionsFromGeminiMediatorTest : public PlatformTest {
 protected:
  SuggestionsFromGeminiMediatorTest() {
    mediator_ = [[SuggestionsFromGeminiMediator alloc]
        initWithPrefService:&pref_service_];
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

// Tests that tapping manage connected apps triggers delegate.
TEST_F(SuggestionsFromGeminiMediatorTest,
       TestSelectManageConnectedAppsTriggersDelegate) {
  OCMExpect([mock_delegate_
      suggestionsFromGeminiMediatorOpenConnectedApps:mediator_]);
  [mediator_ didSelectManageConnectedApps];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

}  // namespace
