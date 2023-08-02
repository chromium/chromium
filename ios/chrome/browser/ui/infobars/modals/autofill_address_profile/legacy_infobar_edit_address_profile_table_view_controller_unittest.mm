// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/legacy_infobar_edit_address_profile_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/legacy_infobar_edit_address_profile_modal_delegate.h"
#import "testing/gtest_mac.h"

#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for testing LegacyInfobarEditAddressProfileTableViewController
// class.
class LegacyInfobarEditAddressProfileTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  LegacyInfobarEditAddressProfileTableViewControllerTest()
      : delegate_(OCMProtocolMock(
            @protocol(LegacyInfobarEditAddressProfileModalDelegate))) {}

  ChromeTableViewController* InstantiateController() override {
    return [[LegacyInfobarEditAddressProfileTableViewController alloc]
        initWithModalDelegate:delegate_];
  }

  id delegate_;
};

// Tests that the edit modal has been initialized.
TEST_F(LegacyInfobarEditAddressProfileTableViewControllerTest,
       TestEditModalInitialization) {
  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(10, NumberOfItemsInSection(0));
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}
