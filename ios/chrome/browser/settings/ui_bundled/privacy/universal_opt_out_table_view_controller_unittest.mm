// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/universal_opt_out_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/prefs/pref_service.h"
#import "components/universal_optout/prefs.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

constexpr char kUniversalOptOutLearnMoreUrl[] =
    "https://support.google.com/chrome?p=opt_out_request";

class UniversalOptOutTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    profile_->GetPrefs()->SetBoolean(
        universal_optout::prefs::kUniversalOptOutEnabled, false);
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[UniversalOptOutTableViewController alloc]
        initWithProfile:profile_.get()];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that toggling the switch properly updates the preference.
TEST_F(UniversalOptOutTableViewControllerTest, TestToggleSwitchUpdatesPref) {
  CreateController();
  CheckController();

  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(GetTableViewItem(0, 0));
  ASSERT_TRUE(switchItem != nil);
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      universal_optout::prefs::kUniversalOptOutEnabled));

  TableViewCellContentView* contentView =
      base::apple::ObjCCastStrict<TableViewCellContentView>([[controller()
                      tableView:controller().tableView
          cellForRowAtIndexPath:[NSIndexPath indexPathForItem:0
                                                    inSection:0]] contentView]);
  ASSERT_TRUE(contentView);
  SwitchContentView* switchContentView =
      base::apple::ObjCCastStrict<SwitchContentView>(
          [contentView trailingContentViewForTesting]);
  ASSERT_TRUE(switchContentView);
  UISwitch* switchView = [switchContentView switchForTesting];
  ASSERT_TRUE(switchView);

  switchView.on = YES;
  [switchView sendActionsForControlEvents:UIControlEventValueChanged];

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      universal_optout::prefs::kUniversalOptOutEnabled));
  EXPECT_TRUE(switchItem.isOn);
}

// Tests that the footer link URL is properly set and simulating a tap on the
// link opens the URL.
TEST_F(UniversalOptOutTableViewControllerTest, TestFooterURLAndLinkTap) {
  CreateController();
  CheckController();

  TableViewLinkHeaderFooterItem* footerItem =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(
          [controller().tableViewModel footerForSectionIndex:0]);
  ASSERT_TRUE(footerItem != nil);
  ASSERT_EQ(1u, footerItem.urls.count);
  EXPECT_EQ(GURL(kUniversalOptOutLearnMoreUrl), footerItem.urls[0].gurl);

  UniversalOptOutTableViewController* optOutController =
      base::apple::ObjCCastStrict<UniversalOptOutTableViewController>(
          controller());
  id mockSceneHandler = OCMProtocolMock(@protocol(SceneCommands));
  optOutController.sceneHandler = mockSceneHandler;

  TableViewLinkHeaderFooterView* footerView =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(
          [optOutController tableView:optOutController.tableView
               viewForFooterInSection:0]);
  ASSERT_TRUE(footerView != nil);
  EXPECT_NSEQ(footerView.delegate, optOutController);

  OCMExpect([mockSceneHandler
      closePresentedViewsAndOpenURL:[OCMArg checkWithBlock:^BOOL(id value) {
        OpenNewTabCommand* command =
            base::apple::ObjCCast<OpenNewTabCommand>(value);
        return command && command.URL == GURL(kUniversalOptOutLearnMoreUrl);
      }]]);

  [optOutController view:footerView didTapLinkURL:footerItem.urls[0]];

  EXPECT_OCMOCK_VERIFY(mockSceneHandler);
}

}  // namespace
