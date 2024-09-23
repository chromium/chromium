// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/password_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/with_feature_override.h"
#import "components/plus_addresses/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_cell.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ui/base/device_form_factor.h"
#import "url/gurl.h"

namespace {

enum ItemType : NSInteger {
  kItemTypeSampleOne = kItemTypeEnumZero,
  kItemTypeSampleTwo,
  kItemTypeSampleThree
};

}  // namespace

class PasswordViewControllerTest : public LegacyChromeTableViewControllerTest,
                                   public base::test::WithFeatureOverride {
 public:
  PasswordViewControllerTest()
      : base::test::WithFeatureOverride(
            plus_addresses::features::kPlusAddressIOSManualFallbackEnabled) {}

 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    CreateController();
    CheckController();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    PasswordViewController* view_controller =
        [[PasswordViewController alloc] initWithSearchController:nil];
    [view_controller loadModel];
    return view_controller;
  }

  // Returns the header item at `section`.
  id GetHeaderItem(int section) {
    return [controller().tableViewModel headerForSectionIndex:section];
  }

  // Returns the type of the table view item at `item` in `section`.
  NSInteger GetTableViewItemType(int section, int item) {
    return base::apple::ObjCCastStrict<TableViewItem>(
               GetTableViewItem(section, item))
        .type;
  }
};

// Tests the following:
// 1. "No password items present" message is shown alongside the action items if
// there are no data items.
// 2. "No password items present" message is shown irrespective of the plus
// address items.
// 3. "No password items present" message is removed once there are password
// items to be shown in the view.
TEST_P(PasswordViewControllerTest, CheckNoDataItemsMessageRemoved) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // TODO(crbug.com/327838014): Fails on iPad.
    return;
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIOSKeyboardAccessoryUpgrade);

  PasswordViewController* password_view_controller =
      base::apple::ObjCCastStrict<PasswordViewController>(controller());

  ManualFillActionItem* action_item =
      [[ManualFillActionItem alloc] initWithTitle:nil action:nil];
  action_item.type = kItemTypeSampleOne;

  // First, send no data items so that the "no password items to show" message
  // is displayed.
  [password_view_controller presentCredentials:@[]];
  [password_view_controller presentActions:@[ action_item ]];

  BOOL plus_address_enabled = base::FeatureList::IsEnabled(
      plus_addresses::features::kPlusAddressIOSManualFallbackEnabled);

  if (plus_address_enabled) {
    [password_view_controller presentPlusAddresses:@[]];
  }

  // Make sure that the table view content is as expected.
  EXPECT_EQ(NumberOfSections(), 2);
  EXPECT_TRUE(GetHeaderItem(/*section=*/0));
  EXPECT_EQ(NumberOfItemsInSection(0), 0);
  EXPECT_EQ(NumberOfItemsInSection(1), 1);
  EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/0),
            kItemTypeSampleOne);

  // Add a plus address item to the view.
  if (plus_address_enabled) {
    ManualFillPlusAddress* plus_address = [[ManualFillPlusAddress alloc]
        initWithPlusAddress:@"random@example.com"
                   siteName:@""
                       host:@""
                        URL:GURL("https://example.com")];
    ManualFillPlusAddressItem* item =
        [[ManualFillPlusAddressItem alloc] initWithPlusAddress:plus_address
                                               contentInjector:nil
                                                   menuActions:@[]
                                   cellIndexAccessibilityLabel:nil];
    [password_view_controller presentPlusAddresses:@[ item ]];
    // Override the type for the test.
    item.type = kItemTypeSampleTwo;

    // Ensure that the "no password items to show" message is presented.
    EXPECT_EQ(NumberOfSections(), 3);
    EXPECT_TRUE(GetHeaderItem(/*section=*/1));
    EXPECT_EQ(NumberOfItemsInSection(0), 1);
    EXPECT_EQ(NumberOfItemsInSection(1), 0);
    EXPECT_EQ(NumberOfItemsInSection(2), 1);
    EXPECT_EQ(GetTableViewItemType(/*section=*/0, /*item=*/0),
              kItemTypeSampleTwo);
    EXPECT_EQ(GetTableViewItemType(/*section=*/2, /*item=*/0),
              kItemTypeSampleOne);
  }

  ManualFillCredential* credential = [[ManualFillCredential alloc]
      initWithUsername:@"test@example.com"
              password:@""
              siteName:@""
                  host:@""
                   URL:GURL("https://example.com")];

  // Add an password data item.
  ManualFillCredentialItem* password_item =
      [[ManualFillCredentialItem alloc] initWithCredential:credential
                                 isConnectedToPreviousItem:NO
                                     isConnectedToNextItem:NO
                                           contentInjector:nil
                                               menuActions:@[]
                                                 cellIndex:0
                               cellIndexAccessibilityLabel:nil
                                    showAutofillFormButton:NO
                                   fromAllPasswordsContext:NO];
  [password_view_controller presentCredentials:@[ password_item ]];
  // Override the type for the test.
  password_item.type = kItemTypeSampleThree;

  // Check that the "no password present" message is removed.
  EXPECT_EQ(NumberOfSections(), plus_address_enabled ? 3 : 2);
  EXPECT_FALSE(GetHeaderItem(/*section=*/0));
  EXPECT_FALSE(GetHeaderItem(/*section=*/1));
  EXPECT_EQ(NumberOfItemsInSection(0), 1);
  EXPECT_EQ(NumberOfItemsInSection(1), 1);
  if (plus_address_enabled) {
    EXPECT_EQ(NumberOfItemsInSection(2), 1);
    EXPECT_EQ(GetTableViewItemType(/*section=*/0, /*item=*/0),
              kItemTypeSampleTwo);
  }
  EXPECT_EQ(GetTableViewItemType(/*section=*/plus_address_enabled ? 2 : 1,
                                 /*item=*/0),
            kItemTypeSampleOne);
  EXPECT_EQ(GetTableViewItemType(/*section=*/plus_address_enabled ? 1 : 0,
                                 /*item=*/0),
            kItemTypeSampleThree);
}

// Tests that the plus address which exists in a credential is not added as a
// manual fallback suggestion.
TEST_P(PasswordViewControllerTest, PlusAddressInCredentialSection) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // TODO(crbug.com/327838014): Fails on iPad.
    return;
  }

  // Test is irrelevant without the feature.
  if (!base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressIOSManualFallbackEnabled)) {
    return;
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIOSKeyboardAccessoryUpgrade);

  PasswordViewController* password_view_controller =
      base::apple::ObjCCastStrict<PasswordViewController>(controller());

  ManualFillPlusAddress* plus_address = [[ManualFillPlusAddress alloc]
      initWithPlusAddress:@"test@example.com"
                 siteName:@""
                     host:@""
                      URL:GURL("https://example.com")];

  ManualFillPlusAddressItem* plus_address_item =
      [[ManualFillPlusAddressItem alloc] initWithPlusAddress:plus_address
                                             contentInjector:nil
                                                 menuActions:@[]
                                 cellIndexAccessibilityLabel:nil];
  [password_view_controller presentPlusAddresses:@[ plus_address_item ]];
  // Override the type for the test.
  plus_address_item.type = kItemTypeSampleTwo;

  ManualFillCredential* credential = [[ManualFillCredential alloc]
      initWithUsername:@"test@example.com"
              password:@""
              siteName:@""
                  host:@""
                   URL:GURL("https://example.com")];
  ManualFillCredentialItem* password_item =
      [[ManualFillCredentialItem alloc] initWithCredential:credential
                                 isConnectedToPreviousItem:NO
                                     isConnectedToNextItem:NO
                                           contentInjector:nil
                                               menuActions:@[]
                                                 cellIndex:0
                               cellIndexAccessibilityLabel:nil
                                    showAutofillFormButton:NO
                                   fromAllPasswordsContext:NO];
  [password_view_controller presentCredentials:@[ password_item ]];
  // Override the type for the test.
  password_item.type = kItemTypeSampleOne;

  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 1);
  EXPECT_EQ(GetTableViewItemType(/*section=*/0, /*item=*/0),
            kItemTypeSampleOne);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PasswordViewControllerTest);
