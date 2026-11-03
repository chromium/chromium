// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/preset_credential_suggestion_bottom_sheet_mediator.h"

#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base+Subclassing.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

constexpr char kTestUrl[] = "https://example.com";

// Create a test password suggestion.
FormSuggestion* CreateSuggestion(NSString* username, NSString* description) {
  return [FormSuggestion
      suggestionWithValue:username
       displayDescription:description
                     icon:nil
                     type:autofill::SuggestionType::kPasswordEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
}

}  // namespace

// Tests for PresetCredentialSuggestionBottomSheetMediator.
class PresetCredentialSuggestionBottomSheetMediatorTest : public PlatformTest {
 protected:
  PresetCredentialSuggestionBottomSheetMediatorTest()
      : web_state_list_(&web_state_list_delegate_) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetCurrentURL(GURL(kTestUrl));
    web_state_list_.InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  web::WebTaskEnvironment task_environment_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
};

// Tests that setting the consumer configures it with suggestions and actions.
TEST_F(PresetCredentialSuggestionBottomSheetMediatorTest, SetsUpConsumer) {
  FormSuggestion* suggestion =
      CreateSuggestion(@"user@example.com", @"example.com");
  PresetCredentialSuggestionBottomSheetMediator* mediator =
      [[PresetCredentialSuggestionBottomSheetMediator alloc]
          initWithWebStateList:&web_state_list_
                 faviconLoader:nullptr
                   suggestions:@[ suggestion ]];

  id consumer =
      OCMStrictProtocolMock(@protocol(CredentialSuggestionBottomSheetConsumer));
  OCMExpect([consumer setSuggestions:@[ suggestion ] andDomain:@"example.com"]);
  OCMExpect([consumer
      setPrimaryActionString:l10n_util::GetNSString(
                                 IDS_IOS_CREDENTIAL_BOTTOM_SHEET_USE_PASSWORD)
       secondaryActionString:l10n_util::GetNSString(IDS_CANCEL)
        secondaryActionImage:nil]);

  mediator.consumer = consumer;
  EXPECT_OCMOCK_VERIFY(consumer);

  [mediator disconnect];
}

// Tests that selecting a suggestion calls the completion handler.
TEST_F(PresetCredentialSuggestionBottomSheetMediatorTest,
       SelectsSuggestionAndCallsCompletion) {
  FormSuggestion* suggestion =
      CreateSuggestion(@"user@example.com", @"example.com");
  __block BOOL completion_called = NO;

  PresetCredentialSuggestionBottomSheetMediator* mediator =
      [[PresetCredentialSuggestionBottomSheetMediator alloc]
          initWithWebStateList:&web_state_list_
                 faviconLoader:nullptr
                   suggestions:@[ suggestion ]];

  [mediator selectSuggestion:suggestion
                     atIndex:0
                  completion:^{
                    completion_called = YES;
                  }];

  EXPECT_TRUE(completion_called);

  [mediator disconnect];
}
