// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_inline_notice_view.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Constants for mock search items.
NSString* const kPassportTypeName = @"Passport";
NSString* const kPassportValue = @"AA123456";
NSString* const kExpirationTypeName = @"Expiration";
NSString* const kExpirationValue = @"2030-01-01";

// Search query used for testing view controller search states.
NSString* const kSearchQuery = @"test search query";

}  // namespace

class AtMemorySearchViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[AtMemorySearchViewController alloc]
        initWithStyle:UITableViewStylePlain];
    [view_controller_ loadViewIfNeeded];
  }

  void TearDown() override {
    view_controller_ = nil;
    PlatformTest::TearDown();
  }

  AtMemorySearchViewController* view_controller_;
};

// Tests that the view controller, navigation items, search bar, and table view
// are initialized properly.
TEST_F(AtMemorySearchViewControllerTest, TestInitialization) {
  EXPECT_NE(view_controller_.navigationItem.searchController, nil);
  EXPECT_NE(view_controller_.navigationItem.rightBarButtonItem, nil);
  EXPECT_NE(view_controller_.tableView, nil);
}

// Tests that the table view displays an empty background view with no items
// when in the initial zero state (no notice and no recent fills).
TEST_F(AtMemorySearchViewControllerTest, TestZeroState) {
  [view_controller_ setNoticeVisible:NO];
  EXPECT_EQ(view_controller_.tableView.numberOfSections, 0);
  EXPECT_NE(view_controller_.tableView.backgroundView, nil);
}

// Tests that setting search results populates the table view.
TEST_F(AtMemorySearchViewControllerTest, TestSetSearchResults) {
  autofill::Suggestion suggestion(
      base::SysNSStringToUTF16(kPassportValue),
      autofill::SuggestionType::kAtMemorySearchResult);
  autofill::Suggestion::AtMemoryPayload payload(
      base::SysNSStringToUTF16(kPassportValue),
      autofill::MemoryDataType::kPassportNumber);
  payload.type_name = base::SysNSStringToUTF16(kPassportTypeName);
  suggestion.payload = std::move(payload);

  AtMemorySearchItem* item =
      [[AtMemorySearchItem alloc] initWithSuggestion:suggestion index:0];
  [view_controller_ setSearchResults:@[ item ]];

  EXPECT_EQ(view_controller_.tableView.numberOfSections, 1);
  EXPECT_EQ([view_controller_.tableView numberOfRowsInSection:0], 1);
  EXPECT_EQ(view_controller_.tableView.backgroundView, nil);

  UITableViewCell* cell = [view_controller_.tableView.dataSource
                  tableView:view_controller_.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];

  TableViewCellContentConfiguration* config =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  EXPECT_NSEQ(config.title, kPassportValue);
  EXPECT_NSEQ(config.subtitle, kPassportTypeName);
}

// Tests that selecting a search result item calls the mutator.
TEST_F(AtMemorySearchViewControllerTest, TestSelectSearchResultItem) {
  id mutator = OCMProtocolMock(@protocol(AtMemorySearchMutator));
  view_controller_.mutator = mutator;

  autofill::Suggestion suggestion(
      base::SysNSStringToUTF16(kPassportValue),
      autofill::SuggestionType::kAtMemorySearchResult);
  autofill::Suggestion::AtMemoryPayload payload(
      base::SysNSStringToUTF16(kPassportValue),
      autofill::MemoryDataType::kPassportNumber);
  payload.type_name = base::SysNSStringToUTF16(kPassportTypeName);
  suggestion.payload = std::move(payload);

  AtMemorySearchItem* item =
      [[AtMemorySearchItem alloc] initWithSuggestion:suggestion index:0];
  [view_controller_ setSearchResults:@[ item ]];

  OCMExpect([mutator didSelectSearchResultItem:item]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];

  EXPECT_OCMOCK_VERIFY(mutator);
}

// Tests that selecting the unsupported query item dismisses AtMemory and starts
// Gemini flow.
TEST_F(AtMemorySearchViewControllerTest, TestSelectUnsupportedQueryItem) {
  id mock_at_memory_handler = OCMProtocolMock(@protocol(AtMemoryCommands));
  view_controller_.atMemoryHandler = mock_at_memory_handler;

  id mock_gemini_handler = OCMProtocolMock(@protocol(GeminiCommands));
  view_controller_.geminiHandler = mock_gemini_handler;

  UISearchController* search_controller =
      view_controller_.navigationItem.searchController;
  search_controller.searchBar.text = kSearchQuery;

  // Set error type to UnsupportedQueryError to show the unsupported query item.
  [view_controller_ setErrorType:AtMemoryErrorType::kUnsupportedQueryError];

  OCMExpect([mock_at_memory_handler dismissAtMemory]);

  OCMExpect(
      [mock_gemini_handler
          startGeminiEntryFlowWithStartupState:[OCMArg checkWithBlock:^BOOL(
                                                           GeminiStartupState*
                                                               state) {
            return state.entryPoint == gemini::EntryPoint::AtMemorySearch &&
                   [state.prepopulatedPrompt isEqualToString:kSearchQuery];
          }]
                            baseViewController:view_controller_
                      showSnackbarOnCompletion:NO
                                    completion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        GeminiEntryFlowCompletion completion;
        [invocation getArgument:&completion atIndex:5];
        if (completion) {
          completion(kGeminiEntryFlowResultSuccess);
        }
      });

  base::UserActionTester user_action_tester;

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];

  EXPECT_EQ(
      user_action_tester.GetActionCount("IOS.AtMemory.UnsupportedQueryTapped"),
      1);
  EXPECT_OCMOCK_VERIFY(mock_at_memory_handler);
  EXPECT_OCMOCK_VERIFY(mock_gemini_handler);
}

// Tests that the table view displays the search cell when in the search
// (typing) state.
TEST_F(AtMemorySearchViewControllerTest, TestSearchState) {
  UISearchController* search_controller =
      view_controller_.navigationItem.searchController;
  search_controller.searchBar.text = kSearchQuery;
  [(id<UISearchResultsUpdating>)view_controller_
      updateSearchResultsForSearchController:search_controller];

  EXPECT_EQ(view_controller_.tableView.numberOfSections, 2);
  EXPECT_EQ([view_controller_.tableView numberOfRowsInSection:0], 1);

  UITableViewCell* cell = [view_controller_.tableView.dataSource
                  tableView:view_controller_.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  EXPECT_TRUE(cell.userInteractionEnabled);

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  EXPECT_NSEQ(configuration.title, search_controller.searchBar.text);
}

// Tests that the table view displays the fetching cell when in the fetching
// state.
TEST_F(AtMemorySearchViewControllerTest, TestFetchingState) {
  UISearchBar* search_bar =
      view_controller_.navigationItem.searchController.searchBar;
  search_bar.text = kSearchQuery;
  [(id<UISearchBarDelegate>)view_controller_
      searchBarSearchButtonClicked:search_bar];

  EXPECT_EQ(view_controller_.tableView.numberOfSections, 1);
  EXPECT_EQ([view_controller_.tableView numberOfRowsInSection:0], 1);

  UITableViewCell* cell = [view_controller_.tableView.dataSource
                  tableView:view_controller_.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  EXPECT_FALSE(cell.userInteractionEnabled);
}

// Tests that the table view displays the notice cell when notice is visible
// in the initial state.
TEST_F(AtMemorySearchViewControllerTest, TestNoticeVisibleInInitialState) {
  [view_controller_ setNoticeVisible:YES];

  EXPECT_EQ(view_controller_.tableView.numberOfSections, 1);
  EXPECT_EQ([view_controller_.tableView numberOfRowsInSection:0], 1);
  EXPECT_EQ(view_controller_.tableView.backgroundView, nil);

  UITableViewCell* cell = [view_controller_.tableView.dataSource
                  tableView:view_controller_.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  EXPECT_EQ(cell.selectionStyle, UITableViewCellSelectionStyleNone);
  EXPECT_NSEQ(cell.backgroundConfiguration.backgroundColor,
              [UIColor colorNamed:kGroupedSecondaryBackgroundColor]);

  AtMemoryInlineNoticeConfiguration* config =
      base::apple::ObjCCastStrict<AtMemoryInlineNoticeConfiguration>(
          cell.contentConfiguration);
  EXPECT_EQ((id)config.delegate, (id)view_controller_);
}

// Tests that toggling notice visibility dynamically updates the table view
// sections, rows, and background view.
TEST_F(AtMemorySearchViewControllerTest, TestNoticeToggleVisibility) {
  [view_controller_ setNoticeVisible:NO];
  EXPECT_EQ(view_controller_.tableView.numberOfSections, 0);
  EXPECT_NE(view_controller_.tableView.backgroundView, nil);

  [view_controller_ setNoticeVisible:YES];
  EXPECT_EQ(view_controller_.tableView.numberOfSections, 1);
  EXPECT_EQ([view_controller_.tableView numberOfRowsInSection:0], 1);
  EXPECT_EQ(view_controller_.tableView.backgroundView, nil);

  [view_controller_ setNoticeVisible:NO];
  EXPECT_EQ(view_controller_.tableView.numberOfSections, 0);
  EXPECT_NE(view_controller_.tableView.backgroundView, nil);

  [view_controller_ setNoticeVisible:NO];
  EXPECT_EQ(view_controller_.tableView.numberOfSections, 0);
  EXPECT_NE(view_controller_.tableView.backgroundView, nil);
}

// Tests that tapping the OK button on the notice informs the mutator to
// acknowledge the privacy notice.
TEST_F(AtMemorySearchViewControllerTest, TestNoticeOKTappedNotifiesMutator) {
  id mock_mutator = OCMProtocolMock(@protocol(AtMemorySearchMutator));
  view_controller_.mutator = mock_mutator;

  OCMExpect([mock_mutator acknowledgePrivacyNotice]);

  [(id<AtMemoryInlineNoticeViewDelegate>)view_controller_
      inlineNoticeViewDidTapOK:nil];

  EXPECT_OCMOCK_VERIFY(mock_mutator);
}

// Tests that tapping the Settings link on the notice informs the mutator.
TEST_F(AtMemorySearchViewControllerTest,
       TestNoticeSettingsTappedNotifiesMutator) {
  id mock_mutator = OCMProtocolMock(@protocol(AtMemorySearchMutator));
  view_controller_.mutator = mock_mutator;

  OCMExpect([mock_mutator didTapSettingsLink]);

  [(id<AtMemoryInlineNoticeViewDelegate>)view_controller_
      inlineNoticeViewDidTapSettings:nil];

  EXPECT_OCMOCK_VERIFY(mock_mutator);
}

// Tests that tapping the footer link calls openManageEnhancedAutofillDetails.
TEST_F(AtMemorySearchViewControllerTest, TestTapsFooterLink) {
  id atMemoryHandler = OCMProtocolMock(@protocol(AtMemoryCommands));
  view_controller_.atMemoryHandler = atMemoryHandler;

  OCMExpect([atMemoryHandler openManageEnhancedAutofillDetails]);

  CrURL* mock_url =
      [[CrURL alloc] initWithGURL:GURL("settings://ai_disclosure")];
  // Cast to id to bypass the static type check for the delegate method.
  [(id<TableViewLinkHeaderFooterItemDelegate>)view_controller_ view:nil
                                                      didTapLinkURL:mock_url];

  OCMReject([atMemoryHandler openManageEnhancedAutofillDetails]);

  mock_url = [[CrURL alloc] initWithGURL:GURL("settings://incorrect_url")];
  // Cast to id to bypass the static type check for the delegate method.
  [(id<TableViewLinkHeaderFooterItemDelegate>)view_controller_ view:nil
                                                      didTapLinkURL:mock_url];

  EXPECT_OCMOCK_VERIFY(atMemoryHandler);
}

// Tests that search results remain visible when
// updateSearchResultsForSearchController is called with an unchanged search
// query (e.g. returning from granular fill with notice).
TEST_F(AtMemorySearchViewControllerTest,
       TestSearchResultsPreservedWhenUpdatingSearchControllerWithSameQuery) {
  UISearchController* search_controller =
      view_controller_.navigationItem.searchController;
  search_controller.searchBar.text = kSearchQuery;

  autofill::Suggestion suggestion(
      base::SysNSStringToUTF16(kPassportValue),
      autofill::SuggestionType::kAtMemorySearchResult);
  autofill::Suggestion::AtMemoryPayload payload(
      base::SysNSStringToUTF16(kPassportValue),
      autofill::MemoryDataType::kPassportNumber);
  payload.type_name = base::SysNSStringToUTF16(kPassportTypeName);
  suggestion.payload = std::move(payload);

  AtMemorySearchItem* item =
      [[AtMemorySearchItem alloc] initWithSuggestion:suggestion index:0];
  [view_controller_ setSearchResults:@[ item ]];
  [view_controller_ setNoticeVisible:YES];

  ASSERT_EQ(view_controller_.tableView.numberOfSections, 2);
  ASSERT_EQ([view_controller_.tableView numberOfRowsInSection:1], 1);

  // Trigger search results updater with the same query.
  [(id<UISearchResultsUpdating>)view_controller_
      updateSearchResultsForSearchController:search_controller];

  // Search results should still be present in section 1 and notice in section
  // 0.
  ASSERT_EQ(view_controller_.tableView.numberOfSections, 2);
  ASSERT_EQ([view_controller_.tableView numberOfRowsInSection:1], 1);

  UITableViewCell* cell = [view_controller_.tableView.dataSource
                  tableView:view_controller_.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:1]];
  TableViewCellContentConfiguration* config =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  EXPECT_NSEQ(config.title, kPassportValue);
  EXPECT_NSEQ(config.subtitle, kPassportTypeName);
}

// Tests that search results are reset to the search typing state when the query
// changes.
TEST_F(AtMemorySearchViewControllerTest,
       TestSearchResultsResetWhenQueryChanges) {
  UISearchController* search_controller =
      view_controller_.navigationItem.searchController;
  search_controller.searchBar.text = kSearchQuery;

  autofill::Suggestion suggestion(
      base::SysNSStringToUTF16(kPassportValue),
      autofill::SuggestionType::kAtMemorySearchResult);
  autofill::Suggestion::AtMemoryPayload payload(
      base::SysNSStringToUTF16(kPassportValue),
      autofill::MemoryDataType::kPassportNumber);
  payload.type_name = base::SysNSStringToUTF16(kPassportTypeName);
  suggestion.payload = std::move(payload);

  AtMemorySearchItem* item =
      [[AtMemorySearchItem alloc] initWithSuggestion:suggestion index:0];
  [view_controller_ setSearchResults:@[ item ]];

  ASSERT_EQ(view_controller_.tableView.numberOfSections, 1);
  ASSERT_EQ([view_controller_.tableView numberOfRowsInSection:0], 1);

  // Change search query.
  search_controller.searchBar.text = @"new query";
  [(id<UISearchResultsUpdating>)view_controller_
      updateSearchResultsForSearchController:search_controller];

  // Table view should transition to search typing state (search section +
  // footer).
  ASSERT_EQ(view_controller_.tableView.numberOfSections, 2);
  ASSERT_EQ([view_controller_.tableView numberOfRowsInSection:0], 1);

  UITableViewCell* cell = [view_controller_.tableView.dataSource
                  tableView:view_controller_.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  EXPECT_NSEQ(configuration.title, @"new query");
}

// Parameters for AtMemorySearchViewControllerErrorTest.
struct AtMemoryErrorTestParam {
  const char* test_name;
  AtMemoryErrorType error_type;
  CGFloat expected_alpha;
  bool expected_user_interaction_enabled;
};

// Parameterized test fixture to verify that setting error types configures
// the table view cell opacity and interaction state as expected.
class AtMemorySearchViewControllerErrorTest
    : public AtMemorySearchViewControllerTest,
      public ::testing::WithParamInterface<AtMemoryErrorTestParam> {};

// Tests that setting each error type properly configures the cell alpha and
// user interaction state.
TEST_P(AtMemorySearchViewControllerErrorTest, TestErrorState) {
  const AtMemoryErrorTestParam& param = GetParam();
  [view_controller_ setErrorType:param.error_type];

  EXPECT_EQ(view_controller_.tableView.numberOfSections, 1);
  EXPECT_EQ([view_controller_.tableView numberOfRowsInSection:0], 1);

  UITableViewCell* cell = [view_controller_.tableView.dataSource
                  tableView:view_controller_.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  EXPECT_EQ(cell.contentView.alpha, param.expected_alpha);
  EXPECT_EQ(cell.userInteractionEnabled,
            param.expected_user_interaction_enabled);
}

// Instantiates the test suite with various combinations of error types to
// ensure cell opacity and interaction states are correctly configured.
INSTANTIATE_TEST_SUITE_P(
    All,
    AtMemorySearchViewControllerErrorTest,
    ::testing::Values(
        AtMemoryErrorTestParam{
            .test_name = "NoDataError",
            .error_type = AtMemoryErrorType::kNoDataError,
            .expected_alpha = kDefaultCellAlpha,
            .expected_user_interaction_enabled = false,
        },
        AtMemoryErrorTestParam{
            .test_name = "NoConnectionError",
            .error_type = AtMemoryErrorType::kNoConnectionError,
            .expected_alpha = kDisabledCellAlpha,
            .expected_user_interaction_enabled = false,
        },
        AtMemoryErrorTestParam{
            .test_name = "UnsupportedQueryError",
            .error_type = AtMemoryErrorType::kUnsupportedQueryError,
            .expected_alpha = kDefaultCellAlpha,
            .expected_user_interaction_enabled = true,
        }),
    [](const ::testing::TestParamInfo<AtMemoryErrorTestParam>& info) {
      return info.param.test_name;
    });
