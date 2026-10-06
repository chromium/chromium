// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <set>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/history/core/browser/browsing_history_service.h"
#import "components/history/core/browser/features.h"
#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller+subclassing.h"
#import "ios/chrome/browser/history/ui_bundled/history_entry_item.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/web/public/test/web_task_environment.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#import "url/gurl.h"

namespace {

// Test section identifier.
const NSInteger kTestSectionIdentifier = kSectionIdentifierEnumZero + 1;

// Fake BrowsingHistoryService for testing query callbacks without backend I/O.
class FakeBrowsingHistoryService : public history::BrowsingHistoryService {
 public:
  FakeBrowsingHistoryService() = default;
  ~FakeBrowsingHistoryService() override = default;
  void QueryHistory(const std::u16string& search_text,
                    const history::QueryOptions& options) override {}
};

}  // namespace

@interface BaseHistoryViewController (Testing)

// Returns the history entries matching the items at `indexPaths`. Each entry
// carries the visits that deleting the item should remove.
- (std::vector<BrowsingHistoryService::HistoryEntry>)
    entriesForItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths;

- (void)fetchHistoryForQuery:(NSString*)query continuation:(BOOL)continuation;

@end

// Fake data source to capture the asynchronous favicon completion block.
@interface FakeFaviconDataSource : NSObject <TableViewFaviconDataSource>
@property(nonatomic, copy) void (^completionBlock)(FaviconAttributes*, bool);
@end

@implementation FakeFaviconDataSource
- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*, bool))completion {
  self.completionBlock = completion;
}
@end

class BaseHistoryViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    favicon_data_source_ = [[FakeFaviconDataSource alloc] init];
    browsing_history_service_ = std::make_unique<FakeBrowsingHistoryService>();

    CreateController();

    history_controller_ =
        base::apple::ObjCCastStrict<BaseHistoryViewController>(controller());
    history_controller_.browser = browser_.get();
    history_controller_.imageDataSource = favicon_data_source_;
    history_controller_.historyService = browsing_history_service_.get();
  }

  void TearDown() override {
    history_controller_.historyService = nullptr;
    browsing_history_service_.reset();
    [history_controller_ detachFromBrowser];
    LegacyChromeTableViewControllerTest::TearDown();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[BaseHistoryViewController alloc] init];
  }

  // Helper to add dummy history item.
  void AddHistoryItem(const GURL& url,
                      base::Time timestamp = base::Time(),
                      const absl::flat_hash_map<GURL, std::set<base::Time>>&
                          all_timestamps = {}) {
    if (![history_controller_.tableViewModel
            hasSectionForSectionIdentifier:kTestSectionIdentifier]) {
      [history_controller_.tableViewModel
          addSectionWithIdentifier:kTestSectionIdentifier];
    }
    HistoryEntryItem* item =
        [[HistoryEntryItem alloc] initWithType:kItemTypeEnumZero
                         accessibilityDelegate:nil];
    item.URL = url;
    item.timestamp = timestamp;
    item.allTimestamps = all_timestamps;
    [history_controller_.tableViewModel addItem:item
                        toSectionWithIdentifier:kTestSectionIdentifier];
  }

  // Returns the index path of the item at `index` in the test section.
  NSIndexPath* TestSectionIndexPath(NSInteger index) {
    NSInteger section_index = [history_controller_.tableViewModel
        sectionForSectionIdentifier:kTestSectionIdentifier];
    return [NSIndexPath indexPathForRow:index inSection:section_index];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeFaviconDataSource* favicon_data_source_;
  std::unique_ptr<FakeBrowsingHistoryService> browsing_history_service_;
  BaseHistoryViewController* history_controller_;
};

// Verifies that the initial view model contains the default status section.
TEST_F(BaseHistoryViewControllerTest,
       ShouldHaveStatusSectionUponInitialization) {
  CheckController();

  // `BaseHistoryViewController` always adds a status section on `-loadModel`.
  EXPECT_EQ(1, NumberOfSections());
}

// Ensures no crash occurs if an item is deleted while its favicon is fetching.
TEST_F(BaseHistoryViewControllerTest,
       FaviconCompletionShouldNotCrashWhenItemIsDeleted) {
  CheckController();

  // Add a dummy item to the model.
  AddHistoryItem(GURL("http://example.com"));

  // Request the cell (triggers async fetch) and then delete the item.
  [history_controller_ tableView:history_controller_.tableView
           cellForRowAtIndexPath:TestSectionIndexPath(0)];

  // Verify the data source captured the completion block before deleting the
  // item.
  ASSERT_TRUE(favicon_data_source_.completionBlock != nil);

  [history_controller_.tableViewModel removeItemWithType:kItemTypeEnumZero
                               fromSectionWithIdentifier:kTestSectionIdentifier
                                                 atIndex:0];

  // Fire the completion block. This should return safely without crashing.
  FaviconAttributes* dummy_attributes =
      [FaviconAttributes attributesWithImage:[[UIImage alloc] init]];
  favicon_data_source_.completionBlock(dummy_attributes, false);
}

// Tests that an entry to delete carries every visit grouped into it when
// history de-duplication is enabled, and only its own visit otherwise.
TEST_F(BaseHistoryViewControllerTest,
       ShouldPipeGroupedTimestampsOnlyWhenHistoryDeduplicationIsEnabled) {
  CheckController();

  // URLs of the visits held by a single de-duplicated history entry.
  const char example_url[] = "http://example.com/";
  const char similar_example_url[] = "http://example.com/example";

  // An item produced with de-duplication enabled groups all the visits of the
  // day made to the same or to similar URLs.
  const base::Time visit_time = base::Time::Now();
  const absl::flat_hash_map<GURL, std::set<base::Time>> all_timestamps = {
      {GURL(example_url), {visit_time, visit_time - base::Hours(1)}},
      {GURL(similar_example_url), {visit_time - base::Hours(2)}}};
  AddHistoryItem(GURL(example_url), visit_time, all_timestamps);
  NSArray<NSIndexPath*>* index_paths = @[ TestSectionIndexPath(0) ];

  // Each phase scopes its own feature list so that the feature state is reset
  // before the next one is configured.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        history::kBrowsingHistorySimilarVisitsGrouping);

    std::vector<BrowsingHistoryService::HistoryEntry> entries =
        [history_controller_ entriesForItemsAtIndexPaths:index_paths];

    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(GURL(example_url), entries.front().url);
    EXPECT_EQ(all_timestamps, entries.front().all_timestamps);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        history::kBrowsingHistorySimilarVisitsGrouping);

    std::vector<BrowsingHistoryService::HistoryEntry> entries =
        [history_controller_ entriesForItemsAtIndexPaths:index_paths];

    // The grouped visits are ignored, even though the item carries them.
    ASSERT_EQ(1u, entries.size());
    const absl::flat_hash_map<GURL, std::set<base::Time>> expected_timestamps =
        {{GURL(example_url), {visit_time}}};
    EXPECT_EQ(expected_timestamps, entries.front().all_timestamps);
  }
}

// Tests that IOS.HistoryPage.TimeToFirstVisibleContent is recorded upon
// receiving non-empty history results on initial load, and not on subsequent
// continuation batches.
TEST_F(BaseHistoryViewControllerTest,
       RecordsTimeToFirstVisibleContentOnInitialLoad) {
  CheckController();
  base::HistogramTester histogram_tester;

  [history_controller_ showHistoryMatchingQuery:nil];

  BrowsingHistoryService::HistoryEntry entry;
  entry.url = GURL("http://example.com");
  entry.time = base::Time::Now();
  BrowsingHistoryService::QueryResultsInfo query_results_info;
  [history_controller_ historyQueryWasCompletedWithResults:{entry}
                                          queryResultsInfo:query_results_info
                                       continuationClosure:base::DoNothing()];

  histogram_tester.ExpectTotalCount("IOS.HistoryPage.TimeToFirstVisibleContent",
                                    1);

  // Request next batch via continuation.
  [history_controller_ fetchHistoryForQuery:nil continuation:YES];

  // Subsequent batch should not record the histogram again.
  [history_controller_ historyQueryWasCompletedWithResults:{entry}
                                          queryResultsInfo:query_results_info
                                       continuationClosure:base::DoNothing()];
  histogram_tester.ExpectTotalCount("IOS.HistoryPage.TimeToFirstVisibleContent",
                                    1);
}

// Tests that IOS.HistoryPage.TimeToFirstVisibleContent is not recorded when
// history query returns empty results.
TEST_F(BaseHistoryViewControllerTest,
       DoesNotRecordTimeToFirstVisibleContentOnEmptyResults) {
  CheckController();
  base::HistogramTester histogram_tester;

  [history_controller_ showHistoryMatchingQuery:nil];

  BrowsingHistoryService::QueryResultsInfo query_results_info;
  [history_controller_ historyQueryWasCompletedWithResults:{}
                                          queryResultsInfo:query_results_info
                                       continuationClosure:base::DoNothing()];

  histogram_tester.ExpectTotalCount("IOS.HistoryPage.TimeToFirstVisibleContent",
                                    0);
}

// Tests that IOS.HistoryPage.TimeToFirstVisibleContent is recorded only once
// per controller, even when the user clears a search and history is fetched
// again.
TEST_F(BaseHistoryViewControllerTest,
       ClearingSearchDoesNotRecordTimeToFirstVisibleContentAgain) {
  CheckController();
  base::HistogramTester histogram_tester;

  BrowsingHistoryService::HistoryEntry entry;
  entry.url = GURL("http://example.com");
  entry.time = base::Time::Now();
  BrowsingHistoryService::QueryResultsInfo query_results_info;

  [history_controller_ showHistoryMatchingQuery:nil];
  [history_controller_ historyQueryWasCompletedWithResults:{entry}
                                          queryResultsInfo:query_results_info
                                       continuationClosure:base::DoNothing()];
  histogram_tester.ExpectTotalCount("IOS.HistoryPage.TimeToFirstVisibleContent",
                                    1);

  // Search, then clear the search.
  [history_controller_ showHistoryMatchingQuery:@"foo"];
  [history_controller_ historyQueryWasCompletedWithResults:{entry}
                                          queryResultsInfo:query_results_info
                                       continuationClosure:base::DoNothing()];
  [history_controller_ showHistoryMatchingQuery:@""];
  [history_controller_ historyQueryWasCompletedWithResults:{entry}
                                          queryResultsInfo:query_results_info
                                       continuationClosure:base::DoNothing()];

  histogram_tester.ExpectTotalCount("IOS.HistoryPage.TimeToFirstVisibleContent",
                                    1);
}

// Tests that IOS.HistoryPage.TimeToFirstVisibleContent is not recorded if a
// search is started before the initial history results arrive.
TEST_F(BaseHistoryViewControllerTest,
       DoesNotRecordTimeToFirstVisibleContentIfSearchStartedBeforeResults) {
  CheckController();
  base::HistogramTester histogram_tester;

  [history_controller_ showHistoryMatchingQuery:nil];
  [history_controller_ showHistoryMatchingQuery:@"foo"];

  BrowsingHistoryService::HistoryEntry entry;
  entry.url = GURL("http://example.com");
  entry.time = base::Time::Now();
  BrowsingHistoryService::QueryResultsInfo query_results_info;
  [history_controller_ historyQueryWasCompletedWithResults:{entry}
                                          queryResultsInfo:query_results_info
                                       continuationClosure:base::DoNothing()];

  histogram_tester.ExpectTotalCount("IOS.HistoryPage.TimeToFirstVisibleContent",
                                    0);
}
