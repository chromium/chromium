// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/favicon_table_view_cell.h"
#import "ios/chrome/credential_provider_extension/favicon_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// A valid favicon file key (64 hex characters).
NSString* const kFaviconKey =
    @"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
NSString* const kMonogram = @"E";

// Returns the monogram currently displayed by `favicon_view`, if any.
NSString* DisplayedMonogram(FaviconView* favicon_view) {
  for (UIView* subview in favicon_view.subviews) {
    if ([subview isKindOfClass:[UILabel class]] && !subview.hidden) {
      return base::apple::ObjCCastStrict<UILabel>(subview).text;
    }
  }
  return nil;
}

// Returns a password credential for `service_name`, `username`, and `favicon`
// with placeholder values for the remaining fields.
ArchivableCredential* MakeCredential(NSString* service_name,
                                     NSString* username = @"user",
                                     NSString* favicon = nil) {
  return [[ArchivableCredential alloc] initWithFavicon:favicon
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
    ASSERT_TRUE(favicon_folder_.CreateUniqueTempDir());
    SetFaviconFolderForTesting(
        base::apple::FilePathToNSURL(favicon_folder_.GetPath()));
    controller_ = [[CredentialListViewController alloc] init];
    controller_.view.frame = CGRectMake(0, 0, 400, 800);
    first_row_ = [NSIndexPath indexPathForRow:0 inSection:0];
  }

  void TearDown() override {
    SetFaviconFolderForTesting(nil);
    PlatformTest::TearDown();
  }

 protected:
  // Writes a monogram favicon to the favicon folder without caching it.
  void WriteFavicon() {
    FaviconAttributes* attributes =
        [FaviconAttributes attributesWithMonogram:kMonogram
                                        textColor:UIColor.whiteColor
                                  backgroundColor:UIColor.blackColor
                           defaultBackgroundColor:NO];
    NSData* data = [NSKeyedArchiver archivedDataWithRootObject:attributes
                                         requiringSecureCoding:YES
                                                         error:nil];
    NSURL* file_url = [base::apple::FilePathToNSURL(favicon_folder_.GetPath())
        URLByAppendingPathComponent:kFaviconKey
                        isDirectory:NO];
    ASSERT_TRUE([data writeToURL:file_url atomically:YES]);
  }

  // Writes a monogram favicon to the favicon folder and loads it into the
  // in-memory favicon cache.
  void WriteAndCacheFavicon() {
    ASSERT_NO_FATAL_FAILURE(WriteFavicon());

    __block bool done = false;
    FetchFaviconAsync(kFaviconKey, ^(FaviconAttributes*) {
      done = true;
    });
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForFileOperationTimeout, ^{
          return done;
        }));
    ASSERT_TRUE(CachedFaviconAttributes(kFaviconKey));
  }

  base::ScopedTempDir favicon_folder_;
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

// Tests that a cached favicon is displayed as soon as the cell is configured,
// without first showing the default world icon.
TEST_F(CredentialListViewControllerTest, CachedFaviconIsShownSynchronously) {
  ASSERT_NO_FATAL_FAILURE(WriteAndCacheFavicon());

  [controller_
      presentSuggestedCredentials:@[]
                   allCredentials:@[ MakeCredential(@"example.com", @"user",
                                                    kFaviconKey) ]
                    showSearchBar:NO
            showNewPasswordOption:NO];

  UITableViewCell* cell = [controller_ tableView:controller_.tableView
                           cellForRowAtIndexPath:first_row_];
  FaviconTableViewCell* favicon_cell =
      base::apple::ObjCCastStrict<FaviconTableViewCell>(cell);

  EXPECT_NSEQ(DisplayedMonogram(favicon_cell.faviconView), kMonogram);
}

// Tests that a cell showing a cached favicon falls back to the default world
// icon when it is reused for a credential whose favicon isn't cached.
TEST_F(CredentialListViewControllerTest, ReusedCellDropsCachedFavicon) {
  ASSERT_NO_FATAL_FAILURE(WriteAndCacheFavicon());

  [controller_
      presentSuggestedCredentials:@[]
                   allCredentials:@[ MakeCredential(@"example.com", @"user",
                                                    kFaviconKey) ]
                    showSearchBar:NO
            showNewPasswordOption:NO];
  FaviconTableViewCell* cached_cell =
      base::apple::ObjCCastStrict<FaviconTableViewCell>(
          [controller_.tableView cellForRowAtIndexPath:first_row_]);
  ASSERT_NSEQ(DisplayedMonogram(cached_cell.faviconView), kMonogram);

  // A valid favicon file key (64 hex characters) not cached in memory.
  NSString* const kUncachedFaviconKey =
      @"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
  [controller_
      presentSuggestedCredentials:@[]
                   allCredentials:@[ MakeCredential(@"other.com", @"user",
                                                    kUncachedFaviconKey) ]
                    showSearchBar:NO
            showNewPasswordOption:NO];
  FaviconTableViewCell* reused_cell =
      base::apple::ObjCCastStrict<FaviconTableViewCell>(
          [controller_.tableView cellForRowAtIndexPath:first_row_]);
  ASSERT_EQ(reused_cell, cached_cell);

  EXPECT_FALSE(DisplayedMonogram(reused_cell.faviconView));
}

// Tests that an uncached favicon is loaded asynchronously from disk and
// applied to the cell.
TEST_F(CredentialListViewControllerTest, UncachedFaviconLoadsAsynchronously) {
  ASSERT_NO_FATAL_FAILURE(WriteFavicon());
  ASSERT_FALSE(CachedFaviconAttributes(kFaviconKey));

  [controller_
      presentSuggestedCredentials:@[]
                   allCredentials:@[ MakeCredential(@"example.com", @"user",
                                                    kFaviconKey) ]
                    showSearchBar:NO
            showNewPasswordOption:NO];

  FaviconTableViewCell* favicon_cell =
      base::apple::ObjCCastStrict<FaviconTableViewCell>(
          [controller_.tableView cellForRowAtIndexPath:first_row_]);
  ASSERT_TRUE(favicon_cell);
  EXPECT_FALSE(DisplayedMonogram(favicon_cell.faviconView));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^{
        return [DisplayedMonogram(favicon_cell.faviconView)
            isEqualToString:kMonogram];
      }));
}

}  // namespace
