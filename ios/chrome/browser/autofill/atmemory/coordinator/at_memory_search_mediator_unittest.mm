// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_mediator.h"

#import <string>

#import "base/test/gmock_callback_support.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/integrators/at_memory/mock_at_memory_query_service.h"
#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/personal_context/first_run/personal_context_first_run_service.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_consumer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using ::testing::_;

namespace {

// Fake implementation of PersonalContextFirstRunService to control the notice
// state.
class FakePersonalContextFirstRunService
    : public personal_context::PersonalContextFirstRunService {
 public:
  FakePersonalContextFirstRunService() = default;
  ~FakePersonalContextFirstRunService() override = default;

  void MarkPersonalContextAmbientAutofillNoticeAsAcknowledged() override {}
  bool ShouldShowPersonalContextAmbientAutofillNotice() const override {
    return false;
  }
  void RecordAmbientAutofillNoticeImpression(uint32_t session_id) override {}

  void MarkPersonalContextInAtMemoryNoticeAsAcknowledged() override {
    acknowledged_ = true;
    should_show_at_memory_notice_ = false;
  }
  bool ShouldShowPersonalContextAtMemoryNotice() const override {
    return should_show_at_memory_notice_;
  }
  void RecordAtMemoryNoticeImpression(uint32_t session_id) override {}

  void set_should_show_at_memory_notice(bool should_show) {
    should_show_at_memory_notice_ = should_show;
  }
  bool acknowledged() const { return acknowledged_; }

 private:
  // Whether the AtMemory notice should be shown.
  bool should_show_at_memory_notice_ = false;
  // Whether the AtMemory notice has been acknowledged.
  bool acknowledged_ = false;
};

}  // namespace

class AtMemorySearchMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_.SetVisibleURL(GURL("http://example.org"));
    web_state_.SetTitle(u"Example Title");
  }

  web::WebTaskEnvironment task_environment_;
  autofill::MockAtMemoryQueryService mock_query_service_;
  web::FakeWebState web_state_;
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
  FakePersonalContextFirstRunService first_run_service;
  AtMemorySearchMediator* mediator = [[AtMemorySearchMediator alloc]
      initWithAtMemoryQueryService:&mock_query_service_
                          webState:&web_state_
                   firstRunService:&first_run_service];
  id mock_consumer = OCMProtocolMock(@protocol(AtMemorySearchConsumer));
  mediator.consumer = mock_consumer;

  autofill::MemorySearchResults fake_results(GetParam().status);

  EXPECT_CALL(mock_query_service_, Query)
      .WillOnce(base::test::RunCallback<3>(fake_results));

  OCMExpect([mock_consumer setErrorType:GetParam().expected_error_type]);

  [mediator startSearchWithQuery:@"test query"];

  EXPECT_OCMOCK_VERIFY(mock_consumer);
  [mediator disconnect];
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

#pragma mark - Notice Tests

// Tests that the notice is shown when the first-run service requires it.
TEST_F(AtMemorySearchMediatorTest, ShowsNoticeInitiallyIfEligible) {
  FakePersonalContextFirstRunService first_run_service;
  first_run_service.set_should_show_at_memory_notice(true);
  id mock_consumer = OCMProtocolMock(@protocol(AtMemorySearchConsumer));

  OCMExpect([mock_consumer setNoticeVisible:YES]);
  OCMExpect([mock_consumer
      updateTableViewBackgroundStyle:AtMemoryBackgroundStyle::kDefaultStyle]);

  AtMemorySearchMediator* mediator = [[AtMemorySearchMediator alloc]
      initWithAtMemoryQueryService:&mock_query_service_
                          webState:&web_state_
                   firstRunService:&first_run_service];
  mediator.consumer = mock_consumer;

  EXPECT_OCMOCK_VERIFY(mock_consumer);
  [mediator disconnect];
}

// Tests that the notice is hidden when the first-run service does not require
// it.
TEST_F(AtMemorySearchMediatorTest, HidesNoticeInitiallyIfNotEligible) {
  FakePersonalContextFirstRunService first_run_service;
  first_run_service.set_should_show_at_memory_notice(false);
  id mock_consumer = OCMProtocolMock(@protocol(AtMemorySearchConsumer));

  OCMExpect([mock_consumer setNoticeVisible:NO]);
  OCMExpect([mock_consumer
      updateTableViewBackgroundStyle:AtMemoryBackgroundStyle::kEmptyStyle]);

  AtMemorySearchMediator* mediator = [[AtMemorySearchMediator alloc]
      initWithAtMemoryQueryService:&mock_query_service_
                          webState:&web_state_
                   firstRunService:&first_run_service];
  mediator.consumer = mock_consumer;

  EXPECT_OCMOCK_VERIFY(mock_consumer);
  [mediator disconnect];
}

// Tests that acknowledging the notice updates the first-run service and UI.
TEST_F(AtMemorySearchMediatorTest, AcknowledgeNoticeAcksServiceAndUpdatesUI) {
  FakePersonalContextFirstRunService first_run_service;
  first_run_service.set_should_show_at_memory_notice(true);
  id mock_consumer = OCMProtocolMock(@protocol(AtMemorySearchConsumer));

  AtMemorySearchMediator* mediator = [[AtMemorySearchMediator alloc]
      initWithAtMemoryQueryService:&mock_query_service_
                          webState:&web_state_
                   firstRunService:&first_run_service];
  mediator.consumer = mock_consumer;

  OCMExpect([mock_consumer setNoticeVisible:NO]);
  OCMExpect([mock_consumer
      updateTableViewBackgroundStyle:AtMemoryBackgroundStyle::kEmptyStyle]);

  [mediator acknowledgePrivacyNotice];

  EXPECT_TRUE(first_run_service.acknowledged());
  EXPECT_OCMOCK_VERIFY(mock_consumer);
  [mediator disconnect];
}

// Tests that setting the consumer logs the "Shown" metric if eligible.
TEST_F(AtMemorySearchMediatorTest, LogsShownMetric) {
  FakePersonalContextFirstRunService first_run_service;
  first_run_service.set_should_show_at_memory_notice(true);
  id mock_consumer = OCMProtocolMock(@protocol(AtMemorySearchConsumer));

  AtMemorySearchMediator* mediator = [[AtMemorySearchMediator alloc]
      initWithAtMemoryQueryService:&mock_query_service_
                          webState:&web_state_
                   firstRunService:&first_run_service];

  base::HistogramTester histogram_tester;

  mediator.consumer = mock_consumer;

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.AtMemory.NoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kShown, 1);

  [mediator disconnect];
}

// Tests that acknowledging the notice logs the "Acknowledged" metric.
TEST_F(AtMemorySearchMediatorTest, LogsAcknowledgedMetric) {
  FakePersonalContextFirstRunService first_run_service;
  first_run_service.set_should_show_at_memory_notice(true);
  id mock_consumer = OCMProtocolMock(@protocol(AtMemorySearchConsumer));

  AtMemorySearchMediator* mediator = [[AtMemorySearchMediator alloc]
      initWithAtMemoryQueryService:&mock_query_service_
                          webState:&web_state_
                   firstRunService:&first_run_service];
  mediator.consumer = mock_consumer;

  base::HistogramTester histogram_tester;

  [mediator acknowledgePrivacyNotice];

  histogram_tester.ExpectBucketCount(
      "PersonalContext.AtMemory.NoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kAcknowledged, 1);

  [mediator disconnect];
}

// Tests that clicking the settings link logs the "LinkButtonClicked" metric and
// calls the handler.
TEST_F(AtMemorySearchMediatorTest,
       LogsSettingsLinkClickedMetricAndCallsHandler) {
  FakePersonalContextFirstRunService first_run_service;
  first_run_service.set_should_show_at_memory_notice(true);
  id mock_consumer = OCMProtocolMock(@protocol(AtMemorySearchConsumer));
  id mock_handler = OCMProtocolMock(@protocol(AtMemoryCommands));

  AtMemorySearchMediator* mediator = [[AtMemorySearchMediator alloc]
      initWithAtMemoryQueryService:&mock_query_service_
                          webState:&web_state_
                   firstRunService:&first_run_service];
  mediator.consumer = mock_consumer;
  mediator.atMemoryHandler = mock_handler;

  OCMExpect([mock_handler openAutofillSettings]);

  base::HistogramTester histogram_tester;

  [mediator didTapSettingsLink];

  histogram_tester.ExpectBucketCount(
      "PersonalContext.AtMemory.NoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kLinkButtonClicked,
      1);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  [mediator disconnect];
}

// Tests that disconnecting without interacting logs the "Dismissed" metric.
TEST_F(AtMemorySearchMediatorTest, LogsDismissedMetricOnDisconnect) {
  FakePersonalContextFirstRunService first_run_service;
  first_run_service.set_should_show_at_memory_notice(true);
  id mock_consumer = OCMProtocolMock(@protocol(AtMemorySearchConsumer));

  AtMemorySearchMediator* mediator = [[AtMemorySearchMediator alloc]
      initWithAtMemoryQueryService:&mock_query_service_
                          webState:&web_state_
                   firstRunService:&first_run_service];
  mediator.consumer = mock_consumer;

  base::HistogramTester histogram_tester;

  [mediator disconnect];

  histogram_tester.ExpectBucketCount(
      "PersonalContext.AtMemory.NoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kDismissed, 1);
}
