// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Returns a password credential for `service_name` and `username` with
// placeholder values for the remaining fields.
ArchivableCredential* MakeCredential(NSString* service_name,
                                     NSString* username = @"user") {
  return [[ArchivableCredential alloc] initWithFavicon:nil
                                                  gaia:nil
                                              password:@"password"
                                                  rank:1
                                      recordIdentifier:@"record"
                                     serviceIdentifier:service_name
                                           serviceName:service_name
                              registryControlledDomain:service_name
                                              username:username
                                                  note:nil
                                          lastUsedTime:0];
}

class CredentialListViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    controller_ = [[CredentialListViewController alloc] init];
    controller_.view.frame = CGRectMake(0, 0, 400, 800);
    first_row_ = [NSIndexPath indexPathForRow:0 inSection:0];
  }

 protected:
  CredentialListViewController* controller_;
  NSIndexPath* first_row_;
};

// Tests that presenting credentials equal to the ones already displayed skips
// reloading the table view while still updating search bar visibility.
TEST_F(CredentialListViewControllerTest, SkipsReloadWhenDataIsUnchanged) {
  [controller_ presentSuggestedCredentials:@[ MakeCredential(@"suggested.com") ]
                            allCredentials:@[ MakeCredential(@"example.com") ]
                             showSearchBar:YES
                     showNewPasswordOption:NO];
  EXPECT_TRUE(controller_.navigationItem.searchController);
  UITableViewCell* cell =
      [controller_.tableView cellForRowAtIndexPath:first_row_];
  ASSERT_NSEQ(cell.textLabel.text, @"suggested.com");

  // Mutate the visible cell to detect whether `reloadData` reconfigures it.
  cell.textLabel.text = @"sentinel";

  // Present equal credentials via newly allocated array and credential objects.
  [controller_ presentSuggestedCredentials:@[ MakeCredential(@"suggested.com") ]
                            allCredentials:@[ MakeCredential(@"example.com") ]
                             showSearchBar:YES
                     showNewPasswordOption:NO];
  EXPECT_NSEQ(
      [controller_.tableView cellForRowAtIndexPath:first_row_].textLabel.text,
      @"sentinel");

  // Changing only `showSearchBar` still updates the navigation item without
  // reloading the table.
  [controller_ presentSuggestedCredentials:@[ MakeCredential(@"suggested.com") ]
                            allCredentials:@[ MakeCredential(@"example.com") ]
                             showSearchBar:NO
                     showNewPasswordOption:NO];
  EXPECT_FALSE(controller_.navigationItem.searchController);
  EXPECT_NSEQ(
      [controller_.tableView cellForRowAtIndexPath:first_row_].textLabel.text,
      @"sentinel");
}

// Tests that changing suggested credentials, all credentials, or
// `showNewPasswordOption` reloads the table view.
TEST_F(CredentialListViewControllerTest, ReloadsWhenDataOrOptionsChange) {
  NSIndexPath* all_first_row = [NSIndexPath indexPathForRow:0 inSection:1];
  [controller_
      presentSuggestedCredentials:@[ MakeCredential(@"suggested.com",
                                                    @"user1") ]
                   allCredentials:@[ MakeCredential(@"example.com", @"user1") ]
                    showSearchBar:YES
            showNewPasswordOption:NO];
  ASSERT_EQ([controller_.tableView numberOfSections], 2);

  // Change `suggestedCredentials` while keeping `allCredentials` identical.
  UITableViewCell* suggested_cell =
      [controller_.tableView cellForRowAtIndexPath:first_row_];
  ASSERT_NSEQ(suggested_cell.textLabel.text, @"suggested.com");
  suggested_cell.textLabel.text = @"sentinel";
  [controller_
      presentSuggestedCredentials:@[ MakeCredential(@"other.com", @"user1") ]
                   allCredentials:@[ MakeCredential(@"example.com", @"user1") ]
                    showSearchBar:YES
            showNewPasswordOption:NO];
  EXPECT_NSEQ(
      [controller_.tableView cellForRowAtIndexPath:first_row_].textLabel.text,
      @"other.com");

  // Change a field on `allCredentials` without changing row count or service
  // name.
  UITableViewCell* all_cell =
      [controller_.tableView cellForRowAtIndexPath:all_first_row];
  ASSERT_NSEQ(all_cell.textLabel.text, @"example.com");
  all_cell.textLabel.text = @"sentinel";
  [controller_
      presentSuggestedCredentials:@[ MakeCredential(@"other.com", @"user1") ]
                   allCredentials:@[ MakeCredential(@"example.com", @"user2") ]
                    showSearchBar:YES
            showNewPasswordOption:NO];
  EXPECT_NSEQ([controller_.tableView cellForRowAtIndexPath:all_first_row]
                  .textLabel.text,
              @"example.com");

  // Toggle `showNewPasswordOption` with identical credential arrays.
  ASSERT_EQ([controller_.tableView numberOfRowsInSection:0], 1);
  [controller_
      presentSuggestedCredentials:@[ MakeCredential(@"other.com", @"user1") ]
                   allCredentials:@[ MakeCredential(@"example.com", @"user2") ]
                    showSearchBar:YES
            showNewPasswordOption:YES];
  EXPECT_EQ([controller_.tableView numberOfRowsInSection:0], 2);

  // Switch to an empty list and verify the table reloads to 0 rows.
  [controller_ presentSuggestedCredentials:@[]
                            allCredentials:@[]
                             showSearchBar:NO
                     showNewPasswordOption:NO];
  EXPECT_EQ([controller_.tableView numberOfSections], 1);
  EXPECT_EQ([controller_.tableView numberOfRowsInSection:0], 0);
}

}  // namespace
