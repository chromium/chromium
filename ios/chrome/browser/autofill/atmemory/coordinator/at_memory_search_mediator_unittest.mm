// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_mediator.h"

#import <string>

#import "base/test/gmock_callback_support.h"
#import "components/autofill/core/browser/integrators/at_memory/mock_at_memory_query_service.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_consumer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using ::testing::_;

@interface AtMemorySearchMediator (Testing)
- (void)requestResultsForQuery:(NSString*)query;
@end

class AtMemorySearchMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_.SetVisibleURL(GURL("http://example.org"));
    web_state_.SetTitle(u"Example Title");

    mediator_ = [[AtMemorySearchMediator alloc]
        initWithAtMemoryQueryService:&mock_query_service_
                            webState:&web_state_];

    mock_consumer_ = OCMProtocolMock(@protocol(AtMemorySearchConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  autofill::MockAtMemoryQueryService mock_query_service_;
  web::FakeWebState web_state_;
  AtMemorySearchMediator* mediator_;
  id mock_consumer_;
};

// Parameters for AtMemorySearchMediatorErrorTest.
struct AtMemoryErrorTestParam {
  std::string test_name;
  autofill::MemorySearchStatus status;
  AtMemoryErrorType expected_error_type;
};

// Parameterized test fixture to verify that handleAtMemorySearchResults maps
// search status to the expected consumer error type.
class AtMemorySearchMediatorErrorTest
    : public AtMemorySearchMediatorTest,
      public testing::WithParamInterface<AtMemoryErrorTestParam> {};

// Tests that handleAtMemorySearchResults maps search status to the expected
// consumer error type.
TEST_P(AtMemorySearchMediatorErrorTest, HandlesErrorStatus) {
  mediator_.consumer = mock_consumer_;
  autofill::MemorySearchResults fake_results(GetParam().status);

  EXPECT_CALL(mock_query_service_, Query)
      .WillOnce(base::test::RunCallback<3>(fake_results));

  OCMExpect([mock_consumer_ setErrorType:GetParam().expected_error_type]);

  [mediator_ requestResultsForQuery:@"test query"];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Instantiates the test suite with various combinations of search statuses to
// ensure the consumer error types are correct.
INSTANTIATE_TEST_SUITE_P(
    AllVariants,
    AtMemorySearchMediatorErrorTest,
    ::testing::ValuesIn<AtMemoryErrorTestParam>({
        {.test_name = "NoConnectionFailure",
         .status = autofill::MemorySearchStatus::kNoConnectionFailure,
         .expected_error_type = AtMemoryErrorType::kNoConnectionError},

        {.test_name = "UnsupportedQuery",
         .status = autofill::MemorySearchStatus::kUnsupportedQuery,
         .expected_error_type = AtMemoryErrorType::kUnsupportedQueryError},

        {.test_name = "InternalFailure",
         .status = autofill::MemorySearchStatus::kInternalFailure,
         .expected_error_type = AtMemoryErrorType::kNoDataError},

        {.test_name = "InferenceFailure",
         .status = autofill::MemorySearchStatus::kInferenceFailure,
         .expected_error_type = AtMemoryErrorType::kNoDataError},

        {.test_name = "FinalResponseSuccess",
         .status = autofill::MemorySearchStatus::kFinalResponseSuccess,
         .expected_error_type = AtMemoryErrorType::kNoDataError},

        {.test_name = "PartialResponseSuccess",
         .status = autofill::MemorySearchStatus::kPartialResponseSuccess,
         .expected_error_type = AtMemoryErrorType::kNoDataError},
    }),
    [](const ::testing::TestParamInfo<AtMemoryErrorTestParam>& info) {
      return info.param.test_name;
    });
