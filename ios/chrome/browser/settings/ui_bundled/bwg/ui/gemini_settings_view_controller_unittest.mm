// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Accessibility identifier for the Camera permission switch row.
NSString* const kCameraCellId = @"CameraCellId";

class GeminiSettingsViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    GeminiSettingsViewController* viewController =
        [[GeminiSettingsViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
    viewController.mutator = mutator_;
    return viewController;
  }

  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    mutator_ = OCMProtocolMock(@protocol(GeminiSettingsMutator));
  }

  GeminiSettingsViewController* GeminiController() {
    return base::apple::ObjCCastStrict<GeminiSettingsViewController>(
        controller());
  }

  // Returns the index path of the item matching `accessibility_id`, or `nil`
  // if no item matches.
  NSIndexPath* IndexPathForAccessibilityId(NSString* accessibility_id) {
    TableViewModel* model = controller().tableViewModel;
    for (NSInteger section = 0; section < [model numberOfSections]; ++section) {
      NSInteger item_count = [model numberOfItemsInSection:section];
      for (NSInteger row = 0; row < item_count; ++row) {
        NSIndexPath* indexPath = [NSIndexPath indexPathForRow:row
                                                    inSection:section];
        TableViewItem* item = [model itemAtIndexPath:indexPath];
        if ([item.accessibilityIdentifier isEqualToString:accessibility_id]) {
          return indexPath;
        }
      }
    }
    return nil;
  }

  id mutator_ = nil;
};

// Tests that the Camera row is rendered as an inline `TableViewSwitchItem` when
// Image Remix is available and updates the camera permission preference and
// user action metrics when toggled.
TEST_F(GeminiSettingsViewControllerTest, CameraPermissionSwitchToggle) {
  base::UserActionTester user_action_tester;
  OCMStub([mutator_ isImageRemixAvailable]).andReturn(YES);

  CreateController();
  [GeminiController() setCameraPermissionEnabled:YES];
  CheckController();

  NSIndexPath* cameraIndexPath = IndexPathForAccessibilityId(kCameraCellId);
  ASSERT_TRUE(cameraIndexPath);
  TableViewSwitchItem* cameraSwitchItem =
      base::apple::ObjCCast<TableViewSwitchItem>(
          [controller().tableViewModel itemAtIndexPath:cameraIndexPath]);
  ASSERT_TRUE(cameraSwitchItem);
  EXPECT_TRUE(cameraSwitchItem.isOn);

  UITableViewCell* cell = [controller() tableView:controller().tableView
                            cellForRowAtIndexPath:cameraIndexPath];
  UISwitch* switchView = chrome_test_util::FindViewByClass<UISwitch>(cell);
  ASSERT_TRUE(switchView);
  EXPECT_TRUE(switchView.isOn);

  OCMExpect([mutator_ setCameraPermissionPref:NO]);
  switchView.on = NO;
  [switchView sendActionsForControlEvents:UIControlEventValueChanged];

  EXPECT_OCMOCK_VERIFY(mutator_);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "MobileGeminiCameraSettingsGeminiCameraPermissionToggledOff"));
}

// Tests that `setCameraPermissionEnabled:` reconfigures the Camera switch item
// after the view is loaded.
TEST_F(GeminiSettingsViewControllerTest,
       SetCameraPermissionEnabledReconfiguresSwitch) {
  OCMStub([mutator_ isImageRemixAvailable]).andReturn(YES);

  CreateController();
  [GeminiController() setCameraPermissionEnabled:YES];
  CheckController();

  NSIndexPath* cameraIndexPath = IndexPathForAccessibilityId(kCameraCellId);
  ASSERT_TRUE(cameraIndexPath);
  TableViewSwitchItem* cameraSwitchItem =
      base::apple::ObjCCast<TableViewSwitchItem>(
          [controller().tableViewModel itemAtIndexPath:cameraIndexPath]);
  ASSERT_TRUE(cameraSwitchItem);
  EXPECT_TRUE(cameraSwitchItem.isOn);

  [GeminiController() setCameraPermissionEnabled:NO];
  EXPECT_FALSE(cameraSwitchItem.isOn);
}

// Tests that the Camera section is omitted when Image Remix is unavailable and
// `setCameraPermissionEnabled:` is a safe no-op after the view is loaded.
TEST_F(GeminiSettingsViewControllerTest,
       CameraSectionOmittedWhenImageRemixUnavailable) {
  OCMStub([mutator_ isImageRemixAvailable]).andReturn(NO);

  CreateController();
  CheckController();

  EXPECT_FALSE(IndexPathForAccessibilityId(kCameraCellId));

  [GeminiController() setCameraPermissionEnabled:YES];
  EXPECT_FALSE(IndexPathForAccessibilityId(kCameraCellId));
}

}  // namespace
