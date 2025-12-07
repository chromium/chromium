// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/time/time.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_action_delegate.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_group_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_mutator.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"
#import "ios/chrome/browser/shared/public/commands/download_record_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::Return;

// Mock for DownloadListMutator protocol.
@interface MockDownloadListMutator : NSObject <DownloadListMutator>
@property(nonatomic, assign) BOOL loadDownloadRecordsCalled;
@property(nonatomic, assign) BOOL syncRecordsIfNeededCalled;
@property(nonatomic, strong) DownloadListItem* lastDeletedItem;
@property(nonatomic, strong) DownloadListItem* lastCancelledItem;
@end

@implementation MockDownloadListMutator

- (void)loadDownloadRecords {
  self.loadDownloadRecordsCalled = YES;
}

- (void)syncRecordsIfNeeded {
  self.syncRecordsIfNeededCalled = YES;
}

- (void)filterRecordsWithType:(DownloadFilterType)type {
  // Not implemented for basic tests.
}

- (void)filterRecordsWithKeyword:(NSString*)keyword {
  // Not implemented for basic tests.
}

- (void)deleteDownloadItem:(DownloadListItem*)item {
  self.lastDeletedItem = item;
}

- (void)cancelDownloadItem:(DownloadListItem*)item {
  self.lastCancelledItem = item;
}

@end

// Mock for DownloadListCommands protocol.
@interface MockDownloadListCommands : NSObject <DownloadListCommands>
@property(nonatomic, assign) BOOL hideDownloadListCalled;
@property(nonatomic, assign) BOOL showDownloadListCalled;
@end

@implementation MockDownloadListCommands

- (void)hideDownloadList {
  self.hideDownloadListCalled = YES;
}

- (void)showDownloadList {
  self.showDownloadListCalled = YES;
}

@end

// Mock for DownloadRecordCommands protocol.
@interface MockDownloadRecordCommands : NSObject <DownloadRecordCommands>
@property(nonatomic, assign) BOOL openFileCalled;
@property(nonatomic, assign) BOOL shareDownloadedFileCalled;
@end

@implementation MockDownloadRecordCommands

- (void)openFileWithPath:(const base::FilePath&)filePath
                mimeType:(const std::string&)mimeType {
  self.openFileCalled = YES;
}

- (void)shareDownloadedFile:(const DownloadRecord&)record
                 sourceView:(UIView*)sourceView {
  self.shareDownloadedFileCalled = YES;
}

@end

// Mock for DownloadListActionDelegate protocol.
@interface MockDownloadListActionDelegate
    : NSObject <DownloadListActionDelegate>
@property(nonatomic, strong) DownloadListItem* lastOpenedItem;
@end

@implementation MockDownloadListActionDelegate

- (void)openDownloadInFiles:(DownloadListItem*)item {
  self.lastOpenedItem = item;
}

@end

// Test category to access private methods and properties.
@interface DownloadListTableViewController (Testing)
- (void)updateUI;
- (void)performPeriodicUpdate;
@property(nonatomic, readonly) DownloadListTableViewHeader* filterHeaderView;
@end

namespace {

/// Creates a test download record with the given parameters.
DownloadRecord CreateTestDownloadRecord(
    const std::string& download_id,
    const std::string& file_name,
    web::DownloadTask::State state = web::DownloadTask::State::kComplete,
    int64_t total_bytes = 1024,
    int64_t received_bytes = 1024) {
  DownloadRecord record;
  record.download_id = download_id;
  record.file_name = file_name;
  record.state = state;
  record.total_bytes = total_bytes;
  record.received_bytes = received_bytes;
  record.created_time = base::Time::Now();
  record.file_path = base::FilePath("/tmp/" + file_name);
  return record;
}

/// Creates a test DownloadListItem from a DownloadRecord.
DownloadListItem* CreateTestDownloadListItem(const DownloadRecord& record) {
  return [[DownloadListItem alloc] initWithDownloadRecord:record];
}

}  // namespace

/// Test fixture for DownloadListTableViewController.
class DownloadListTableViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    controller_ = [[DownloadListTableViewController alloc]
        initWithStyle:UITableViewStyleInsetGrouped];

    // Set up mocks.
    mock_mutator_ = [[MockDownloadListMutator alloc] init];
    mock_download_list_handler_ = [[MockDownloadListCommands alloc] init];
    mock_download_record_handler_ = [[MockDownloadRecordCommands alloc] init];
    mock_action_delegate_ = [[MockDownloadListActionDelegate alloc] init];

    // Configure controller with mocks.
    controller_.mutator = mock_mutator_;
    controller_.downloadListHandler = mock_download_list_handler_;
    controller_.downloadRecordHandler = mock_download_record_handler_;
    controller_.actionDelegate = mock_action_delegate_;

    // Load the view to trigger viewDidLoad.
    [controller_ loadViewIfNeeded];
  }

  void TearDown() override {
    controller_ = nil;
    mock_mutator_ = nil;
    mock_download_list_handler_ = nil;
    mock_download_record_handler_ = nil;
    mock_action_delegate_ = nil;
    PlatformTest::TearDown();
  }

  /// Creates test download items for testing.
  NSArray<DownloadListItem*>* CreateTestDownloadItems() {
    DownloadRecord record1 = CreateTestDownloadRecord("id1", "file1.pdf");
    DownloadRecord record2 = CreateTestDownloadRecord("id2", "file2.jpg");
    DownloadRecord record3 = CreateTestDownloadRecord(
        "id3", "file3.zip", web::DownloadTask::State::kInProgress, 2048, 1024);

    return @[
      CreateTestDownloadListItem(record1), CreateTestDownloadListItem(record2),
      CreateTestDownloadListItem(record3)
    ];
  }

  /// Helper method to test context menu configuration with specified actions.
  /// @param actions The actions to mock for the download item.
  /// @return The context menu configuration returned by the controller.
  UIContextMenuConfiguration* TestContextMenuWithActions(
      DownloadListItemAction actions) {
    // Create test item.
    DownloadRecord record = CreateTestDownloadRecord("id1", "file1.pdf");
    DownloadListItem* item = CreateTestDownloadListItem(record);

    // Mock the item to return specified actions.
    id mockItem = OCMPartialMock(item);
    OCMStub([mockItem availableActions]).andReturn(actions);

    [controller_ setDownloadListItems:@[ mockItem ]];

    // Trigger periodic update manually to process cached items.
    [controller_ performPeriodicUpdate];

    NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];

    return [controller_ tableView:controller_.tableView
        contextMenuConfigurationForRowAtIndexPath:indexPath
                                            point:CGPointZero];
  }

  web::WebTaskEnvironment task_environment_;
  DownloadListTableViewController* controller_;
  MockDownloadListMutator* mock_mutator_;
  MockDownloadListCommands* mock_download_list_handler_;
  MockDownloadRecordCommands* mock_download_record_handler_;
  MockDownloadListActionDelegate* mock_action_delegate_;
};

#pragma mark - Initialization and Setup Tests

/// Tests that the view controller initializes correctly.
TEST_F(DownloadListTableViewControllerTest, TestInitialization) {
  EXPECT_TRUE(controller_);
  EXPECT_TRUE(
      [controller_ isKindOfClass:[DownloadListTableViewController class]]);
}

/// Tests viewDidLoad configures the UI correctly.
TEST_F(DownloadListTableViewControllerTest, TestViewDidLoad) {
  // Verify title is set correctly.
  NSString* expectedTitle = l10n_util::GetNSString(IDS_IOS_DOWNLOAD_LIST_TITLE);
  EXPECT_TRUE([controller_.title isEqualToString:expectedTitle]);

  // Verify data source is configured.
  EXPECT_TRUE(controller_.tableView.dataSource);

  // Verify mutator was called to load records.
  EXPECT_TRUE(mock_mutator_.loadDownloadRecordsCalled);
}

#pragma mark - DownloadListConsumer Protocol Tests

/// Tests setDownloadListItems with empty array.
TEST_F(DownloadListTableViewControllerTest, TestSetDownloadListItemsEmpty) {
  NSArray<DownloadListItem*>* emptyItems = @[];

  [controller_ setDownloadListItems:emptyItems];

  // Trigger periodic update manually to process cached items.
  [controller_ performPeriodicUpdate];

  // Verify the table view has no sections when empty.
  EXPECT_EQ(0, controller_.tableView.numberOfSections);
}

/// Tests setDownloadListItems with multiple items.
TEST_F(DownloadListTableViewControllerTest, TestSetDownloadListItemsMultiple) {
  NSArray<DownloadListItem*>* items = CreateTestDownloadItems();

  [controller_ setDownloadListItems:items];

  // Trigger periodic update manually to process cached items.
  [controller_ performPeriodicUpdate];

  // Verify table view has sections (grouped by date).
  EXPECT_GT(controller_.tableView.numberOfSections, 0);

  // Verify total number of items across all sections matches input.
  NSInteger totalItems = 0;
  for (NSInteger section = 0; section < controller_.tableView.numberOfSections;
       section++) {
    totalItems += [controller_.tableView numberOfRowsInSection:section];
  }
  EXPECT_EQ(static_cast<NSInteger>(items.count), totalItems);
}

/// Tests setEmptyState behavior for both enabled and disabled states.
TEST_F(DownloadListTableViewControllerTest, TestSetEmptyState) {
  // Test enabling empty state.
  [controller_ setEmptyState:YES];

  // Verify navigation item is configured for empty state.
  EXPECT_EQ(UINavigationItemLargeTitleDisplayModeNever,
            controller_.navigationItem.largeTitleDisplayMode);

  // Verify background view is set.
  EXPECT_TRUE(controller_.tableView.backgroundView);

  // Test disabling empty state.
  [controller_ setEmptyState:NO];

  // Verify navigation item is configured for normal state.
  EXPECT_EQ(UINavigationItemLargeTitleDisplayModeAlways,
            controller_.navigationItem.largeTitleDisplayMode);

  // Verify background view is cleared.
  EXPECT_FALSE(controller_.tableView.backgroundView);
}

/// Tests setLoadingState method exists and doesn't crash.
TEST_F(DownloadListTableViewControllerTest, TestSetLoadingState) {
  // This method currently has no implementation, but should not crash.
  [controller_ setLoadingState:YES];
  [controller_ setLoadingState:NO];
}

#pragma mark - UITableViewDelegate Tests

/// Tests contextMenuConfigurationForRowAtIndexPath behavior for different
/// action combinations.
TEST_F(DownloadListTableViewControllerTest, TestContextMenuConfiguration) {
  // Test with no actions - should return nil.
  UIContextMenuConfiguration* configNoActions =
      TestContextMenuWithActions(DownloadListItemActionNone);
  EXPECT_FALSE(configNoActions);

  // Test with available actions - should return valid configuration.
  UIContextMenuConfiguration* configWithActions = TestContextMenuWithActions(
      DownloadListItemActionOpenInFiles | DownloadListItemActionDelete);
  EXPECT_TRUE(configWithActions);
}

/// Tests viewForHeaderInSection returns correct header view.
TEST_F(DownloadListTableViewControllerTest, TestViewForHeaderInSection) {
  NSArray<DownloadListItem*>* items = CreateTestDownloadItems();
  [controller_ setDownloadListItems:items];

  // Trigger periodic update manually to process cached items.
  [controller_ performPeriodicUpdate];

  UIView* headerView = [controller_ tableView:controller_.tableView
                       viewForHeaderInSection:0];

  EXPECT_TRUE(headerView);
}

#pragma mark - Action Tests

/// Tests that cancel button is displayed for in-progress downloads.
TEST_F(DownloadListTableViewControllerTest,
       TestCancelButtonDisplayForInProgressDownload) {
  // Create an in-progress download that should show cancel button.
  DownloadRecord inProgressRecord = CreateTestDownloadRecord(
      "id1", "file1.pdf", web::DownloadTask::State::kInProgress);
  DownloadListItem* inProgressItem =
      CreateTestDownloadListItem(inProgressRecord);

  [controller_ setDownloadListItems:@[ inProgressItem ]];

  // Trigger periodic update manually to process cached items.
  [controller_ performPeriodicUpdate];

  // Verify the in-progress item shows cancel button.
  EXPECT_TRUE([inProgressItem cancelable]);

  // Verify the corresponding table view cell has accessoryView.
  EXPECT_GT(controller_.tableView.numberOfSections, 0);
  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  UITableViewCell* cell =
      [controller_.tableView.dataSource tableView:controller_.tableView
                            cellForRowAtIndexPath:indexPath];

  // In-progress download should have accessoryView.
  EXPECT_TRUE(cell.accessoryView != nil);
}

/// Tests that cancel button is not displayed for completed downloads.
TEST_F(DownloadListTableViewControllerTest,
       TestCancelButtonDisplayForCompletedDownload) {
  // Create a completed download that should not show cancel button.
  DownloadRecord completedRecord = CreateTestDownloadRecord(
      "id2", "file2.jpg", web::DownloadTask::State::kComplete);
  DownloadListItem* completedItem = CreateTestDownloadListItem(completedRecord);

  [controller_ setDownloadListItems:@[ completedItem ]];

  // Trigger periodic update manually to process cached items.
  [controller_ performPeriodicUpdate];

  // Verify the completed item does not show cancel button.
  EXPECT_FALSE([completedItem cancelable]);

  // Verify the corresponding table view cell does not have accessoryView.
  EXPECT_GT(controller_.tableView.numberOfSections, 0);
  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  UITableViewCell* cell =
      [controller_.tableView.dataSource tableView:controller_.tableView
                            cellForRowAtIndexPath:indexPath];

  // Completed download should not have accessoryView.
  EXPECT_TRUE(cell.accessoryView == nil);
}

#pragma mark - Navigation Tests

/// Tests navigation item configuration has done button.
TEST_F(DownloadListTableViewControllerTest, TestNavigationItemConfiguration) {
  // Verify that the navigation item has a right bar button item (done button).
  EXPECT_TRUE(controller_.navigationItem.rightBarButtonItem);
}

/// Tests presentationControllerWillDismiss calls download list handler.
TEST_F(DownloadListTableViewControllerTest,
       TestPresentationControllerWillDismiss) {
  id mockPresentationController =
      OCMClassMock([UIPresentationController class]);

  [controller_ presentationControllerWillDismiss:mockPresentationController];

  EXPECT_TRUE(mock_download_list_handler_.hideDownloadListCalled);
}

#pragma mark - Data Source Tests

/// Tests data source configuration and reconfiguration on content changes.
TEST_F(DownloadListTableViewControllerTest,
       TestDataSourceConfigurationAndReconfiguration) {
  NSArray<DownloadListItem*>* items = CreateTestDownloadItems();

  // Set initial items and verify basic configuration.
  [controller_ setDownloadListItems:items];

  // Trigger periodic update manually to process cached items.
  [controller_ performPeriodicUpdate];

  // Verify that we can access cells for sections.
  if (controller_.tableView.numberOfSections > 0) {
    NSInteger rowCount = [controller_.tableView numberOfRowsInSection:0];
    EXPECT_GT(rowCount, 0);
  }

  // Create modified items (same IDs but different content).
  DownloadRecord modifiedRecord =
      CreateTestDownloadRecord("id1", "modified_file.pdf");
  DownloadListItem* modifiedItem = CreateTestDownloadListItem(modifiedRecord);

  NSArray<DownloadListItem*>* modifiedItems = @[
    modifiedItem,
    items[1],  // Unchanged.
    items[2]   // Unchanged.
  ];

  // Update with modified items and verify reconfiguration.
  [controller_ setDownloadListItems:modifiedItems];

  // Trigger periodic update manually to process cached items.
  [controller_ performPeriodicUpdate];

  // Verify the data source was updated.
  NSInteger totalItems = 0;
  for (NSInteger section = 0; section < controller_.tableView.numberOfSections;
       section++) {
    totalItems += [controller_.tableView numberOfRowsInSection:section];
  }
  EXPECT_EQ(static_cast<NSInteger>(modifiedItems.count), totalItems);
}

#pragma mark - New Functionality Tests

/// Tests periodic updates mechanism.
TEST_F(DownloadListTableViewControllerTest, TestPeriodicUpdates) {
  NSArray<DownloadListItem*>* items = CreateTestDownloadItems();

  // Set items but don't trigger update yet.
  [controller_ setDownloadListItems:items];

  // Initially no sections should be visible since no update was triggered.
  EXPECT_EQ(0, controller_.tableView.numberOfSections);

  // Manually trigger periodic update.
  [controller_ performPeriodicUpdate];

  // Now sections should be visible.
  EXPECT_GT(controller_.tableView.numberOfSections, 0);
}

/// Tests throttling mechanism behavior.
TEST_F(DownloadListTableViewControllerTest, TestThrottlingMechanism) {
  NSArray<DownloadListItem*>* items = CreateTestDownloadItems();

  // Set items multiple times rapidly (simulating rapid updates).
  [controller_ setDownloadListItems:items];
  [controller_ setDownloadListItems:items];
  [controller_ setDownloadListItems:items];

  // Should still be 0 sections until update is triggered.
  EXPECT_EQ(0, controller_.tableView.numberOfSections);

  // Single periodic update should process all cached changes.
  [controller_ performPeriodicUpdate];

  // Now should show sections.
  EXPECT_GT(controller_.tableView.numberOfSections, 0);
}

/// Tests filter header view functionality.
TEST_F(DownloadListTableViewControllerTest, TestFilterHeaderView) {
  // Initially no header should be shown.
  EXPECT_FALSE(controller_.tableView.tableHeaderView);

  // Show header.
  [controller_ setDownloadListHeaderShown:YES];

  // Header should now be visible.
  EXPECT_TRUE(controller_.tableView.tableHeaderView);

  // Hide header.
  [controller_ setDownloadListHeaderShown:NO];

  // Header should be hidden.
  EXPECT_FALSE(controller_.tableView.tableHeaderView);
}

/// Tests view lifecycle methods.
TEST_F(DownloadListTableViewControllerTest, TestViewLifecycleMethods) {
  NSArray<DownloadListItem*>* items = CreateTestDownloadItems();
  [controller_ setDownloadListItems:items];

  // Simulate view will disappear - should pause updates.
  [controller_ viewWillDisappear:NO];

  // Periodic update should not show items since updates are paused.
  [controller_ performPeriodicUpdate];
  EXPECT_EQ(0, controller_.tableView.numberOfSections);

  // Simulate view will appear - should resume updates.
  [controller_ viewWillAppear:NO];

  // Now periodic update should work.
  [controller_ performPeriodicUpdate];
  EXPECT_GT(controller_.tableView.numberOfSections, 0);
}

#pragma mark - Swipe-to-Delete Tests

/// Tests swipe actions configuration for items with delete action.
TEST_F(DownloadListTableViewControllerTest, TestSwipeToDeleteAction) {
  // Create test item with delete action available.
  DownloadRecord record = CreateTestDownloadRecord("id1", "file1.pdf");
  DownloadListItem* item = CreateTestDownloadListItem(record);

  // Mock the item to return delete action.
  id mockItem = OCMPartialMock(item);
  OCMStub([mockItem availableActions]).andReturn(DownloadListItemActionDelete);

  [controller_ setDownloadListItems:@[ mockItem ]];
  [controller_ performPeriodicUpdate];

  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];

  // Test trailing swipe actions configuration.
  UISwipeActionsConfiguration* config =
      [controller_ tableView:controller_.tableView
          trailingSwipeActionsConfigurationForRowAtIndexPath:indexPath];

  EXPECT_TRUE(config);
  EXPECT_EQ(1U, config.actions.count);
  EXPECT_EQ(UIContextualActionStyleDestructive,
            config.actions.firstObject.style);
}
