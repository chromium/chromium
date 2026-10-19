// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_details_view_controller.h"

#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_test_util.h"
#import "ios/chrome/credential_provider_extension/generated_localized_strings.h"
#import "ios/chrome/credential_provider_extension/ui/credential_details_view_controller+Testing.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Checks that de text and detail text of the given `cell` are as expected.
void CheckCell(UITableViewCell* cell,
               NSString* expected_text,
               NSString* expected_detail_text) {
  EXPECT_NSEQ(cell.textLabel.text, expected_text);
  EXPECT_NSEQ(cell.detailTextLabel.text, expected_detail_text);
}

// Returns a formatted representation of a timestamp.
NSString* FormatTimestamp(int64_t timestamp) {
  return [NSDateFormatter
      localizedStringFromDate:[NSDate dateWithTimeIntervalSince1970:timestamp]
                    dateStyle:NSDateFormatterMediumStyle
                    timeStyle:NSDateFormatterNoStyle];
}

}  // namespace

class CredentialDetailsViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {}

  // Sets `controller_` if needed and returns it.
  CredentialDetailsViewController* controller() {
    if (controller_) {
      return controller_;
    }

    controller_ = [[CredentialDetailsViewController alloc] init];
    return controller_;
  }

  // Returns the table view cell found at the given `section` and `row`.
  UITableViewCell* GetTableViewCell(int section, int row) {
    NSIndexPath* index_path = [NSIndexPath indexPathForRow:row
                                                 inSection:section];
    UITableViewCell* cell =
        [controller().tableView.dataSource tableView:controller().tableView
                               cellForRowAtIndexPath:index_path];
    return cell;
  }

 private:
  CredentialDetailsViewController* controller_;
};

// Tests that a passkey is displayed properly.
TEST_F(CredentialDetailsViewControllerTest, TestPasskeyPresentation) {
  id<Credential> credential = TestPasskeyCredential();
  [controller() presentCredential:credential];

  // Check that the table view has the expected number of sections and rows.
  UITableView* table_view = controller().tableView;
  EXPECT_EQ([table_view numberOfSections], 1);
  EXPECT_EQ([table_view numberOfRowsInSection:0], 4);

  // Check that the content of every table view cell is as expected.
  CheckCell(GetTableViewCell(0, 0), CredentialProviderDetailsUrlString(),
            credential.serviceIdentifier);
  CheckCell(GetTableViewCell(0, 1), CredentialProviderDetailsUsernameString(),
            credential.username);
  CheckCell(GetTableViewCell(0, 2),
            CredentialProviderDetailsUserDisplayNameString(),
            credential.userDisplayName);

  CheckCell(
      GetTableViewCell(0, 3), CredentialProviderDetailsShowCreationDateString(),
      [CredentialProviderDetailsCreationDateString()
          stringByReplacingOccurrencesOfString:@"$1"
                                    withString:FormatTimestamp(
                                                   kDefaultLastUsedTime)]);
}

// Tests that converting the passkey creation time to a formatted date gives the
// expected result.
TEST_F(CredentialDetailsViewControllerTest,
       TestFormattedDateForPasskeyCreationTime) {
  id<Credential> credential = TestPasskeyCredential();
  EXPECT_NSEQ([controller()
                  formattedDateForPasskeyCreationDate:credential.creationDate],
              FormatTimestamp(kDefaultLastUsedTime));
}
