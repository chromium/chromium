// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_details_view_controller.h"

#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/credential_provider_extension/ui/credential_details_view_controller+Testing.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Time in seconds that represent January 1st 2024.
constexpr int64_t kJan1st2024 = 1704085200;

// Converts the given `seconds` to milliseconds.
int64_t SecondsToMilliseconds(int64_t seconds) {
  return seconds * 1000;
}

// Converts the given `seconds` to microseconds.
int64_t SecondsToMicroseconds(int64_t seconds) {
  return seconds * 1000000;
}

// Creates a data object from the given `str`.
NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

// Creates a test passkey.
ArchivableCredential* TestPasskeyCredential() {
  return [[ArchivableCredential alloc]
       initWithFavicon:@"favicon"
                  gaia:nil
      recordIdentifier:@"recordIdentifier"
                syncId:StringToData("syncId")
              username:@"username"
       userDisplayName:@"userDisplayName"
                userId:StringToData("userId")
          credentialId:StringToData("credentialId")
                  rpId:@"rpId"
            privateKey:StringToData("privateKey")
             encrypted:StringToData("encrypted")
          creationTime:SecondsToMilliseconds(kJan1st2024)
          lastUsedTime:SecondsToMicroseconds(kJan1st2024)
                hidden:NO
            hiddenTime:SecondsToMilliseconds(kJan1st2024)
          editedByUser:NO];
}

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
  CheckCell(GetTableViewCell(0, 0),
            NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_DETAILS_URL", @""),
            credential.serviceIdentifier);
  CheckCell(
      GetTableViewCell(0, 1),
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_DETAILS_USERNAME", @""),
      credential.username);
  CheckCell(GetTableViewCell(0, 2),
            NSLocalizedString(
                @"IDS_IOS_CREDENTIAL_PROVIDER_DETAILS_USER_DISPLAY_NAME", @""),
            credential.userDisplayName);

  CheckCell(
      GetTableViewCell(0, 3),
      NSLocalizedString(
          @"IDS_IOS_CREDENTIAL_PROVIDER_DETAILS_SHOW_CREATION_DATE", @""),
      [NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_DETAILS_CREATION_DATE",
                         @"")
          stringByReplacingOccurrencesOfString:@"$1"
                                    withString:FormatTimestamp(kJan1st2024)]);
}

// Tests that converting the passkey creation time to a formatted date gives the
// expected result.
TEST_F(CredentialDetailsViewControllerTest,
       TestFormattedDateForPasskeyCreationTime) {
  id<Credential> credential = TestPasskeyCredential();
  EXPECT_NSEQ([controller()
                  formattedDateForPasskeyCreationDate:credential.creationDate],
              FormatTimestamp(kJan1st2024));
}
