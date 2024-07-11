// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::BookmarksDeleteSwipeButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextBarCenterButtonWithLabel;
using chrome_test_util::ContextBarLeadingButtonWithLabel;
using chrome_test_util::ContextBarTrailingButtonWithLabel;
using chrome_test_util::SearchIconButton;
using chrome_test_util::TappableBookmarkNodeWithLabel;

namespace {

// Returns an AppLaunchConfiguration containing the given policy data.
// `policyData` must be in XML format.
AppLaunchConfiguration GenerateAppLaunchConfiguration(std::string policy_data) {
  AppLaunchConfiguration config;
  // Remove whitespace from the policy data, because the XML parser does not
  // tolerate newlines.
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  // Commandline flags that start with a single "-" are automatically added to
  // the NSArgumentDomain in NSUserDefaults. Set fake policy data that can be
  // read by the production platform policy provider.
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policy_data);

  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

void VerifyBookmarkContextBarNewFolderButtonDisabled() {
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarkEarlGreyUI
                                              contextBarNewFolderString])]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

void VerifyBookmarkContextBarEditButtonDisabled() {
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarSelectString])]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

void LongPressBookmarkNodeWithLabel(NSString* bookmark_node_label) {
  id<GREYMatcher> nodeMatcher =
      TappableBookmarkNodeWithLabel(bookmark_node_label);
  [ChromeEarlGrey
      waitForMatcher:grey_allOf(nodeMatcher, grey_interactable(), nil)];
  [[EarlGrey selectElementWithMatcher:nodeMatcher]
      performAction:grey_longPress()];
}

void VerifyBookmarkContextMenuNil() {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeContextMenuIdentifier)]
      assertWithMatcher:grey_nil()];
}

void VerifyBookmarkNodeWithLabelNotNil(NSString* bookmark_node_label) {
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          bookmark_node_label)]
      assertWithMatcher:grey_notNil()];
}

void SearchBookmarksForText(NSString* search_text) {
  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(search_text)];
}

}  // namespace

// ManagedBookmarks test case with empty managed bookmarks. This can be
// sub-classed to provide non-empty managed bookmarks policy data.
@interface ManagedBookmarksTestCase : ChromeTestCase
@end

@implementation ManagedBookmarksTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  const std::string managedBookmarksData = [self managedBookmarksPolicyData];
  std::string policyData = "<dict>"
                           "<key>EnableExperimentalPolicies</key>"
                           "<array><string>" +
                           std::string(policy::key::kManagedBookmarks) +
                           "</string></array>"
                           "<key>" +
                           std::string(policy::key::kManagedBookmarks) +
                           "</key>" + managedBookmarksData + "</dict>";
  return GenerateAppLaunchConfiguration(policyData);
}

// Overridable by subclasses for custom managed bookmarks policy data.
- (std::string)managedBookmarksPolicyData {
  return "<array></array>";
}

- (void)setUp {
  [super setUp];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

@end

// Tests ManagedBookmarks when the policy data is empty.
@interface ManagedBookmarksEmptyPolicyDataTestCase : ManagedBookmarksTestCase
@end

@implementation ManagedBookmarksEmptyPolicyDataTestCase

- (std::string)managedBookmarksPolicyData {
  return "<array></array>";
}

// Tests that the managed bookmarks folder does not exist when the policy data
// is empty.
// Flaky. TODO(crbug.com/40721889): Re-enable.
- (void)DISABLED_testEmptyManagedBookmarks {
  [BookmarkEarlGreyUI openBookmarks];

  [BookmarkEarlGreyUI verifyEmptyState];

  // Managed bookmarks folder does not exist.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Managed Bookmarks")]
      assertWithMatcher:grey_nil()];
}

@end

// Tests ManagedBookmarks when the policy is set with no top-level folder name.
@interface ManagedBookmarksDefaultFolderTestCase : ManagedBookmarksTestCase
@end

@implementation ManagedBookmarksDefaultFolderTestCase

- (std::string)managedBookmarksPolicyData {
  // Note that this test removes all whitespace when setting the policy.
  return R"(
    <array>
      <dict>
        <key>url</key><string>firstURL.com</string>
        <key>name</key><string>First_Managed_URL</string>
      </dict>
    </array>
  )";
}

// Tests that the managed bookmarks folder exists with default name.
- (void)testDefaultFolderName {
  [BookmarkEarlGreyUI openBookmarks];

  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Managed Bookmarks")]
      performAction:grey_tap()];

  VerifyBookmarkNodeWithLabelNotNil(@"First_Managed_URL");
}

@end

// Tests ManagedBookmarks when the policy is set with an item in the top-level
// folder as well as an item in a sub-folder.
@interface ManagedBookmarksPolicyDataWithSubFolderTestCase
    : ManagedBookmarksTestCase
@end

@implementation ManagedBookmarksPolicyDataWithSubFolderTestCase

#pragma mark - Overrides

- (std::string)managedBookmarksPolicyData {
  // Note that this test removes all whitespace when setting the policy.
  return R"(
    <array>
      <dict>
        <key>toplevel_name</key><string>Custom_Folder_Name</string>
      </dict>
      <dict>
        <key>url</key><string>firstURL.com</string>
        <key>name</key><string>First_Managed_URL</string>
      </dict>
      <dict>
        <key>name</key><string>Managed_Sub_Folder</string>
        <key>children</key>
        <array>
          <dict>
            <key>url</key><string>subFolderFirstURL.org</string>
            <key>name</key><string>Sub_Folder_First_URL</string>
          </dict>
        </array>
      </dict>
    </array>
  )";
}

#pragma mark - Test Helpers

- (void)openCustomManagedBookmarksFolder {
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Custom_Folder_Name")]
      performAction:grey_tap()];
}

- (void)openCustomManagedSubFolder {
  id<GREYMatcher> subFolderMatcher =
      TappableBookmarkNodeWithLabel(@"Managed_Sub_Folder");
  [ChromeEarlGrey
      waitForMatcher:grey_allOf(subFolderMatcher, grey_interactable(), nil)];
  [[EarlGrey selectElementWithMatcher:subFolderMatcher]
      performAction:grey_tap()];
}

#pragma mark - Tests

// Tests that the managed bookmarks folder exists with custom name and
// contents.
- (void)testManagedBookmarksFolderStructure {
  [BookmarkEarlGreyUI openBookmarks];
  [self openCustomManagedBookmarksFolder];
  VerifyBookmarkNodeWithLabelNotNil(@"First_Managed_URL");

  [self openCustomManagedSubFolder];
  VerifyBookmarkNodeWithLabelNotNil(@"Sub_Folder_First_URL");
}

// Tests that 'New Folder' and 'Edit' buttons are disabled inside top-level
// managed bookmarks folder and sub-folder.
- (void)testContextBarButtonsDisabled {
  [BookmarkEarlGreyUI openBookmarks];
  [self openCustomManagedBookmarksFolder];

  VerifyBookmarkContextBarNewFolderButtonDisabled();
  VerifyBookmarkContextBarEditButtonDisabled();

  [self openCustomManagedSubFolder];

  VerifyBookmarkContextBarNewFolderButtonDisabled();
  VerifyBookmarkContextBarEditButtonDisabled();
}

// Tests that long press is disabled for the top-level managed bookmarks folder.
- (void)testLongPressDisabledForManagedFolders {
  [BookmarkEarlGreyUI openBookmarks];

  // Top-level managed folder cannot be long-pressed.
  LongPressBookmarkNodeWithLabel(@"Custom_Folder_Name");
  VerifyBookmarkContextMenuNil();
}

// Tests that the context menu for long-press on managed URLs disables the
// 'edit bookmark' option. For managed folders, 'edit folder' and 'move' are
// disabled.
// TODO(crbug.com/40684788): Long press unexpectedly triggers a tap (only in
// earl grey tests).
- (void)DISABLED_testContextMenuWithDisabledEditOption {
  [BookmarkEarlGreyUI openBookmarks];
  [self openCustomManagedBookmarksFolder];

  LongPressBookmarkNodeWithLabel(@"First_Managed_URL");
  [BookmarkEarlGreyUI verifyContextMenuForSingleURLWithEditEnabled:NO];
  [BookmarkEarlGreyUI dismissContextMenu];

  // Test long press on a folder.
  LongPressBookmarkNodeWithLabel(@"Managed_Sub_Folder");
  [BookmarkEarlGreyUI verifyContextMenuForSingleFolderWithEditEnabled:NO];
  [BookmarkEarlGreyUI dismissContextMenu];

  [self openCustomManagedSubFolder];

  // Test long press inside sub-folder.
  LongPressBookmarkNodeWithLabel(@"Sub_Folder_First_URL");
  [BookmarkEarlGreyUI verifyContextMenuForSingleURLWithEditEnabled:NO];
  [BookmarkEarlGreyUI dismissContextMenu];
}

// Tests that swipe is disabled in managed bookmarks top-level folder and
// sub-folder.
- (void)testSwipeDisabled {
  [BookmarkEarlGreyUI openBookmarks];
  [self openCustomManagedBookmarksFolder];

  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"First_Managed_URL")]
      assertWithMatcher:grey_not(
                            chrome_test_util::CellCanBeSwipedToDismissed())];

  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Managed_Sub_Folder")]
      assertWithMatcher:grey_not(
                            chrome_test_util::CellCanBeSwipedToDismissed())];

  [self openCustomManagedSubFolder];

  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Sub_Folder_First_URL")]
      assertWithMatcher:grey_not(
                            chrome_test_util::CellCanBeSwipedToDismissed())];
}

// Tests that swiping is disabled on managed bookmark items on search results.
- (void)testSwipeDisabledOnSearchResults {
  [BookmarkEarlGreyUI openBookmarks];
  SearchBookmarksForText(@"URL\n");

  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"First_Managed_URL")]
      assertWithMatcher:grey_not(
                            chrome_test_util::CellCanBeSwipedToDismissed())];
}

// Tests long presses on managed bookmark items in search results.
// TODO(crbug.com/40684788): Long press unexpectedly triggers a tap (only in
// earl grey tests).
- (void)DISABLED_testLongPressOnSearchResults {
  [BookmarkEarlGreyUI openBookmarks];
  SearchBookmarksForText(@"URL\n");

  LongPressBookmarkNodeWithLabel(@"First_Managed_URL");
  [BookmarkEarlGreyUI verifyContextMenuForSingleURLWithEditEnabled:NO];
  [BookmarkEarlGreyUI dismissContextMenu];
}

// Tests that edit is enabled on search results, but managed bookmarks cannot be
// selected for action.
- (void)testEditOnSearchResults {
  [BookmarkEarlGreyUI openBookmarks];
  SearchBookmarksForText(@"URL\n");

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"First_Managed_URL")]
      performAction:grey_tap()];

  // Delete disabled.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];

  // More button disabled.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
  // Cancel editing.
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      performAction:grey_tap()];
}

@end
