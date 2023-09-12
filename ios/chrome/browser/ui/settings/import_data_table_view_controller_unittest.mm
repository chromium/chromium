// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, ItemIndex) {
  kKeepDataSeparateItemIndex = 0,
  kImportDataItemIndex = 1,
};

}  // namespace

@interface ImportDataControllerTestDelegate
    : NSObject <ImportDataControllerDelegate>
@property(nonatomic, readonly) BOOL didChooseClearDataPolicyCalled;
@property(nonatomic, readonly) ShouldClearData shouldClearData;
@end

@implementation ImportDataControllerTestDelegate

@synthesize didChooseClearDataPolicyCalled = _didChooseClearDataPolicyCalled;
@synthesize shouldClearData = _shouldClearData;

- (void)didChooseClearDataPolicy:(ImportDataTableViewController*)controller
                 shouldClearData:(ShouldClearData)shouldClearData {
  _didChooseClearDataPolicyCalled = YES;
  _shouldClearData = shouldClearData;
}

@end

namespace {

class ImportDataTableViewControllerTest : public ChromeTableViewControllerTest {
 public:
  ImportDataControllerTestDelegate* delegate() { return delegate_; }

 protected:
  ChromeTableViewController* InstantiateController() override {
    delegate_ = [[ImportDataControllerTestDelegate alloc] init];
    return [[ImportDataTableViewController alloc]
        initWithDelegate:delegate_
               fromEmail:@"fromEmail@gmail.com"
                 toEmail:@"toEmail@gmail.com"];
  }

  void SelectRowAtIndex(NSInteger itemIndex) {
    ImportDataTableViewController* import_data_controller =
        base::apple::ObjCCastStrict<ImportDataTableViewController>(
            controller());
    NSIndexPath* itemPath = [NSIndexPath indexPathForItem:itemIndex
                                                inSection:1];
    [import_data_controller tableView:[import_data_controller tableView]
              didSelectRowAtIndexPath:itemPath];
  }

  ImportDataControllerTestDelegate* delegate_;
};

TEST_F(ImportDataTableViewControllerTest, TestModelSignedOut) {
  CreateController();
  CheckController();
  ASSERT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  SettingsImageDetailTextItem* item = GetTableViewItem(0, 0);
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_IOS_OPTIONS_IMPORT_DATA_HEADER,
                              base::SysNSStringToUTF16(@"fromEmail@gmail.com")),
      item.text);
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_KEEP_SEPARATE_TITLE),
      l10n_util::GetNSString(
          IDS_IOS_OPTIONS_IMPORT_DATA_KEEP_SEPARATE_SUBTITLE),
      1, kKeepDataSeparateItemIndex);
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_IMPORT_TITLE),
      l10n_util::GetNSStringF(IDS_IOS_OPTIONS_IMPORT_DATA_IMPORT_SUBTITLE,
                              base::SysNSStringToUTF16(@"toEmail@gmail.com")),
      1, kImportDataItemIndex);

  // No item is selected by default.
  CheckAccessoryType(UITableViewCellAccessoryNone, 1,
                     kKeepDataSeparateItemIndex);
  CheckAccessoryType(UITableViewCellAccessoryNone, 1, kImportDataItemIndex);

  // Continue button is disabled by default.
  EXPECT_FALSE(controller().navigationItem.rightBarButtonItem.enabled);
}

// Tests that checking a checkbox correctly uncheck the other one.
TEST_F(ImportDataTableViewControllerTest, TestUniqueBoxChecked) {
  CreateController();

  ImportDataTableViewController* import_data_controller =
      base::apple::ObjCCastStrict<ImportDataTableViewController>(controller());
  NSIndexPath* importIndexPath =
      [NSIndexPath indexPathForItem:kImportDataItemIndex inSection:1];
  NSIndexPath* keepSeparateIndexPath =
      [NSIndexPath indexPathForItem:kKeepDataSeparateItemIndex inSection:1];
  SettingsImageDetailTextItem* importItem =
      base::apple::ObjCCastStrict<SettingsImageDetailTextItem>(
          [import_data_controller.tableViewModel
              itemAtIndexPath:importIndexPath]);
  SettingsImageDetailTextItem* keepSeparateItem =
      base::apple::ObjCCastStrict<SettingsImageDetailTextItem>(
          [import_data_controller.tableViewModel
              itemAtIndexPath:keepSeparateIndexPath]);
  EXPECT_EQ(UITableViewCellAccessoryNone, importItem.accessoryType);
  EXPECT_EQ(UITableViewCellAccessoryNone, keepSeparateItem.accessoryType);

  SelectRowAtIndex(kImportDataItemIndex);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, importItem.accessoryType);
  EXPECT_EQ(UITableViewCellAccessoryNone, keepSeparateItem.accessoryType);

  SelectRowAtIndex(kKeepDataSeparateItemIndex);
  EXPECT_EQ(UITableViewCellAccessoryNone, importItem.accessoryType);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, keepSeparateItem.accessoryType);
}

TEST_F(ImportDataTableViewControllerTest, TestImportDataCalled) {
  CreateController();

  EXPECT_FALSE(delegate().didChooseClearDataPolicyCalled);
  UIBarButtonItem* continueItem =
      controller().navigationItem.rightBarButtonItem;
  ASSERT_TRUE(continueItem);

  EXPECT_FALSE(continueItem.enabled);
  SelectRowAtIndex(kImportDataItemIndex);
  EXPECT_TRUE(continueItem.enabled);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [continueItem.target performSelector:continueItem.action];
#pragma clang diagnostic pop

  EXPECT_TRUE(delegate().didChooseClearDataPolicyCalled);
  EXPECT_EQ(SHOULD_CLEAR_DATA_MERGE_DATA, delegate().shouldClearData);
}

TEST_F(ImportDataTableViewControllerTest, TestClearDataCalled) {
  CreateController();

  EXPECT_FALSE(delegate().didChooseClearDataPolicyCalled);
  UIBarButtonItem* continueItem =
      controller().navigationItem.rightBarButtonItem;
  ASSERT_TRUE(continueItem);

  EXPECT_FALSE(continueItem.enabled);
  SelectRowAtIndex(kKeepDataSeparateItemIndex);
  EXPECT_TRUE(continueItem.enabled);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [continueItem.target performSelector:continueItem.action];
#pragma clang diagnostic pop

  EXPECT_TRUE(delegate().didChooseClearDataPolicyCalled);
  EXPECT_EQ(SHOULD_CLEAR_DATA_CLEAR_DATA, delegate().shouldClearData);
}

}  // namespace
