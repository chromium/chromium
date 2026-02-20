// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_suggestion_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/password_manager/core/browser/passkey_credential.h"
#import "components/webauthn/ios/ios_passkey_client.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate_factory.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager::PasskeyCredential;

namespace {

constexpr char kFrameId[] = "frame_id";
constexpr char kRequestId[] = "request_id";
constexpr char kDisplayName[] = "display_name";

// Returns a generic PasskeyCredential.
PasskeyCredential CreatePasskeyCredential() {
  return PasskeyCredential(PasskeyCredential::Source::kGooglePasswordManager,
                           PasskeyCredential::RpId("example.com"),
                           PasskeyCredential::CredentialId({1, 2, 3, 4}),
                           PasskeyCredential::UserId(),
                           PasskeyCredential::Username("username"),
                           PasskeyCredential::DisplayName(kDisplayName));
}

}  // namespace

// Test fixture for PasskeySuggestionBottomSheetMediator.
class PasskeySuggestionBottomSheetMediatorTest : public PlatformTest {
 protected:
  PasskeySuggestionBottomSheetMediatorTest() {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    web_state_->SetWebFramesManager(ContentWorldForAutofillJavascriptFeatures(),
                                    std::move(web_frames_manager));

    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());

    webauthn::IOSWebAuthnCredentialsDelegateFactory* factory =
        webauthn::IOSWebAuthnCredentialsDelegateFactory::GetFactory(web_state_);
    webauthn_credentials_delegate_ = factory->GetDelegateForFrame(kFrameId);

    consumer_ =
        OCMProtocolMock(@protocol(CredentialSuggestionBottomSheetConsumer));
  }

  void TearDown() override { [mediator_ disconnect]; }

  void CreateMediator() {
    mediator_ = [[PasskeySuggestionBottomSheetMediator alloc]
        initWithWebStateList:web_state_list_.get()
                 requestInfo:{kFrameId, kRequestId}];
  }

  web::WebTaskEnvironment task_environment_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  raw_ptr<web::FakeWebState> web_state_;
  raw_ptr<webauthn::IOSWebAuthnCredentialsDelegate>
      webauthn_credentials_delegate_;
  id consumer_;
  PasskeySuggestionBottomSheetMediator* mediator_;
};

// Tests that the consumer is notified with the available passkey suggestions.
TEST_F(PasskeySuggestionBottomSheetMediatorTest, InitializeSuggestions) {
  std::vector<PasskeyCredential> credentials = {CreatePasskeyCredential()};
  webauthn_credentials_delegate_->OnCredentialsReceived(credentials,
                                                        kRequestId);

  CreateMediator();

  OCMExpect([consumer_
      setSuggestions:[OCMArg checkWithBlock:^BOOL(
                                 NSArray<FormSuggestion*>* suggestions) {
        EXPECT_EQ(suggestions.count, 1u);
        FormSuggestion* suggestion = suggestions[0];
        EXPECT_NSEQ(suggestion.value, base::SysUTF8ToNSString(kDisplayName));
        EXPECT_EQ(suggestion.type,
                  autofill::SuggestionType::kWebauthnCredential);
        return YES;
      }]
           andDomain:[OCMArg isNotNil]]);
  OCMExpect([consumer_
      setPrimaryActionString:l10n_util::GetNSString(
                                 IDS_IOS_CREDENTIAL_BOTTOM_SHEET_CONTINUE)]);

  [mediator_ setConsumer:consumer_];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is not notified when there aren't any passkey
// suggestions to present.
TEST_F(PasskeySuggestionBottomSheetMediatorTest, HandleEmptySuggestions) {
  std::vector<PasskeyCredential> credentials;
  webauthn_credentials_delegate_->OnCredentialsReceived(credentials,
                                                        kRequestId);

  CreateMediator();

  OCMReject([consumer_ setSuggestions:[OCMArg any] andDomain:[OCMArg any]]);

  [mediator_ setConsumer:consumer_];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the mediator is correctly cleaned up when the WebStateList is
// destroyed. There are a lot of checked observer lists that could potentially
// cause a crash in the process, so this test ensures they're executed.
TEST_F(PasskeySuggestionBottomSheetMediatorTest,
       CleanupWhenWebStateListDestroyed) {
  CreateMediator();
  ASSERT_TRUE(mediator_);

  // Pointers in the test fixture must be nullified before the objects they
  // point to are destroyed to avoid dangling pointer errors.
  web_state_ = nullptr;
  webauthn_credentials_delegate_ = nullptr;

  web_state_list_.reset();
}
