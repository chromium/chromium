// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Test SettingsRootTableViewController subclass that conformfs to
// SettingsControllerProtocol. Used to test that 'settingsWillBeDismissed' is
// called at the right time.
@interface FakeSettingsRootTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

@property(nonatomic, assign) BOOL settingsWillBeDismissedCalled;

@end

@implementation FakeSettingsRootTableViewController

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
}

- (void)reportBackUserAction {
}

- (void)settingsWillBeDismissed {
  _settingsWillBeDismissedCalled = YES;
}

@end

class SettingsRootTableViewControllerTest : public PlatformTest {
 public:
  SettingsRootTableViewController* Controller() {
    return [[SettingsRootTableViewController alloc]
        initWithStyle:UITableViewStylePlain];
  }

  SettingsNavigationController* NavigationController() {
    if (!browser_) {
      TestProfileIOS::Builder builder;
      profile_ = std::move(builder).Build();
      browser_ = std::make_unique<TestBrowser>(profile_.get());
    }
    return [[SettingsNavigationController alloc]
        initWithRootViewController:nil
                           browser:browser_.get()
                          delegate:nil];
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

TEST_F(SettingsRootTableViewControllerTest, TestUpdateUIForEditState) {
  SettingsRootTableViewController* controller = Controller();

  id mockController = OCMPartialMock(controller);
  SettingsNavigationController* navigationController = NavigationController();
  OCMStub([mockController navigationController])
      .andReturn(navigationController);

  // Check that there the navigation controller's button if the table view isn't
  // edited and the controller has the default behavior for
  // `shouldShowEditButton`. Also check that toolbar is hidden when
  // `shouldHideToolbar` returns YES.
  controller.tableView.editing = NO;
  OCMExpect([mockController shouldHideToolbar]).andReturn(YES);
  [controller updateUIForEditState];
  UIBarButtonItem* item = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:nil
                           action:nil];
  EXPECT_NSEQ(item.title, controller.navigationItem.rightBarButtonItem.title);
  EXPECT_TRUE(controller.navigationController.toolbarHidden);

  // Check that there the OK button if the table view is being edited and the
  // controller has the default behavior for `shouldShowEditButton`. Also check
  // that toolbar is not hidden when `shouldHideToolbar` returns NO.
  controller.tableView.editing = YES;
  OCMExpect([mockController shouldHideToolbar]).andReturn(NO);
  [controller updateUIForEditState];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON),
              controller.navigationItem.rightBarButtonItem.title);
  EXPECT_FALSE(controller.navigationController.toolbarHidden);

  // Check that there the OK button if the table view isn't edited and the
  // controller returns YES for `shouldShowEditButton`. Also check that toolbar
  // is not hidden when `shouldHideToolbar` returns NO.
  controller.tableView.editing = NO;
  OCMStub([mockController shouldShowEditButton]).andReturn(YES);
  OCMExpect([mockController shouldHideToolbar]).andReturn(NO);
  [controller updateUIForEditState];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
              controller.navigationItem.rightBarButtonItem.title);
  EXPECT_FALSE(controller.navigationController.toolbarHidden);
  [navigationController cleanUpSettings];
}

// Tests that the delete button in the bottom toolbar is displayed only when the
// collection is being edited.
TEST_F(SettingsRootTableViewControllerTest, TestDeleteToolbar) {
  SettingsRootTableViewController* controller = Controller();

  id mockController = OCMPartialMock(controller);
  id mockTableView = OCMPartialMock(controller.tableView);
  SettingsNavigationController* navigationController = NavigationController();
  OCMStub([mockController navigationController])
      .andReturn(navigationController);

  NSIndexPath* testIndexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  NSMutableArray* testSelectedItems = [NSMutableArray array];
  [testSelectedItems addObject:testIndexPath];
  ASSERT_TRUE(navigationController.toolbarHidden);

  // Test that if the table view isn't being edited, the toolbar is still
  // hidden.
  controller.tableView.editing = NO;
  [controller tableView:controller.tableView
      didSelectRowAtIndexPath:testIndexPath];
  EXPECT_TRUE(navigationController.toolbarHidden);

  // Test that if the table view is being edited, the toolbar is displayed when
  // the element is selected.
  controller.tableView.editing = YES;
  EXPECT_TRUE(navigationController.toolbarHidden);
  [controller tableView:controller.tableView
      didSelectRowAtIndexPath:testIndexPath];
  EXPECT_FALSE(navigationController.toolbarHidden);

  // Test that if the table view is being edited, the toolbar is not hidden when
  // an element is deselected but some elements are still selected.
  OCMStub([mockTableView indexPathsForSelectedRows])
      .andReturn(testSelectedItems);
  [controller tableView:controller.tableView
      didDeselectRowAtIndexPath:testIndexPath];
  EXPECT_FALSE(navigationController.toolbarHidden);

  // Test that if the table view is being edited, the toolbar is hidden when the
  // last selected element is deselected.
  [testSelectedItems removeAllObjects];
  [controller tableView:controller.tableView
      didDeselectRowAtIndexPath:testIndexPath];
  EXPECT_TRUE(navigationController.toolbarHidden);
  [navigationController cleanUpSettings];
}

// Tests that a subclass of SettingsRootViewController that implements the
// SettingsControllerProtocol does not have its implementation of
// 'settingsWillBeDismissed' called when 'willMoveToParentViewController' is
// triggered. 'settingsWillBeDismissed' should only be called when
// 'didMoveToParentViewController' is triggered. Otherwise, a crash may occur as
// some of the subclass objects can be reset before the subclass is deallocated.
// A call to 'willMoveToParentViewController' does not necessarily mean that the
// view controller will be removed from the navigation stack.
TEST_F(SettingsRootTableViewControllerTest,
       TestSettingsWillBeDismissedCallTiming) {
  FakeSettingsRootTableViewController* controller =
      [[FakeSettingsRootTableViewController alloc]
          initWithStyle:UITableViewStylePlain];

  [controller willMoveToParentViewController:nil];
  EXPECT_FALSE(controller.settingsWillBeDismissedCalled);

  [controller didMoveToParentViewController:nil];
  EXPECT_TRUE(controller.settingsWillBeDismissedCalled);
}
