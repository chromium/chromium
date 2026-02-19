// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/history/ui_bundled/history_entry_item.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/web/public/test/web_task_environment.h"
#import "url/gurl.h"

namespace {

// Test section identifier.
const NSInteger kTestSectionIdentifier = kSectionIdentifierEnumZero + 1;

}  // namespace

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
  void AddHistoryItem(const GURL& url) {
    if (![history_controller_.tableViewModel
            hasSectionForSectionIdentifier:kTestSectionIdentifier]) {
      [history_controller_.tableViewModel
          addSectionWithIdentifier:kTestSectionIdentifier];
    }
    HistoryEntryItem* item =
        [[HistoryEntryItem alloc] initWithType:kItemTypeEnumZero
                         accessibilityDelegate:nil];
    item.URL = url;
    [history_controller_.tableViewModel addItem:item
                        toSectionWithIdentifier:kTestSectionIdentifier];
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
  NSInteger section_index = [history_controller_.tableViewModel
      sectionForSectionIdentifier:kTestSectionIdentifier];
  NSIndexPath* index_path = [NSIndexPath indexPathForRow:0
                                               inSection:section_index];
  [history_controller_ tableView:history_controller_.tableView
           cellForRowAtIndexPath:index_path];

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
