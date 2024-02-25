// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/identity_button_cell.h"
#import "ios/chrome/browser/ui/settings/downloads/identity_button_item.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/fake_save_to_photos_settings_mutator.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mutator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

namespace {

// Generates test image with given `width` and `height`.
UIImage* GenerateTestImage(CGFloat width, CGFloat height) {
  return ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
      CGSize{.width = width, .height = height}, UIColor.whiteColor);
}

}  // namespace

// Fake presentation delegate.
@interface FakeDownloadsSettingsTableViewControllerPresentationDelegate
    : NSObject <DownloadsSettingsTableViewControllerPresentationDelegate>

@property(nonatomic, assign) BOOL wasRemovedCalled;

@end

@implementation FakeDownloadsSettingsTableViewControllerPresentationDelegate

- (void)downloadsSettingsTableViewControllerWasRemoved:
    (DownloadsSettingsTableViewController*)controller {
  self.wasRemovedCalled = YES;
}

@end

// Fake action delegate.
@interface FakeDownloadsSettingsTableViewControllerActionDelegate
    : NSObject <DownloadsSettingsTableViewControllerActionDelegate>

@property(nonatomic, assign) BOOL selectSaveToPhotosAccountCalled;

@end

@implementation FakeDownloadsSettingsTableViewControllerActionDelegate

- (void)downloadsSettingsTableViewControllerOpenSaveToPhotosAccountSelection:
    (DownloadsSettingsTableViewController*)controller {
  self.selectSaveToPhotosAccountCalled = YES;
}

@end

// Unit tests for DownloadsSettingsTableViewController.
class DownloadsSettingsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() final {
    LegacyChromeTableViewControllerTest::SetUp();

    save_to_photos_mutator_ = [[FakeSaveToPhotosSettingsMutator alloc] init];
    presentation_delegate_ =
        [[FakeDownloadsSettingsTableViewControllerPresentationDelegate alloc]
            init];
    action_delegate_ =
        [[FakeDownloadsSettingsTableViewControllerActionDelegate alloc] init];

    CreateController();
  }

  LegacyChromeTableViewController* InstantiateController() final {
    DownloadsSettingsTableViewController* controller =
        [[DownloadsSettingsTableViewController alloc] init];
    controller.saveToPhotosSettingsMutator = save_to_photos_mutator_;
    controller.presentationDelegate = presentation_delegate_;
    controller.actionDelegate = action_delegate_;
    return controller;
  }

  FakeSaveToPhotosSettingsMutator* save_to_photos_mutator_;
  FakeDownloadsSettingsTableViewControllerPresentationDelegate*
      presentation_delegate_;
  FakeDownloadsSettingsTableViewControllerActionDelegate* action_delegate_;
};

// Tests that DownloadsSettingsTableViewController has the expected content.
// Also tests that the view controller correctly calls the mutator when the
// switch is toggled, the action delegate when the identity button is tapped,
// and the presentation delegate when the controller is removed.
TEST_F(DownloadsSettingsTableViewControllerTest,
       CanToggleAskEveryTimeAndSelectSaveToPhotosAccount) {
  // Push an items configuration through consumer interface.
  DownloadsSettingsTableViewController* downloadsController =
      base::apple::ObjCCast<DownloadsSettingsTableViewController>(controller());
  ASSERT_TRUE(downloadsController);
  UIImage* identity_avatar = GenerateTestImage(30.0, 30.0);
  [downloadsController setIdentityButtonAvatar:identity_avatar
                                          name:@"Firstname Lastname"
                                         email:@"firstname.lastname@example.org"
                                        gaiaID:@"mygaiaid"
                          askEveryTimeSwitchOn:YES];

  EXPECT_EQ(1, NumberOfSections());
  CheckTitleWithId(IDS_IOS_SETTINGS_DOWNLOADS_TITLE);

  // Test Save to Photos selection section has expected content.
  CheckSectionHeaderWithId(IDS_IOS_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_HEADER, 0);
  IdentityButtonItem* identity_button_item =
      base::apple::ObjCCast<IdentityButtonItem>(GetTableViewItem(0, 0));
  ASSERT_TRUE(identity_button_item);
  EXPECT_EQ(identity_avatar, identity_button_item.identityAvatar);
  EXPECT_NSEQ(@"Firstname Lastname", identity_button_item.identityName);
  EXPECT_NSEQ(@"firstname.lastname@example.org",
              identity_button_item.identityEmail);
  EXPECT_NSEQ(@"mygaiaid", identity_button_item.identityGaiaID);
  EXPECT_TRUE(identity_button_item.enabled);
  EXPECT_EQ(IdentityButtonControlArrowRight,
            identity_button_item.arrowDirection);
  EXPECT_EQ(IdentityViewStyleConsistency,
            identity_button_item.identityViewStyle);
  CheckSwitchCellStateAndTextWithId(
      YES, IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_ASK_EVERY_TIME, 0, 1);

  // Test that disabling and re-enabling the switch updates the mutator.
  EXPECT_FALSE(save_to_photos_mutator_.selectedIdentityGaiaID);
  TableViewSwitchCell* switchCell = base::apple::ObjCCast<TableViewSwitchCell>(
      [controller() tableView:controller().tableView
          cellForRowAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:0]]);
  ASSERT_TRUE(switchCell);
  switchCell.switchView.on = NO;
  [switchCell.switchView
      sendActionsForControlEvents:UIControlEventValueChanged];
  EXPECT_FALSE(save_to_photos_mutator_.askWhichAccountToUseEveryTime);
  switchCell.switchView.on = YES;
  [switchCell.switchView
      sendActionsForControlEvents:UIControlEventValueChanged];
  EXPECT_TRUE(save_to_photos_mutator_.askWhichAccountToUseEveryTime);

  // Test that tapping the Identity button calls the action delegate.
  EXPECT_FALSE(action_delegate_.selectSaveToPhotosAccountCalled);
  IdentityButtonCell* identityButtonCell =
      base::apple::ObjCCast<IdentityButtonCell>([controller()
                      tableView:controller().tableView
          cellForRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]]);
  [identityButtonCell.identityButtonControl
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(action_delegate_.selectSaveToPhotosAccountCalled);

  // Test that removing the controller calls the presentation delegate.
  EXPECT_FALSE(presentation_delegate_.wasRemovedCalled);
  [controller() didMoveToParentViewController:nil];
  EXPECT_TRUE(presentation_delegate_.wasRemovedCalled);
}
