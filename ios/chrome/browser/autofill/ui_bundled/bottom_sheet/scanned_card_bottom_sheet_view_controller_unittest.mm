// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/scanned_card_bottom_sheet_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

class ScannedCardBottomSheetViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    // Initialize Mocks and Fakes.
    mock_data_source_ =
        OCMProtocolMock(@protocol(SaveCardBottomSheetDataSource));
    mock_delegate_ = OCMProtocolMock(@protocol(SaveCardBottomSheetDelegate));
    mock_mutator_ = OCMProtocolMock(@protocol(SaveCardBottomSheetMutator));

    // Instantiate the target ViewController.
    view_controller_ = [[ScannedCardBottomSheetViewController alloc] init];

    // Inject properties.
    view_controller_.dataSource = mock_data_source_;
    view_controller_.mutator = mock_mutator_;
    view_controller_.delegate = mock_delegate_;
  }

  void TearDown() override {
    [view_controller_ viewDidDisappear:NO];
    PlatformTest::TearDown();
  }

  // Helper method to extract the diffable data source snapshot.
  NSDiffableDataSourceSnapshot* GetSnapshot() {
    UITableViewDiffableDataSource* dataSource =
        base::apple::ObjCCastStrict<UITableViewDiffableDataSource>(
            view_controller_.tableView.dataSource);
    return [dataSource snapshot];
  }

  ScannedCardBottomSheetViewController* view_controller_;
  id mock_data_source_;
  id mock_delegate_;
  id mock_mutator_;
};

// Tests if the UI model is loaded correctly with the expected sections and
// cells.
TEST_F(ScannedCardBottomSheetViewControllerTest, TestLoadModel) {
  // Triggers viewDidLoad and loadModel.
  [view_controller_ loadViewIfNeeded];

  // Retrieve the snapshot representing the current UI state.
  NSDiffableDataSourceSnapshot* snapshot = GetSnapshot();

  // Verify the number of sections (Card Details and Nickname).
  EXPECT_EQ(2u, snapshot.sectionIdentifiers.count);

  // Card Details.
  NSArray* cardDetailsItems =
      [snapshot itemIdentifiersInSectionWithIdentifier:@(0)];
  EXPECT_EQ(4u, cardDetailsItems.count);

  // Nickname.
  NSArray* nicknameItems =
      [snapshot itemIdentifiersInSectionWithIdentifier:@(1)];
  EXPECT_EQ(1u, nicknameItems.count);
}

// Tests if the UI Model is updated correctly when data is passed from the
// Scanner Consumer.
TEST_F(ScannedCardBottomSheetViewControllerTest, TestScannerUpdatesUI) {
  [view_controller_ loadViewIfNeeded];

  // Simulate incoming scanned results.
  [view_controller_ setCreditCardNumber:@"4111222233334444"
                        expirationMonth:@"12"
                         expirationYear:@"26"];

  NSDiffableDataSourceSnapshot* snapshot = GetSnapshot();
  NSArray* cardDetailsItems =
      [snapshot itemIdentifiersInSectionWithIdentifier:@(0)];

  TableViewTextEditItem* numberItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[0]);
  // Card Number is Section 0 Row 0
  EXPECT_NSEQ(@"4111222233334444", numberItem.textFieldValue);

  TableViewTextEditItem* expItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[1]);
  // Expiration Date is Section 0 Row 1
  EXPECT_NSEQ(@"12/26", expItem.textFieldValue);
}

// Tests if the UI model correctly configures placeholder texts for the fields.
TEST_F(ScannedCardBottomSheetViewControllerTest, TestPlaceholders) {
  [view_controller_ loadViewIfNeeded];

  NSDiffableDataSourceSnapshot* snapshot = GetSnapshot();
  NSArray* cardDetailsItems =
      [snapshot itemIdentifiersInSectionWithIdentifier:@(0)];
  NSArray* nicknameItems =
      [snapshot itemIdentifiersInSectionWithIdentifier:@(1)];

  // Card Number.
  TableViewTextEditItem* numberItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[0]);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_AUTOFILL_SCAN_CARD_PLACEHOLDER_CARD_NUMBER),
              numberItem.textFieldPlaceholder);

  // Expiration Date.
  TableViewTextEditItem* expItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[1]);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_AUTOFILL_SCAN_CARD_PLACEHOLDER_EXPIRY_DATE),
              expItem.textFieldPlaceholder);

  // Cardholder Name.
  TableViewTextEditItem* nameItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[2]);
  EXPECT_NSEQ(nil, nameItem.textFieldPlaceholder);

  // CVC.
  TableViewTextEditItem* cvcItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[3]);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_CVC_OPTIONAL),
      cvcItem.textFieldPlaceholder);

  // Nickname.
  TableViewTextEditItem* nicknameItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(nicknameItems[0]);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_NICKNAME),
      nicknameItem.textFieldPlaceholder);
}

// Tests the behavior when the "Cancel" button is tapped.
TEST_F(ScannedCardBottomSheetViewControllerTest, TestDidTapCancel) {
  [view_controller_ loadViewIfNeeded];

  // Expect that cancel was called on the mutator.
  [[mock_mutator_ expect] didCancel];

  // Simulate tapping the cancel button.
  UIBarButtonItem* cancelItem =
      view_controller_.navigationItem.leftBarButtonItem;
  ASSERT_TRUE(cancelItem);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [cancelItem.target performSelector:cancelItem.action withObject:cancelItem];
#pragma clang diagnostic pop

  // Verify that expectations are met.
  [mock_mutator_ verify];
}
