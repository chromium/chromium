// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/universal_opt_out_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/prefs/pref_service.h"
#import "components/universal_optout/prefs.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/universal_optout/model/constants.h"
#import "ios/chrome/browser/web_extension/model/extension_service_factory.h"
#import "ios/chrome/browser/web_extension/model/fake_extension_service.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

class UniversalOptOutTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ExtensionServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeExtensionService>();
            }));
    profile_ = std::move(builder).Build();
    profile_->GetPrefs()->SetBoolean(
        universal_optout::prefs::kUniversalOptOutEnabled, false);
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[UniversalOptOutTableViewController alloc]
        initWithProfile:profile_.get()];
  }

  FakeExtensionService* fake_extension_service() {
    return static_cast<FakeExtensionService*>(
        ExtensionServiceFactory::GetForProfile(profile_.get()));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that toggling the switch properly updates the preference.
TEST_F(UniversalOptOutTableViewControllerTest, TestToggleSwitchUpdatesPref) {
  base::UserActionTester user_action_tester;

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
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Privacy.UniversalOptOut.SettingsToggleOn"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Privacy.UniversalOptOut.SettingsToggleOff"));

  switchView.on = NO;
  [switchView sendActionsForControlEvents:UIControlEventValueChanged];

  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      universal_optout::prefs::kUniversalOptOutEnabled));
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Privacy.UniversalOptOut.SettingsToggleOn"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Privacy.UniversalOptOut.SettingsToggleOff"));
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
  EXPECT_EQ(GURL(kUniversalOptOutLearnMoreURL), footerItem.urls[0].gurl);

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
        return command && command.URL == GURL(kUniversalOptOutLearnMoreURL);
      }]]);

  [optOutController view:footerView didTapLinkURL:footerItem.urls[0]];

  EXPECT_OCMOCK_VERIFY(mockSceneHandler);
}

// Tests that when ExtensionService reports a load error, the error section
// is added with the localized error message and "Learn more" button, the
// footer is attached to the error section, and tapping "Learn more" opens
// the error URL in a new tab.
TEST_F(UniversalOptOutTableViewControllerTest,
       TestErrorSectionWhenExtensionHasLoadError) {
  fake_extension_service()->SetHasLoadError(true);

  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(2, NumberOfItemsInSection(1));

  TableViewDetailIconItem* messageItem =
      base::apple::ObjCCastStrict<TableViewDetailIconItem>(
          GetTableViewItem(1, 0));
  ASSERT_TRUE(messageItem != nil);

  int expectedMessageId =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
          ? IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT_ERROR_IPAD
          : IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT_ERROR_IPHONE;
  EXPECT_NSEQ(l10n_util::GetNSString(expectedMessageId), messageItem.text);
  EXPECT_NSEQ(kUniversalOptOutErrorMessageItemAccessibilityIdentifier,
              messageItem.accessibilityIdentifier);
  EXPECT_EQ(UITableViewCellSelectionStyleNone, messageItem.selectionStyle);
  EXPECT_EQ(UIStackViewAlignmentTop, messageItem.verticalAlignment);

  TableViewTextItem* learnMoreItem =
      base::apple::ObjCCastStrict<TableViewTextItem>(GetTableViewItem(1, 1));
  ASSERT_TRUE(learnMoreItem != nil);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT_ERROR_LEARN_MORE),
              learnMoreItem.text);
  EXPECT_NSEQ(kUniversalOptOutErrorLearnMoreItemAccessibilityIdentifier,
              learnMoreItem.accessibilityIdentifier);
  EXPECT_TRUE(learnMoreItem.reservesLeadingSpace);
  EXPECT_EQ(UIAccessibilityTraitButton,
            learnMoreItem.accessibilityTraits & UIAccessibilityTraitButton);

  // Verify that the footer is on section 1 (the error section), not section 0.
  EXPECT_EQ(nil, [controller().tableViewModel footerForSectionIndex:0]);
  TableViewLinkHeaderFooterItem* footerItem =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(
          [controller().tableViewModel footerForSectionIndex:1]);
  ASSERT_TRUE(footerItem != nil);
  ASSERT_EQ(1u, footerItem.urls.count);
  EXPECT_EQ(GURL(kUniversalOptOutLearnMoreURL), footerItem.urls[0].gurl);

  UniversalOptOutTableViewController* optOutController =
      base::apple::ObjCCastStrict<UniversalOptOutTableViewController>(
          controller());

  // Verify highlighting: only the "Learn more" row is highlightable.
  EXPECT_FALSE([optOutController tableView:optOutController.tableView
             shouldHighlightRowAtIndexPath:[NSIndexPath indexPathForRow:0
                                                              inSection:1]]);
  EXPECT_TRUE([optOutController tableView:optOutController.tableView
            shouldHighlightRowAtIndexPath:[NSIndexPath indexPathForRow:1
                                                             inSection:1]]);

  // Verify selecting the "Learn more" row opens the error URL.
  id mockSceneHandler = OCMProtocolMock(@protocol(SceneCommands));
  optOutController.sceneHandler = mockSceneHandler;

  OCMExpect([mockSceneHandler
      closePresentedViewsAndOpenURL:[OCMArg checkWithBlock:^BOOL(id value) {
        OpenNewTabCommand* command =
            base::apple::ObjCCast<OpenNewTabCommand>(value);
        return command && command.URL == GURL(kUniversalOptOutLearnMoreURL);
      }]]);

  [optOutController tableView:optOutController.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:1]];

  EXPECT_OCMOCK_VERIFY(mockSceneHandler);
}

// Tests that changing the ExtensionService load error state dynamically updates
// the table view model and sections.
TEST_F(UniversalOptOutTableViewControllerTest, TestDynamicLoadErrorChanges) {
  CreateController();
  CheckController();

  // Initially no error.
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_TRUE([controller().tableViewModel footerForSectionIndex:0] != nil);

  // Transition to load error.
  fake_extension_service()->SetHasLoadError(true);

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  TableViewDetailIconItem* messageItem =
      base::apple::ObjCCastStrict<TableViewDetailIconItem>(
          GetTableViewItem(1, 0));
  ASSERT_TRUE(messageItem != nil);
  TableViewTextItem* learnMoreItem =
      base::apple::ObjCCastStrict<TableViewTextItem>(GetTableViewItem(1, 1));
  ASSERT_TRUE(learnMoreItem != nil);
  EXPECT_EQ(nil, [controller().tableViewModel footerForSectionIndex:0]);
  EXPECT_TRUE([controller().tableViewModel footerForSectionIndex:1] != nil);

  // Transition back to no load error.
  fake_extension_service()->SetHasLoadError(false);

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_TRUE([controller().tableViewModel footerForSectionIndex:0] != nil);
}

}  // namespace
