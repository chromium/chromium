// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_COLLECTION_VIEW_CONTROLLER_TEST_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_COLLECTION_VIEW_CONTROLLER_TEST_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"
#import "base/ios/block_types.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@class CollectionViewController;

class CollectionViewControllerTest : public BlockCleanupTest {
 public:
  CollectionViewControllerTest();
  ~CollectionViewControllerTest() override;

 protected:
  void TearDown() override;

  // Derived classes allocate their controller here.
  virtual CollectionViewController* InstantiateController() = 0;

  // Tests should call this function to create their controller for testing.
  void CreateController();

  // Will call CreateController() if |controller_| is nil.
  CollectionViewController* controller();

  // Deletes the controller.
  void ResetController();

  // Tests that the controller's view, model, collectionView, and delegate are
  // valid. Also tests that the model is the collectionView's data source.
  void CheckController();

  // Returns the number of sections in the collectionView.
  int NumberOfSections();

  // Returns the number of items in |section|.
  int NumberOfItemsInSection(int section);

  // Returns the collection view item at |item| in |section|.
  id GetCollectionViewItem(int section, int item);

  // Verifies that the title matches |expected_title|.
  void CheckTitle(NSString* expected_title);

  // Verifies that the title matches the l10n string for |expected_title_id|.
  void CheckTitleWithId(int expected_title_id);

  // Verifies that the section title at |section| matches the |expected_title|.
  void CheckSectionHeader(NSString* expected_title, int section);

  // Verifies that the section title at |section| matches the l10n string for
  // |expected_title_id|.
  void CheckSectionHeaderWithId(int expected_title_id, int section);

  // Verifies that the section footer at |section| matches the |expected_text|.
  // TODO(crbug.com/650424): Until the bug in MDC is fixed, footers are simple
  // items in a dedicated section.
  void CheckSectionFooter(NSString* expected_text, int section);

  // Verifies that the section footer at |section| matches the l10n string for
  // |expected_text_id|.
  // TODO(crbug.com/650424): Until the bug in MDC is fixed, footers are simple
  // items in a dedicated section.
  void CheckSectionFooterWithId(int expected_text_id, int section);

  // Verifies that the text cell at |item| in |section| has a text property
  // which matches |expected_title|.
  void CheckTextCellText(NSString* expected_text, int section, int item);

  // Verifies that the text cell at |item| in |section| has a text property
  // which matches the l10n string for |expected_title_id|.
  void CheckTextCellTextWithId(int expected_text_id, int section, int item);

  // Verifies that the text cell at |item| in |section| has a title which
  // matches |expected_title|.
  void CheckTextCellTitle(NSString* expected_title, int section, int item);

  // Verifies that the text cell at |item| in |section| has a title which
  // matches the l10n string for |expected_title_id|.
  void CheckTextCellTitleWithId(int expected_title_id, int section, int item);

  // Verifies that the text cell at |item| in |section| has a title and subtitle
  // which match strings for |expected_title| and |expected_subtitle|,
  // respectively.
  void CheckTextCellTitleAndSubtitle(NSString* expected_title,
                                     NSString* expected_subtitle,
                                     int section,
                                     int item);

  // Verifies that the text cell at |item| in |section| has a title and subtitle
  // which match strings for |expected_title| and |expected_subtitle|,
  // respectively.
  void CheckDetailItemTextWithIds(int expected_text_id,
                                  int expected_detail_text_id,
                                  int section_id,
                                  int item_id);

  // Verifies that the switch cell at |item| in |section| has a title which
  // matches |expected_title| and is currently in |state|.
  void CheckSwitchCellStateAndTitle(BOOL expected_state,
                                    NSString* expected_title,
                                    int section,
                                    int item);

  // Verifies that the switch cell at |item| in |section| has a title which
  // matches the l10n string for |expected_title_id| and is currently in
  // |state|.
  void CheckSwitchCellStateAndTitleWithId(BOOL expected_state,
                                          int expected_title_id,
                                          int section,
                                          int item);

  // Verifies that the cell at |item| in |section| has the given
  // |accessory_type|.
  void CheckAccessoryType(MDCCollectionViewCellAccessoryType accessory_type,
                          int section,
                          int item);

  // For |section|, deletes the item at |item|. |completion_block| is called at
  // the end of the call to -performBatchUpdates:completion:.
  void DeleteItem(int section, int item, ProceduralBlock completion_block);

 private:
  CollectionViewController* controller_;
};

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_COLLECTION_VIEW_CONTROLLER_TEST_H_
