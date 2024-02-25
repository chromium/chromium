// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_CONTROLLER_TEST_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_CONTROLLER_TEST_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/test/block_cleanup_test.h"

@class LegacyChromeTableViewController;

class LegacyChromeTableViewControllerTest : public BlockCleanupTest {
 public:
  LegacyChromeTableViewControllerTest();
  ~LegacyChromeTableViewControllerTest() override;

 protected:
  void TearDown() override;

  // Derived classes allocate their controller here.
  virtual LegacyChromeTableViewController* InstantiateController() = 0;

  // Tests should call this function to create their controller for testing.
  void CreateController();

  // Will call CreateController() if `controller_` is nil.
  LegacyChromeTableViewController* controller();

  // Deletes the controller.
  void ResetController();

  // Tests that the controller's view, model, tableView, and delegate are
  // valid. Also tests that the model is the tableView's data source.
  void CheckController();

  // Returns the number of sections in the tableView.
  int NumberOfSections();

  // Returns the number of items in `section`.
  int NumberOfItemsInSection(int section);

  // Indicates whether the collection view has an item at `item` in `section`.
  bool HasTableViewItem(int section, int item);

  // Returns the collection view item at `item` in `section`.
  id GetTableViewItem(int section, int item);

  // Verifies that the title matches `expected_title`.
  void CheckTitle(NSString* expected_title);

  // Verifies that the title matches the l10n string for `expected_title_id`.
  void CheckTitleWithId(int expected_title_id);

  // Verifies that the section header at `section` matches the `expected_text`.
  void CheckSectionHeader(NSString* expected_text, int section);

  // Verifies that the section header at `section` matches the l10n string for
  // `expected_text_id`.
  void CheckSectionHeaderWithId(int expected_text_id, int section);

  // Verifies that the section footer at `section` matches the `expected_text`.
  void CheckSectionFooter(NSString* expected_text, int section);

  // Verifies that the section footer at `section` matches the l10n string for
  // `expected_text_id`.
  void CheckSectionFooterWithId(int expected_text_id, int section);

  // Verifies that the text cell in section `section` at index `index` has a
  // `isEnabled` property that matches `expected_enabled`.
  void CheckTextCellEnabled(BOOL expected_enabled, int section, int item);

  // Verifies that the text cell at `item` in `section` has a text property
  // which matches `expected_text`.
  void CheckTextCellText(NSString* expected_text, int section, int item);

  // Verifies that the text cell at `item` in `section` has a text property
  // which matches the l10n string for `expected_text_id`.
  void CheckTextCellTextWithId(int expected_text_id, int section, int item);

  // Verifies that the text cell at `item` in `section` has a text and
  // detailText properties which match strings for `expected_text` and
  // `expected_detail_text`, respectively.
  void CheckTextCellTextAndDetailText(NSString* expected_text,
                                      NSString* expected_detail_text,
                                      int section,
                                      int item);

  // Verifies that the URL cell at `item` in `section` has a title and
  // detailText properties which match strings for `expected_title` and
  // `expected_detail_text`, respectively.
  void CheckURLCellTitleAndDetailText(NSString* expected_title,
                                      NSString* expected_detail_text,
                                      int section,
                                      int item);

  // Verifies that the URL cell at `item` in `section` has a title property
  // only which match strings for `expected_title`. It should not have a
  // detailsText.
  void CheckURLCellTitle(NSString* expected_title, int section, int item);

  // Verifies that the text cell at `item` in `section` has a text and
  // detailText properties which match strings for `expected_text` and
  // `expected_detail_text`, respectively.
  void CheckDetailItemTextWithIds(int expected_text_id,
                                  int expected_detail_text_id,
                                  int section_id,
                                  int item_id);

  // Verifies that the switch cell at `item` in `section` has a text property
  // which matches `expected_title` and a isOn method which matches
  // `expected_state`.
  void CheckSwitchCellStateAndText(BOOL expected_state,
                                   NSString* expected_title,
                                   int section,
                                   int item);

  // Verifies that the switch cell at `item` in `section` has a text property
  // which matches the l10n string for `expected_title_id` and a isOn method
  // which matches  `expected_state`.
  void CheckSwitchCellStateAndTextWithId(BOOL expected_state,
                                         int expected_title_id,
                                         int section,
                                         int item);

  // Verifies that the info button cell at `item` in `section` has a text
  // property which matches `expected_title` and a status text which matches
  // `expected_status text`.
  void CheckInfoButtonCellStatusAndText(NSString* expected_status_text,
                                        NSString* expected_title,
                                        int section,
                                        int item);

  // Verifies that the info button cell at `item` in `section` has a text
  // property which matches the l10n string for `expected_title_id` and a status
  // text which matches `expected_status text`.
  void CheckInfoButtonCellStatusWithIdAndTextWithId(int expected_status_text_id,
                                                    int expected_title_id,
                                                    int section,
                                                    int item);

  // Verifies that the cell at `item` in `section` has the given
  // `accessory_type`.
  void CheckAccessoryType(UITableViewCellAccessoryType accessory_type,
                          int section,
                          int item);

  // Verifies that the text button cell at `item` in `section` has the given
  // `buttonText`.
  void CheckTextButtonCellButtonText(NSString* expected_button_text,
                                     int section,
                                     int item);

  // Verifies that the text button cell at `item` in `section` has a
  // `buttonText` property which matches the l10n string for
  // `expected_button_text_id`.
  void CheckTextButtonCellButtonTextWithId(int expected_button_text_id,
                                           int section,
                                           int item);

  // For `section`, deletes the item at `item`. `completion_block` is called at
  // the end of the call to -performBatchUpdates:completion:.
  void DeleteItem(int section, int item, ProceduralBlock completion_block);

 private:
  LegacyChromeTableViewController* controller_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_CONTROLLER_TEST_H_
