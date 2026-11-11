// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr char16_t kTestDomain[] = u"example.com";
constexpr char16_t kBeginLinkTag[] = u"BEGIN_LINK";
constexpr char16_t kEndLinkTag[] = u"END_LINK";

// Returns the expected subtitle, without the link tags.
NSString* ExpectedSubtitle() {
  return ParseStringWithLinks(
             l10n_util::GetNSStringF(IDS_REMOTE_ACTOR_SHARING_DIALOG_SUBTITLE,
                                     kBeginLinkTag, kEndLinkTag, u"Gemini"))
      .string;
}

// Returns the expected title for `domain`.
NSString* ExpectedTitle(const char16_t* domain) {
  return l10n_util::GetNSStringF(IDS_REMOTE_ACTOR_SHARING_DIALOG_TITLE,
                                 u"Gemini", domain);
}

// Returns a password suggestion for `username`.
FormSuggestion* PasswordSuggestion(NSString* username) {
  return [FormSuggestion
      suggestionWithValue:username
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kPasswordEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:YES];
}

}  // namespace

// Tests for `RemoteActorCredentialBottomSheetViewController`.
class RemoteActorCredentialBottomSheetViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    mutator_ = OCMStrictProtocolMock(
        @protocol(RemoteActorCredentialBottomSheetMutator));
    view_controller_ =
        [[RemoteActorCredentialBottomSheetViewController alloc] init];
    view_controller_.mutator = mutator_;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mutator_);
    PlatformTest::TearDown();
  }

  // Sets the test domain and `suggestions`, then loads the view.
  void LoadViewWithSuggestions(NSArray<FormSuggestion*>* suggestions) {
    [view_controller_ setDomain:base::SysUTF16ToNSString(kTestDomain)];
    [view_controller_ setSuggestions:suggestions];
    [view_controller_ loadViewIfNeeded];
  }

  // Returns the table view displaying the credentials.
  UITableView* TableView() {
    return chrome_test_util::FindViewByClass<UITableView>(
        view_controller_.view);
  }

  // Returns the cell at `row` from the table view's data source.
  UITableViewCell* CellAtRow(NSInteger row) {
    UITableView* table_view = TableView();
    return [table_view.dataSource tableView:table_view
                      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row
                                                               inSection:0]];
  }

  // Returns the content configuration of the cell at `row`.
  TableViewCellContentConfiguration* ConfigurationAtRow(NSInteger row) {
    return base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
        CellAtRow(row).contentConfiguration);
  }

  id mutator_;
  RemoteActorCredentialBottomSheetViewController* view_controller_;
};

// Tests that the buttons display the allow and cancel titles.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest, TestButtonTitles) {
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_REMOTE_ACTOR_SHARING_DIALOG_ALLOW_BUTTON),
      view_controller_.configuration.primaryActionString);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL),
              view_controller_.configuration.secondaryActionString);
}

// Tests that the secondary action notifies the mutator of the cancellation.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TestSecondaryActionCancels) {
  OCMExpect([mutator_ didCancel]);
  [view_controller_.actionHandler confirmationAlertSecondaryAction];
}

// Tests that dismissing the bottom sheet notifies the mutator of the
// cancellation.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TestDismissalCancels) {
  OCMExpect([mutator_ didCancel]);
  [view_controller_.actionHandler confirmationAlertDismissed];
}

// Tests that the view controller configures the subtitle without link tags and
// the button strings once the view is loaded.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       ViewDidLoadSetsSubtitleAndButtons) {
  [view_controller_ loadViewIfNeeded];

  EXPECT_NSEQ(ExpectedSubtitle(), view_controller_.subtitleString);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_REMOTE_ACTOR_SHARING_DIALOG_ALLOW_BUTTON),
      view_controller_.configuration.primaryActionString);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL),
              view_controller_.configuration.secondaryActionString);
}

// Tests that setting the domain before the view loads configures the title
// with the domain.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest, SetDomainSetsTitle) {
  [view_controller_ setDomain:base::SysUTF16ToNSString(kTestDomain)];
  [view_controller_ loadViewIfNeeded];

  EXPECT_NSEQ(ExpectedTitle(kTestDomain), view_controller_.titleString);
  EXPECT_NSEQ(ExpectedTitle(kTestDomain), view_controller_.titleLabel.text);
  EXPECT_FALSE(view_controller_.titleLabel.hidden);
}

// Tests that the table view is updated when the suggestions arrive after the
// view has loaded.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       SetSuggestionsAfterViewLoadedUpdatesTable) {
  [view_controller_ setDomain:base::SysUTF16ToNSString(kTestDomain)];
  [view_controller_ loadViewIfNeeded];
  UITableView* table_view = TableView();
  ASSERT_TRUE(table_view);
  EXPECT_EQ(0, [table_view numberOfRowsInSection:0]);

  [view_controller_ setSuggestions:@[
    PasswordSuggestion(@"user1"), PasswordSuggestion(@"user2")
  ]];

  EXPECT_EQ(2, [table_view numberOfRowsInSection:0]);
}

// Tests that the primary button is disabled until suggestions are set.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       PrimaryButtonEnabledOnceSuggestionsAreSet) {
  [view_controller_ loadViewIfNeeded];
  EXPECT_FALSE(view_controller_.configuration.primaryActionEnabled);

  [view_controller_ setSuggestions:@[ PasswordSuggestion(@"user1") ]];
  EXPECT_TRUE(view_controller_.configuration.primaryActionEnabled);
}

// Tests that the primary button stays disabled when there are no suggestions.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       PrimaryButtonDisabledWithoutSuggestions) {
  [view_controller_ setSuggestions:@[]];
  [view_controller_ loadViewIfNeeded];
  EXPECT_FALSE(view_controller_.configuration.primaryActionEnabled);
}

// Tests that the table view displays one row per suggestion, with the username
// as title and "Password" as subtitle.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TableViewDisplaysSuggestions) {
  LoadViewWithSuggestions(
      @[ PasswordSuggestion(@"user1"), PasswordSuggestion(@"user2") ]);

  UITableView* table_view = TableView();
  ASSERT_TRUE(table_view);
  EXPECT_EQ(2, [table_view.dataSource tableView:table_view
                          numberOfRowsInSection:0]);

  TableViewCellContentConfiguration* first = ConfigurationAtRow(0);
  EXPECT_NSEQ(@"user1", first.title);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSWORD_SUBTEXT), first.subtitle);
  EXPECT_NSEQ(@"user2", ConfigurationAtRow(1).title);
}

// Tests that a suggestion without a username displays a placeholder.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       EmptyUsernameDisplaysPlaceholder) {
  LoadViewWithSuggestions(@[ PasswordSuggestion(@"") ]);

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_BOTTOM_SHEET_NO_USERNAME),
      ConfigurationAtRow(0).title);
}

// Tests that the first row has a checkmark when the suggestions arrive after
// the view has loaded.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       SelectedRowHasCheckmark) {
  [view_controller_ setDomain:base::SysUTF16ToNSString(kTestDomain)];
  [view_controller_ loadViewIfNeeded];
  [view_controller_ setSuggestions:@[
    PasswordSuggestion(@"user1"), PasswordSuggestion(@"user2")
  ]];

  EXPECT_EQ(UITableViewCellAccessoryCheckmark, CellAtRow(0).accessoryType);
  EXPECT_EQ(UITableViewCellAccessoryNone, CellAtRow(1).accessoryType);
}

// Tests that the primary action shares the credential of the selected row.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       PrimaryActionSharesSelectedRow) {
  LoadViewWithSuggestions(
      @[ PasswordSuggestion(@"user1"), PasswordSuggestion(@"user2") ]);
  UITableView* table_view = TableView();
  ASSERT_TRUE(table_view);

  NSIndexPath* selected_index_path = [NSIndexPath indexPathForRow:1
                                                        inSection:0];
  [table_view selectRowAtIndexPath:selected_index_path
                          animated:NO
                    scrollPosition:UITableViewScrollPositionNone];
  [table_view.delegate tableView:table_view
         didSelectRowAtIndexPath:selected_index_path];

  OCMExpect([mutator_ didSelectCredentialAtIndex:1]);
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
}

// Tests that the cells display the favicon once it is set.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest, CellsShowFavicon) {
  LoadViewWithSuggestions(@[ PasswordSuggestion(@"user1") ]);
  EXPECT_FALSE(ConfigurationAtRow(0).leadingConfiguration);

  FaviconAttributes* attributes =
      [FaviconAttributes attributesWithImage:[[UIImage alloc] init]];
  [view_controller_ setFaviconAttributes:attributes];

  FaviconContentConfiguration* favicon_configuration =
      base::apple::ObjCCast<FaviconContentConfiguration>(
          ConfigurationAtRow(0).leadingConfiguration);
  ASSERT_TRUE(favicon_configuration);
  EXPECT_NSEQ(attributes, favicon_configuration.faviconAttributes);
}
