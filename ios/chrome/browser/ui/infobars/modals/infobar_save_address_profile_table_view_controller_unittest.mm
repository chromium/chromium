// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_save_address_profile_table_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_address_profile_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "testing/gtest_mac.h"

#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing InfobarSaveAddressProfileTableViewController class.
class InfobarSaveAddressProfileTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  InfobarSaveAddressProfileTableViewControllerTest()
      : modal_delegate_(OCMProtocolMock(
            @protocol(InfobarSaveAddressProfileModalDelegate))) {}

  ChromeTableViewController* InstantiateController() override {
    return [[InfobarSaveAddressProfileTableViewController alloc]
        initWithModalDelegate:modal_delegate_];
  }

  NSDictionary* GetDataForUpdateModal() {
    NSDictionary* prefs = @{
      kAddressPrefKey : @"",
      kPhonePrefKey : @"",
      kEmailPrefKey : @"",
      kCurrentAddressProfileSavedPrefKey : @(false),
      kIsUpdateModalPrefKey : @(true),
      kProfileDataDiffKey : @{
        [NSNumber numberWithInt:AutofillUITypeNameFullWithHonorificPrefix] :
            @[ @"John Doe", @"John H. Doe" ]
      },
      kUpdateModalDescriptionKey : @"For John Doe, 345 Spear Street"
    };
    return prefs;
  }

  id modal_delegate_;
};

// Tests that the update modal has been initialized.
TEST_F(InfobarSaveAddressProfileTableViewControllerTest,
       TestUpdateModalInitialization) {
  CreateController();
  CheckController();
  InfobarSaveAddressProfileTableViewController* update_view_controller =
      base::mac::ObjCCastStrict<InfobarSaveAddressProfileTableViewController>(
          controller());
  [update_view_controller
      setupModalViewControllerWithPrefs:GetDataForUpdateModal()];
  [update_view_controller loadModel];

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(6, NumberOfItemsInSection(0));
}
