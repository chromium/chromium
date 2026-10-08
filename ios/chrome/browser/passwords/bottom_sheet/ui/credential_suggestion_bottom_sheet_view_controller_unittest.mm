// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/webauthn/ios/features.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_handler.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

NSString* const kUsername = @"username";
NSString* const kRpId = @"credential-domain.com";
NSString* const kDisplayDescription = @"Passkey • username";
constexpr char kCrossSiteUrl[] = "https://cross-site-domain.com";
constexpr char kCredentialUrl[] = "https://credential-domain.com";
constexpr char kRtlIdnUrl[] =
    "https://paypal.com.xn--4gbrim.xn--ngbc5azd/login";
constexpr char16_t kExpectedLtrWrappedPunycodeHost[] =
    u"\x202a"
    u"paypal.com.xn--4gbrim.xn--ngbc5azd"
    u"\x202c";
NSString* const kCrossSiteDomain = @"cross-site-domain.com";
NSString* const kCredentialDomain = @"credential-domain.com";

}  // namespace

class CredentialSuggestionBottomSheetViewControllerTest : public PlatformTest {
 protected:
  CredentialSuggestionBottomSheetViewControllerTest() {
    handler_ =
        OCMProtocolMock(@protocol(CredentialSuggestionBottomSheetHandler));
  }

  void CreateViewController(const GURL& url) {
    view_controller_ = [[CredentialSuggestionBottomSheetViewController alloc]
        initWithHandler:handler_
                    URL:url];
  }

  id handler_;
  CredentialSuggestionBottomSheetViewController* view_controller_;
};

// Tests that the RP ID is displayed as a second subtitle when there is a domain
// mismatch.
TEST_F(CredentialSuggestionBottomSheetViewControllerTest,
       RpIdDisplayedWhenMismatched) {
  CreateViewController(GURL(kCrossSiteUrl));

  FormSuggestion* suggestion = [FormSuggestion
              suggestionWithValue:kUsername
                       minorValue:kRpId
               displayDescription:kDisplayDescription
                             icon:nil
                             type:autofill::SuggestionType::kWebauthnCredential
                          payload:autofill::Suggestion::Guid("fake_guid")
      fieldByFieldFillingTypeUsed:autofill::FieldType::EMPTY_TYPE
                   requiresReauth:YES
       acceptanceA11yAnnouncement:nil];

  [view_controller_ setSuggestions:@[ suggestion ] andDomain:kCrossSiteDomain];
  [view_controller_ loadViewIfNeeded];

  UITableView* tableView =
      [[UITableView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
  [TableViewCellContentConfiguration registerCellForTableView:tableView];

  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];

  id<UITableViewDataSource> dataSource =
      (id<UITableViewDataSource>)view_controller_;
  UITableViewCell* cell = [dataSource tableView:tableView
                          cellForRowAtIndexPath:indexPath];

  TableViewCellContentConfiguration* configuration =
      (TableViewCellContentConfiguration*)cell.contentConfiguration;
  EXPECT_NSEQ(kRpId, configuration.secondSubtitle);
}

// Tests that the RP ID is not displayed as a second subtitle when the domains
// match.
TEST_F(CredentialSuggestionBottomSheetViewControllerTest,
       RpIdNotDisplayedWhenMatched) {
  CreateViewController(GURL(kCredentialUrl));

  FormSuggestion* suggestion = [FormSuggestion
              suggestionWithValue:kUsername
                       minorValue:kRpId
               displayDescription:kDisplayDescription
                             icon:nil
                             type:autofill::SuggestionType::kWebauthnCredential
                          payload:autofill::Suggestion::Guid("fake_guid")
      fieldByFieldFillingTypeUsed:autofill::FieldType::EMPTY_TYPE
                   requiresReauth:YES
       acceptanceA11yAnnouncement:nil];

  [view_controller_ setSuggestions:@[ suggestion ] andDomain:kCredentialDomain];
  [view_controller_ loadViewIfNeeded];

  UITableView* tableView =
      [[UITableView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
  [TableViewCellContentConfiguration registerCellForTableView:tableView];

  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];

  id<UITableViewDataSource> dataSource =
      (id<UITableViewDataSource>)view_controller_;
  UITableViewCell* cell = [dataSource tableView:tableView
                          cellForRowAtIndexPath:indexPath];

  TableViewCellContentConfiguration* configuration =
      (TableViewCellContentConfiguration*)cell.contentConfiguration;
  EXPECT_NSEQ(nil, configuration.secondSubtitle);
}

// Test that an IDN URL containing strong RTL characters is formatted as
// punycode and wrapped in LTR directional formatting in the subtitle.
TEST_F(CredentialSuggestionBottomSheetViewControllerTest,
       SubtitleFormatsRtlIdnAsPunycodeWithLtrWrapping) {
  CreateViewController(GURL(kRtlIdnUrl));
  [view_controller_ loadViewIfNeeded];

  NSString* expectedSubtitle = l10n_util::GetNSStringF(
      IsConditionalPasskeyLoginEnabled()
          ? IDS_IOS_CREDENTIAL_BOTTOM_SHEET_SUBTITLE_WITH_PASSKEYS
          : IDS_IOS_CREDENTIAL_BOTTOM_SHEET_SUBTITLE,
      kExpectedLtrWrappedPunycodeHost);
  EXPECT_NSEQ(expectedSubtitle, view_controller_.subtitleString);
}
