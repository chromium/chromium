// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_ai_base_item_type.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

class ShoppingTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    mock_delegate_ =
        OCMProtocolMock(@protocol(ShoppingTableViewControllerDelegate));
    mock_mutator_ = OCMProtocolMock(@protocol(ShoppingMutator));
    CreateController();
  }

  void TearDown() override {
    mock_delegate_ = nil;
    mock_mutator_ = nil;
    LegacyChromeTableViewControllerTest::TearDown();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    ShoppingTableViewController* controller =
        [[ShoppingTableViewController alloc] init];
    controller.delegate = mock_delegate_;
    controller.mutator = mock_mutator_;
    [controller setShouldShowSuggestionsFromGemini:YES enabled:YES];
    return controller;
  }

  id mock_delegate_;
  id mock_mutator_;
};

// Tests that the Shopping page displays its title and the toggle, Suggestions
// from Gemini, order, and shipment sections with their headers and footers when
// entities are present.
TEST_F(ShoppingTableViewControllerTest, TestSectionsAndVisualComposition) {
  CheckController();
  CheckTitleWithId(IDS_AUTOFILL_SHOPPING_TITLE);

  ShoppingTableViewController* view_controller =
      base::apple::ObjCCastStrict<ShoppingTableViewController>(controller());

  TableViewItem* order =
      [[TableViewItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];
  TableViewItem* shipment =
      [[TableViewItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];

  [view_controller setShoppingWithOrders:@[ order ] shipments:@[ shipment ]];
  [view_controller setShoppingToggleState:YES enabled:YES managed:NO];

  EXPECT_EQ(4, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  TableViewTextHeaderFooterItem* footer =
      base::apple::ObjCCastStrict<TableViewTextHeaderFooterItem>(
          [[view_controller tableViewModel] footerForSectionIndex:0]);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_AUTOFILL_SHOPPING_OPT_IN_TOGGLE_SUB_LABEL),
      footer.subtitle);

  EXPECT_EQ(1, NumberOfItemsInSection(1));
  CheckSectionFooterWithId(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_SUBPAGE_SUMMARY, 1);
  TableViewMultiDetailTextItem* gemini_item =
      base::apple::ObjCCastStrict<TableViewMultiDetailTextItem>(
          GetTableViewItem(1, 0));
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_TITLE),
      gemini_item.text);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              gemini_item.trailingDetailText);

  EXPECT_EQ(1, NumberOfItemsInSection(2));
  CheckSectionHeaderWithId(IDS_AUTOFILL_AI_ORDERS_TITLE, 2);

  EXPECT_EQ(1, NumberOfItemsInSection(3));
  CheckSectionHeaderWithId(IDS_AUTOFILL_AI_SHIPMENTS_TITLE, 3);
}

// Tests that tapping the main toggle notifies the mutator via
// `didToggleShopping:`.
TEST_F(ShoppingTableViewControllerTest, TestToggleInteraction) {
  ShoppingTableViewController* view_controller =
      base::apple::ObjCCastStrict<ShoppingTableViewController>(controller());

  [view_controller setShoppingToggleState:YES enabled:YES managed:NO];

  // Verify initial state.
  CheckSwitchCellStateAndTextWithId(
      YES, IDS_AUTOFILL_SHOPPING_OPT_IN_TOGGLE_LABEL, 0, 0);

  // Retrieve the cell and switch.
  TableViewCellContentView* content_view =
      base::apple::ObjCCastStrict<TableViewCellContentView>([[view_controller
                      tableView:view_controller.tableView
          cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0
                                                   inSection:0]] contentView]);
  ASSERT_TRUE(content_view);
  SwitchContentView* switch_content_view =
      base::apple::ObjCCastStrict<SwitchContentView>(
          [content_view trailingContentViewForTesting]);
  ASSERT_TRUE(switch_content_view);
  UISwitch* switch_view = [switch_content_view switchForTesting];
  ASSERT_TRUE(switch_view);

  // Simulate toggle tap off.
  OCMExpect([mock_mutator_ didToggleShopping:NO]);
  switch_view.on = NO;
  [switch_view sendActionsForControlEvents:UIControlEventValueChanged];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);

  // Simulate toggle tap on.
  OCMExpect([mock_mutator_ didToggleShopping:YES]);
  switch_view.on = YES;
  [switch_view sendActionsForControlEvents:UIControlEventValueChanged];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that a managed toggle state renders a `TableViewInfoButtonItem`
// instead of a `TableViewSwitchItem`.
TEST_F(ShoppingTableViewControllerTest, TestManagedState) {
  ShoppingTableViewController* view_controller =
      base::apple::ObjCCastStrict<ShoppingTableViewController>(controller());

  [view_controller setShoppingToggleState:NO enabled:NO managed:YES];

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));

  id item = GetTableViewItem(0, 0);
  EXPECT_TRUE([item isKindOfClass:[TableViewInfoButtonItem class]]);
}

// Tests that an unmanaged toggle state renders a `TableViewSwitchItem`.
TEST_F(ShoppingTableViewControllerTest, TestUnmanagedState) {
  ShoppingTableViewController* view_controller =
      base::apple::ObjCCastStrict<ShoppingTableViewController>(controller());

  [view_controller setShoppingToggleState:YES enabled:YES managed:NO];

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));

  id item = GetTableViewItem(0, 0);
  EXPECT_TRUE([item isKindOfClass:[TableViewSwitchItem class]]);
}

// Tests that selecting an entity row notifies the mutator via
// `didSelectEntityItem:`.
TEST_F(ShoppingTableViewControllerTest, TestRowSelectionInteraction) {
  ShoppingTableViewController* view_controller =
      base::apple::ObjCCastStrict<ShoppingTableViewController>(controller());

  TableViewItem* order =
      [[TableViewItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];

  [view_controller setShoppingWithOrders:@[ order ] shipments:@[]];

  OCMExpect([mock_mutator_ didSelectEntityItem:order]);

  // Simulate row selection. Section 2 is Orders, Row 0.
  NSIndexPath* index_path = [NSIndexPath indexPathForRow:0 inSection:2];
  [view_controller tableView:view_controller.tableView
      didSelectRowAtIndexPath:index_path];

  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that when the consumer provides empty arrays for entities, only the
// toggle and Suggestions from Gemini sections are displayed and no entity
// sections are rendered.
TEST_F(ShoppingTableViewControllerTest, TestEmptyStates) {
  ShoppingTableViewController* view_controller =
      base::apple::ObjCCastStrict<ShoppingTableViewController>(controller());

  [view_controller setShoppingWithOrders:@[] shipments:@[]];

  // Only the toggle and Suggestions from Gemini sections should be present.
  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Tests that moving away from the view controller notifies the delegate via
// `shoppingTableViewControllerDidRemove:`.
TEST_F(ShoppingTableViewControllerTest, TestDelegateOnRemove) {
  ShoppingTableViewController* view_controller =
      base::apple::ObjCCastStrict<ShoppingTableViewController>(controller());

  OCMExpect(
      [mock_delegate_ shoppingTableViewControllerDidRemove:view_controller]);
  [view_controller didMoveToParentViewController:nil];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that updating the Suggestions from Gemini enabled state updates the
// item's trailing detail text.
TEST_F(ShoppingTableViewControllerTest, TestSuggestionsFromGeminiStateUpdate) {
  ShoppingTableViewController* view_controller =
      base::apple::ObjCCastStrict<ShoppingTableViewController>(controller());

  TableViewMultiDetailTextItem* gemini_item =
      base::apple::ObjCCastStrict<TableViewMultiDetailTextItem>(
          GetTableViewItem(1, 0));
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              gemini_item.trailingDetailText);

  [view_controller setShouldShowSuggestionsFromGemini:YES enabled:NO];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              gemini_item.trailingDetailText);
}

// Tests that hiding Suggestions from Gemini removes its section from the table
// view.
TEST_F(ShoppingTableViewControllerTest,
       TestSuggestionsFromGeminiSectionRemoved) {
  ShoppingTableViewController* view_controller =
      base::apple::ObjCCastStrict<ShoppingTableViewController>(controller());

  EXPECT_EQ(2, NumberOfSections());

  [view_controller setShouldShowSuggestionsFromGemini:NO enabled:NO];
  EXPECT_EQ(1, NumberOfSections());
}

// Tests that selecting the Suggestions from Gemini row notifies the delegate
// via `shoppingTableViewControllerDidSelectSuggestionsFromGemini:`.
TEST_F(ShoppingTableViewControllerTest, TestSuggestionsFromGeminiRowSelection) {
  ShoppingTableViewController* view_controller =
      base::apple::ObjCCastStrict<ShoppingTableViewController>(controller());

  OCMExpect(
      [mock_delegate_ shoppingTableViewControllerDidSelectSuggestionsFromGemini:
                          view_controller]);
  OCMReject([mock_mutator_ didSelectEntityItem:OCMOCK_ANY]);

  NSIndexPath* index_path = [NSIndexPath indexPathForRow:0 inSection:1];
  [view_controller tableView:view_controller.tableView
      didSelectRowAtIndexPath:index_path];

  EXPECT_OCMOCK_VERIFY(mock_delegate_);
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}
