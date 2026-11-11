// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_mutator.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
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
