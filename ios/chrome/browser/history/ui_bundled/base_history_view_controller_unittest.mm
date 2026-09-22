// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <set>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/history/core/browser/browsing_history_service.h"
#import "components/history/core/browser/features.h"
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

}  // namespace

@interface BaseHistoryViewController (Testing)

// Returns the history entries matching the items at `indexPaths`. Each entry
// carries the visits that deleting the item should remove.
- (std::vector<BrowsingHistoryService::HistoryEntry>)
    entriesForItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths;

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

    CreateController();

    history_controller_ =
        base::apple::ObjCCastStrict<BaseHistoryViewController>(controller());
    history_controller_.browser = browser_.get();
    history_controller_.imageDataSource = favicon_data_source_;
  }

  void TearDown() override {
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
