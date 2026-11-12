// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_config.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_mutator.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

constexpr char16_t kTestDomain[] = u"example.com";
constexpr char16_t kBeginLinkTag[] = u"BEGIN_LINK";
constexpr char16_t kEndLinkTag[] = u"END_LINK";

// Returns the learn more URL of the Gemini remote actor.
GURL ExpectedLearnMoreURL() {
  return password_manager::GetRemoteActorConfig(
             password_manager::RemoteActorType::kGemini)
      .learn_more_url;
}

// Returns the expected subtitle parsed with its link range.
StringWithTags ExpectedSubtitleWithTags() {
  return ParseStringWithLinks(l10n_util::GetNSStringF(
      IDS_REMOTE_ACTOR_SHARING_DIALOG_SUBTITLE, kBeginLinkTag, kEndLinkTag,
      l10n_util::GetStringUTF16(IDS_REMOTE_ACTOR_GEMINI_NAME)));
}

// Returns the expected subtitle, without the link tags.
NSString* ExpectedSubtitle() {
  return ExpectedSubtitleWithTags().string;
}

// Returns the expected range of the link in the subtitle.
NSRange ExpectedSubtitleLinkRange() {
  return ExpectedSubtitleWithTags().ranges[0];
}

// Returns the subtitle text view in `view`'s hierarchy, or nil if not found.
UITextView* SubtitleTextView(UIView* view) {
  return chrome_test_util::FindViewById<UITextView>(
      view, kConfirmationAlertSubtitleAccessibilityIdentifier);
}

// Returns the expected title for `domain`.
NSString* ExpectedTitle(const char16_t* domain) {
  return l10n_util::GetNSStringF(
      IDS_REMOTE_ACTOR_SHARING_DIALOG_TITLE,
      l10n_util::GetStringUTF16(IDS_REMOTE_ACTOR_GEMINI_NAME), domain);
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
    scene_handler_ = OCMStrictProtocolMock(@protocol(SceneCommands));
    view_controller_ =
        [[RemoteActorCredentialBottomSheetViewController alloc] init];
    view_controller_.mutator = mutator_;
    view_controller_.sceneHandler = scene_handler_;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mutator_);
    EXPECT_OCMOCK_VERIFY(scene_handler_);
    PlatformTest::TearDown();
  }

  // Loads the view and returns the URL attached to the subtitle link. Expects
  // the URL to span exactly the link range.
  NSURL* SubtitleLinkURL() {
    [view_controller_ loadViewIfNeeded];
    UITextView* subtitle = SubtitleTextView(view_controller_.view);
    EXPECT_TRUE(subtitle);
    NSRange link_range = ExpectedSubtitleLinkRange();
    NSRange effective_range;
    NSURL* link = [subtitle.attributedText attribute:NSLinkAttributeName
                                             atIndex:link_range.location
                                      effectiveRange:&effective_range];
    EXPECT_TRUE(NSEqualRanges(link_range, effective_range));
    return link;
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

  // Expects the loaded view to show the no saved passwords state for the test
  // domain.
  void ExpectNoSavedPasswordsUI() {
    UIView* view = view_controller_.view;
    UILabel* title = chrome_test_util::FindViewById<UILabel>(
        view, kConfirmationAlertTitleAccessibilityIdentifier);
    EXPECT_NSEQ(l10n_util::GetNSStringF(
                    IDS_IOS_REMOTE_ACTOR_NO_PASSWORD_DIALOG_TITLE, kTestDomain),
                title.text);

    UITextView* subtitle = SubtitleTextView(view);
    ASSERT_TRUE(subtitle);
    EXPECT_NSEQ(l10n_util::GetNSStringF(
                    IDS_IOS_REMOTE_ACTOR_NO_PASSWORD_DIALOG_SUBTITLE,
                    l10n_util::GetStringUTF16(IDS_REMOTE_ACTOR_GEMINI_NAME)),
                subtitle.text);
    __block bool has_link_styling = false;
    [subtitle.attributedText
        enumerateAttributesInRange:NSMakeRange(0,
                                               subtitle.attributedText.length)
                           options:0
                        usingBlock:^(
                            NSDictionary<NSAttributedStringKey, id>* attributes,
                            NSRange range, BOOL* stop) {
                          has_link_styling |=
                              attributes[NSLinkAttributeName] != nil ||
                              attributes[NSUnderlineStyleAttributeName] != nil;
                        }];
    EXPECT_FALSE(has_link_styling);

    ChromeButton* primary_button = chrome_test_util::FindViewById<ChromeButton>(
        view, kButtonStackPrimaryActionAccessibilityIdentifier);
    ChromeButton* secondary_button =
        chrome_test_util::FindViewById<ChromeButton>(
            view, kButtonStackSecondaryActionAccessibilityIdentifier);
    ASSERT_TRUE(primary_button);
    EXPECT_NSEQ(l10n_util::GetNSStringF(
                    IDS_IOS_REMOTE_ACTOR_NO_PASSWORD_DIALOG_BACK_BUTTON,
                    l10n_util::GetStringUTF16(IDS_REMOTE_ACTOR_GEMINI_NAME)),
                view_controller_.configuration.primaryActionString);
    EXPECT_TRUE(primary_button.enabled);
    EXPECT_EQ(ChromeButtonStylePrimary, primary_button.style);
    EXPECT_TRUE(!secondary_button || secondary_button.hidden);
  }

  id mutator_;
  id scene_handler_;
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

// Tests that the bottom sheet can still be dismissed interactively during the
// loading state.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TestLoadingAllowsInteractiveDismissal) {
  LoadViewWithSuggestions(@[ PasswordSuggestion(@"user") ]);
  EXPECT_FALSE(view_controller_.modalInPresentation);

  [view_controller_ setLoading:YES];
  EXPECT_TRUE(view_controller_.configuration.loading);
  EXPECT_FALSE(view_controller_.modalInPresentation);

  [view_controller_ setLoading:NO];
  EXPECT_FALSE(view_controller_.modalInPresentation);
}

// Tests that the loading state disables the primary button and switches it to
// the primary style with an accessibility label while keeping the cancel button
// enabled in its tertiary style, and that leaving it restores the initial
// state.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TestLoadingUpdatesButtons) {
  LoadViewWithSuggestions(@[ PasswordSuggestion(@"user") ]);
  ChromeButton* primary_button = chrome_test_util::FindViewById<ChromeButton>(
      view_controller_.view, kButtonStackPrimaryActionAccessibilityIdentifier);
  ChromeButton* secondary_button = chrome_test_util::FindViewById<ChromeButton>(
      view_controller_.view,
      kButtonStackSecondaryActionAccessibilityIdentifier);
  ASSERT_TRUE(primary_button);
  ASSERT_TRUE(secondary_button);
  EXPECT_TRUE(primary_button.enabled);
  EXPECT_TRUE(secondary_button.enabled);
  EXPECT_EQ(ChromeButtonStyleTertiary, primary_button.style);
  EXPECT_EQ(ChromeButtonStyleTertiary, secondary_button.style);

  NSString* loading_accessibility_label =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_STATUS_PROGRESS_TITLE);

  [view_controller_ setLoading:YES];
  [secondary_button updateConfiguration];
  EXPECT_FALSE(primary_button.enabled);
  EXPECT_TRUE(secondary_button.enabled);
  EXPECT_EQ(ChromeButtonStylePrimary, primary_button.style);
  EXPECT_EQ(ChromeButtonStyleTertiary, secondary_button.style);
  EXPECT_EQ(1.0, secondary_button.alpha);
  EXPECT_EQ(PrimaryButtonImageSpinner, primary_button.primaryButtonImage);
  EXPECT_NSEQ(loading_accessibility_label, primary_button.accessibilityLabel);

  [view_controller_ setLoading:NO];
  EXPECT_TRUE(primary_button.enabled);
  EXPECT_TRUE(secondary_button.enabled);
  EXPECT_EQ(ChromeButtonStyleTertiary, primary_button.style);
  EXPECT_EQ(ChromeButtonStyleTertiary, secondary_button.style);
  EXPECT_FALSE([primary_button.accessibilityLabel
      isEqualToString:loading_accessibility_label]);
}

// Tests that the confirmation state replaces the loading state's spinner with a
// checkmark in the primary style with an accessibility label, and disables
// both buttons.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TestConfirmedShowsCheckmark) {
  LoadViewWithSuggestions(@[ PasswordSuggestion(@"user") ]);
  ChromeButton* primary_button = chrome_test_util::FindViewById<ChromeButton>(
      view_controller_.view, kButtonStackPrimaryActionAccessibilityIdentifier);
  ChromeButton* secondary_button = chrome_test_util::FindViewById<ChromeButton>(
      view_controller_.view,
      kButtonStackSecondaryActionAccessibilityIdentifier);
  ASSERT_TRUE(primary_button);
  ASSERT_TRUE(secondary_button);

  [view_controller_ setLoading:YES];
  EXPECT_TRUE(view_controller_.configuration.loading);
  EXPECT_EQ(PrimaryButtonImageSpinner, primary_button.primaryButtonImage);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_STATUS_PROGRESS_TITLE),
      primary_button.accessibilityLabel);

  [view_controller_ showConfirmationState];
  EXPECT_FALSE(view_controller_.configuration.loading);
  EXPECT_TRUE(view_controller_.configuration.confirmed);
  EXPECT_EQ(PrimaryButtonImageCheckmark, primary_button.primaryButtonImage);
  EXPECT_FALSE(primary_button.enabled);
  EXPECT_FALSE(secondary_button.enabled);
  EXPECT_EQ(ChromeButtonStylePrimary, primary_button.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_SUCCESS_TITLE),
              primary_button.accessibilityLabel);
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

// Tests that the subtitle link points to the remote actor's learn more URL.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       SubtitleLinkUsesLearnMoreURL) {
  EXPECT_NSEQ(net::NSURLWithGURL(ExpectedLearnMoreURL()), SubtitleLinkURL());
}

// Tests that tapping the subtitle link opens the learn more URL in a new tab.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TappingSubtitleLinkOpensLearnMoreURL) {
  [view_controller_ loadViewIfNeeded];
  UITextView* subtitle = SubtitleTextView(view_controller_.view);
  ASSERT_TRUE(subtitle);
  ASSERT_NSEQ(view_controller_, subtitle.delegate);

  OCMExpect([scene_handler_
      closePresentedViewsAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                                OpenNewTabCommand* command) {
        return command.URL == ExpectedLearnMoreURL();
      }]]);

  id text_item = OCMClassMock([UITextItem class]);
  UIAction* default_action = [UIAction actionWithHandler:^(UIAction*){
  }];
  UIAction* action = [subtitle.delegate textView:subtitle
                        primaryActionForTextItem:text_item
                                   defaultAction:default_action];
  ASSERT_TRUE(action);
  [action performWithSender:nil target:nil];
  EXPECT_FALSE([subtitle.delegate textView:subtitle
              menuConfigurationForTextItem:text_item
                               defaultMenu:[UIMenu menuWithChildren:@[]]]);
  [text_item stopMocking];
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

// Tests that a single suggestion arriving after the view has loaded has a
// checkmark and isn't selectable.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       SelectedRowHasCheckmark) {
  [view_controller_ setDomain:base::SysUTF16ToNSString(kTestDomain)];
  [view_controller_ loadViewIfNeeded];
  [view_controller_ setSuggestions:@[ PasswordSuggestion(@"user1") ]];

  UITableView* table_view = TableView();
  ASSERT_TRUE(table_view);
  UITableViewCell* cell = CellAtRow(0);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, cell.accessoryType);

  [table_view.delegate tableView:table_view
                 willDisplayCell:cell
               forRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  EXPECT_FALSE(cell.userInteractionEnabled);
}

// Tests that selecting another row moves the checkmark to it.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       SelectingAnotherRowMovesCheckmark) {
  LoadViewWithSuggestions(
      @[ PasswordSuggestion(@"user1"), PasswordSuggestion(@"user2") ]);
  UITableView* table_view = TableView();
  ASSERT_TRUE(table_view);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, CellAtRow(0).accessoryType);
  EXPECT_EQ(UITableViewCellAccessoryNone, CellAtRow(1).accessoryType);

  [table_view.delegate tableView:table_view
         didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]];
  EXPECT_EQ(UITableViewCellAccessoryNone, CellAtRow(0).accessoryType);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, CellAtRow(1).accessoryType);

  [table_view.delegate tableView:table_view
         didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
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

  [table_view.delegate tableView:table_view
         didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]];

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

// Tests that the bottom sheet uses the large detent, including after the
// suggestions arrive.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest, UsesLargeDetent) {
  [view_controller_ loadViewIfNeeded];
  UISheetPresentationController* presentation_controller =
      view_controller_.sheetPresentationController;
  NSArray<UISheetPresentationControllerDetent*>* expected_detents =
      @[ [UISheetPresentationControllerDetent largeDetent] ];
  EXPECT_NSEQ(expected_detents, presentation_controller.detents);
  EXPECT_NSEQ(UISheetPresentationControllerDetentIdentifierLarge,
              presentation_controller.selectedDetentIdentifier);

  [view_controller_ setSuggestions:@[ PasswordSuggestion(@"user1") ]];
  EXPECT_NSEQ(expected_detents, presentation_controller.detents);
  EXPECT_NSEQ(UISheetPresentationControllerDetentIdentifierLarge,
              presentation_controller.selectedDetentIdentifier);
}

// Tests that the no saved passwords state shows its title, a subtitle without
// a link, and a single button, when set after the view loads.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TestNoSavedPasswordsAfterViewLoaded) {
  [view_controller_ setDomain:base::SysUTF16ToNSString(kTestDomain)];
  [view_controller_ loadViewIfNeeded];
  [view_controller_ showNoSavedPasswords];
  ExpectNoSavedPasswordsUI();
}

// Tests that the no saved passwords UI is shown when the empty state is
// triggered before the view is loaded.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TestNoSavedPasswordsBeforeViewLoaded) {
  [view_controller_ setDomain:base::SysUTF16ToNSString(kTestDomain)];
  [view_controller_ showNoSavedPasswords];
  [view_controller_ loadViewIfNeeded];
  ExpectNoSavedPasswordsUI();
}

// Tests that the primary action in the no saved passwords state notifies the
// mutator of the cancellation.
TEST_F(RemoteActorCredentialBottomSheetViewControllerTest,
       TestNoSavedPasswordsPrimaryActionCancels) {
  [view_controller_ showNoSavedPasswords];
  OCMExpect([mutator_ didCancel]);
  [view_controller_.actionHandler confirmationAlertPrimaryAction];
}
