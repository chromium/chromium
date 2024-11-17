// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/prefs/testing_pref_service.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

class ReadingListBrowserAgentUnitTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());

    std::vector<scoped_refptr<ReadingListEntry>> initial_entries;
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::move(initial_entries)));

    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    mock_snackbar_commands_handler_ =
        [OCMockObject niceMockForProtocol:@protocol(SnackbarCommands)];

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_commands_handler_
                     forProtocol:@protocol(SnackbarCommands)];

    ReadingListBrowserAgent::CreateForBrowser(browser_.get());
    reading_list_browser_agent_ =
        ReadingListBrowserAgent::FromBrowser(browser_.get());
  }

  void TearDown() override {
    profile_.reset();
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
  }

  ReadingListModel* reading_list_model() {
    return ReadingListModelFactory::GetForProfile(profile_.get());
  }

 protected:
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  base::HistogramTester histogram_tester_;
  web::WebTaskEnvironment task_environment_;
  raw_ptr<ReadingListBrowserAgent> reading_list_browser_agent_;
  id mock_snackbar_commands_handler_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
};

#pragma mark - Tests.

// Tests BulkAddURLsToReadingListWithViewSnackbar with no valid URL passed.
TEST_F(ReadingListBrowserAgentUnitTest,
       TestBulkAddURLsToReadingListNoValidURL) {
  NSArray* urls = @[ [[NSURL alloc] initWithString:@""] ];

  reading_list_browser_agent_->BulkAddURLsToReadingListWithViewSnackbar(urls);

  EXPECT_EQ(reading_list_model()->size(), (size_t)0);
  histogram_tester_.ExpectBucketCount("IOS.ReadingList.BulkAddURLsCount", 0, 1);
}

// Tests BulkAddURLsToReadingListWithViewSnackbar with multiple invalid URLs
// passed.
TEST_F(ReadingListBrowserAgentUnitTest,
       TestBulkAddURLsToReadingListMultipleInvalidURLs) {
  NSArray* urls = @[
    [[NSURL alloc] initWithString:@""], [[NSURL alloc] initWithString:@"://"],
    [[NSURL alloc] initWithString:@"::invalid::"]
  ];

  reading_list_browser_agent_->BulkAddURLsToReadingListWithViewSnackbar(urls);

  EXPECT_EQ(reading_list_model()->size(), (size_t)0);
  histogram_tester_.ExpectBucketCount("IOS.ReadingList.BulkAddURLsCount", 0, 1);
}

// Tests BulkAddURLsToReadingListWithViewSnackbar with one valid URL passed.
TEST_F(ReadingListBrowserAgentUnitTest,
       TestBulkAddURLsToReadingListOneValidURL) {
  NSArray* urls = @[ [[NSURL alloc] initWithString:@"https://google.ca"] ];

  reading_list_browser_agent_->BulkAddURLsToReadingListWithViewSnackbar(urls);

  EXPECT_EQ(reading_list_model()->size(), (size_t)1);
  histogram_tester_.ExpectBucketCount("IOS.ReadingList.BulkAddURLsCount", 1, 1);
}

// Tests BulkAddURLsToReadingListWithViewSnackbar with two valid URLs passed.
TEST_F(ReadingListBrowserAgentUnitTest,
       TestBulkAddURLsToReadingListTwoValidURLs) {
  NSArray* urls = @[
    [[NSURL alloc] initWithString:@"https://google.com?q=test"],
    [[NSURL alloc] initWithString:@"https://google.fr"]
  ];

  reading_list_browser_agent_->BulkAddURLsToReadingListWithViewSnackbar(urls);

  EXPECT_EQ(reading_list_model()->size(), (size_t)2);
  histogram_tester_.ExpectBucketCount("IOS.ReadingList.BulkAddURLsCount", 2, 1);
}

// Tests BulkAddURLsToReadingListWithViewSnackbar with a set of mixed valid and
// invalid URLs.
TEST_F(ReadingListBrowserAgentUnitTest, TestBulkAddURLsToReadingListMixedSet) {
  NSArray* urls = @[
    [[NSURL alloc] initWithString:@"https://google.com/"],
    [[NSURL alloc] initWithString:@"::invalid::"],
    [[NSURL alloc] initWithString:@"https://google.fr/path"],
    [[NSURL alloc]
        initWithString:@"https://google.co.jp/path/to/document.pdf"]
  ];

  reading_list_browser_agent_->BulkAddURLsToReadingListWithViewSnackbar(urls);

  EXPECT_EQ(reading_list_model()->size(), (size_t)3);
  histogram_tester_.ExpectBucketCount("IOS.ReadingList.BulkAddURLsCount", 3, 1);
}

}  // namespace
