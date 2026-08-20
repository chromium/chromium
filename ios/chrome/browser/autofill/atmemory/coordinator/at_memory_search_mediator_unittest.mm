// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_mediator.h"

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/integrators/at_memory/mock_at_memory_query_service.h"
#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/personal_context/first_run/personal_context_first_run_service.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_fill_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_search_result_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"
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

// Constants for mock search items.
NSString* const kPassportTypeName = @"Passport";
NSString* const kPassportValue = @"AA123456";

// Search query used for testing mediator search requests.
NSString* const kSearchQuery = @"test mediator query";

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

// Fake implementation of AtMemorySearchResultCommands to intercept granular
// fill requests and verify it was called.
@interface FakeAtMemorySearchResultHandler
    : NSObject <AtMemorySearchResultCommands>
@property(nonatomic, assign) BOOL wasCalled;
@end

@implementation FakeAtMemorySearchResultHandler
- (void)showAtMemoryGranularFillWithResult:
    (const autofill::MemorySearchResult&)result {
  self.wasCalled = YES;
}
@end

class AtMemorySearchMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_.SetVisibleURL(GURL("http://example.org"));
    web_state_.SetTitle(u"Example Title");
    mock_consumer_ = OCMProtocolMock(@protocol(AtMemorySearchConsumer));
  }

  void TearDown() override {
    if (mediator_) {
      [mediator_ disconnect];
      mediator_ = nil;
    }
    PlatformTest::TearDown();
  }

  // Creates an AtMemorySearchMediator.
  void CreateMediator() {
    mediator_ = [[AtMemorySearchMediator alloc]
        initWithAtMemoryQueryService:&mock_query_service_
                            webState:&web_state_
                     firstRunService:&first_run_service_];
    mediator_.consumer = mock_consumer_;
  }

  web::WebTaskEnvironment task_environment_;
  autofill::MockAtMemoryQueryService mock_query_service_;
  web::FakeWebState web_state_;
  FakePersonalContextFirstRunService first_run_service_;
  id mock_consumer_;
  AtMemorySearchMediator* mediator_;
};

// Tests that successful search results are converted to items and pushed to the
// consumer.
TEST_F(AtMemorySearchMediatorTest, PushesSearchResultsToConsumer) {
  CreateMediator();

  autofill::MemorySearchResults fake_results(
      autofill::MemorySearchStatus::kFinalResponseSuccess);
  fake_results.entries.push_back(
      autofill::MemorySearchResult(autofill::MemoryDataType::kPassportNumber,
                                   base::SysNSStringToUTF16(kPassportTypeName),
                                   base::SysNSStringToUTF16(kPassportValue)));

  EXPECT_CALL(mock_query_service_, Query)
      .WillOnce(base::test::RunCallback<3>(fake_results));

  OCMExpect([mock_consumer_
      setSearchResults:[OCMArg checkWithBlock:^BOOL(
                                   NSArray<AtMemorySearchItem*>* items) {
        if (items.count != 1) {
          return NO;
        }
        AtMemorySearchItem* item = items.firstObject;
        return [item.title isEqualToString:kPassportValue] &&
               [item.subtitle isEqualToString:kPassportTypeName];
      }]]);

  [mediator_ startSearchWithQuery:kSearchQuery];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that selecting a search result item calls fillHandler with the item's
// title and dismisses the UI.
TEST_F(AtMemorySearchMediatorTest,
       SelectSearchResultItemFillsFormAndDismisses) {
  CreateMediator();

  id mock_fill_handler = OCMProtocolMock(@protocol(AtMemoryFillCommands));
  id mock_at_memory_handler = OCMProtocolMock(@protocol(AtMemoryCommands));
  mediator_.fillHandler = mock_fill_handler;
  mediator_.atMemoryHandler = mock_at_memory_handler;

  autofill::MemorySearchResult mock_result(
      autofill::MemoryDataType::kPassportNumber,
      base::SysNSStringToUTF16(kPassportTypeName),
      base::SysNSStringToUTF16(kPassportValue));

  AtMemorySearchItem* item =
      [[AtMemorySearchItem alloc] initWithMemorySearchResult:mock_result
                                                       index:0];

  OCMExpect([mock_fill_handler fillWithContent:kPassportValue]);
  OCMExpect([mock_at_memory_handler dismissAtMemory]);

  [mediator_ didSelectSearchResultItem:item];

  EXPECT_OCMOCK_VERIFY(mock_fill_handler);
  EXPECT_OCMOCK_VERIFY(mock_at_memory_handler);
}

// Tests that opening granular fill for a search result invokes the handler.
TEST_F(AtMemorySearchMediatorTest, OpensGranularFillForSearchResult) {
  CreateMediator();

  FakeAtMemorySearchResultHandler* fake_handler =
      [[FakeAtMemorySearchResultHandler alloc] init];
  mediator_.searchResultHandler = fake_handler;

  autofill::MemorySearchResults fake_results(
      autofill::MemorySearchStatus::kFinalResponseSuccess);
  fake_results.entries.push_back(
      autofill::MemorySearchResult(autofill::MemoryDataType::kPassportNumber,
                                   base::SysNSStringToUTF16(kPassportTypeName),
                                   base::SysNSStringToUTF16(kPassportValue)));

  EXPECT_CALL(mock_query_service_, Query)
      .WillOnce(base::test::RunCallback<3>(fake_results));

  [mediator_ startSearchWithQuery:kSearchQuery];

  [mediator_ openGranularFillForSearchResultAtIndex:0];

  EXPECT_TRUE(fake_handler.wasCalled);
}

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
  CreateMediator();

  autofill::MemorySearchResults fake_results(GetParam().status);

  EXPECT_CALL(mock_query_service_, Query)
      .WillOnce(base::test::RunCallback<3>(fake_results));

  OCMExpect([mock_consumer_ setErrorType:GetParam().expected_error_type]);

  [mediator_ startSearchWithQuery:kSearchQuery];

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

#pragma mark - Notice Tests

// Tests that the notice is shown when the first-run service requires it.
TEST_F(AtMemorySearchMediatorTest, ShowsNoticeInitiallyIfEligible) {
  first_run_service_.set_should_show_at_memory_notice(true);

  OCMExpect([mock_consumer_ setNoticeVisible:YES]);

  CreateMediator();

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that the notice is hidden when the first-run service does not require
// it.
TEST_F(AtMemorySearchMediatorTest, HidesNoticeInitiallyIfNotEligible) {
  first_run_service_.set_should_show_at_memory_notice(false);

  OCMExpect([mock_consumer_ setNoticeVisible:NO]);

  CreateMediator();

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that acknowledging the notice updates the first-run service and UI.
TEST_F(AtMemorySearchMediatorTest, AcknowledgeNoticeAcksServiceAndUpdatesUI) {
  first_run_service_.set_should_show_at_memory_notice(true);
  CreateMediator();

  OCMExpect([mock_consumer_ setNoticeVisible:NO]);

  [mediator_ acknowledgePrivacyNotice];

  EXPECT_TRUE(first_run_service_.acknowledged());
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that setting the consumer logs the "Shown" metric if eligible.
TEST_F(AtMemorySearchMediatorTest, LogsShownMetric) {
  first_run_service_.set_should_show_at_memory_notice(true);

  base::HistogramTester histogram_tester;

  CreateMediator();

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.AtMemory.NoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kShown, 1);
}

// Tests that acknowledging the notice logs the "Acknowledged" metric.
TEST_F(AtMemorySearchMediatorTest, LogsAcknowledgedMetric) {
  first_run_service_.set_should_show_at_memory_notice(true);
  CreateMediator();

  base::HistogramTester histogram_tester;

  [mediator_ acknowledgePrivacyNotice];

  histogram_tester.ExpectBucketCount(
      "PersonalContext.AtMemory.NoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kAcknowledged, 1);
}

// Tests that clicking the settings link logs the "LinkButtonClicked" metric and
// calls the handler.
TEST_F(AtMemorySearchMediatorTest,
       LogsSettingsLinkClickedMetricAndCallsHandler) {
  first_run_service_.set_should_show_at_memory_notice(true);
  id mock_handler = OCMProtocolMock(@protocol(AtMemoryCommands));

  CreateMediator();
  mediator_.atMemoryHandler = mock_handler;

  OCMExpect([mock_handler openAutofillSettings]);

  base::HistogramTester histogram_tester;

  [mediator_ didTapSettingsLink];

  histogram_tester.ExpectBucketCount(
      "PersonalContext.AtMemory.NoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kLinkButtonClicked,
      1);

  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that disconnecting without interacting logs the "Dismissed" metric.
TEST_F(AtMemorySearchMediatorTest, LogsDismissedMetricOnDisconnect) {
  first_run_service_.set_should_show_at_memory_notice(true);
  CreateMediator();

  base::HistogramTester histogram_tester;

  [mediator_ disconnect];
  mediator_ = nil;

  histogram_tester.ExpectBucketCount(
      "PersonalContext.AtMemory.NoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kDismissed, 1);
}
