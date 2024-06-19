// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mediator.h"

#import "base/functional/callback_helpers.h"
#import "base/test/mock_callback.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mutator.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

using ChallengeOptionId =
    autofill::CardUnmaskChallengeOption::ChallengeOptionId;

class CardUnmaskAuthenticationSelectionMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    consumer_ =
        OCMProtocolMock(@protocol(CardUnmaskAuthenticationSelectionConsumer));
    delegate_ = OCMProtocolMock(
        @protocol(CardUnmaskAuthenticationSelectionMediatorDelegate));
  }

  void TearDown() override {
    if (consumer_) {
      EXPECT_OCMOCK_VERIFY((id)consumer_);
    }
    if (delegate_) {
      EXPECT_OCMOCK_VERIFY((id)delegate_);
    }
  }

  CardUnmaskAuthenticationSelectionMediator* InitializeMediator(
      const std::vector<autofill::CardUnmaskChallengeOption>&
          challenge_options) {
    CHECK(!mediator_) << "Only initialize the mediator once in tests.";
    controller_ = std::make_unique<
        autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>(
        challenge_options, confirm_unmasking_method_callback_.Get(),
        cancel_unmasking_closure_.Get());
    mediator_ = std::make_unique<CardUnmaskAuthenticationSelectionMediator>(
        controller_->GetWeakPtr(), consumer_);
    mediator_->set_delegate(delegate_);
    return mediator_.get();
  }

  id<CardUnmaskAuthenticationSelectionConsumer> consumer() { return consumer_; }

  id<CardUnmaskAuthenticationSelectionMediatorDelegate> delegate() {
    return delegate_;
  }

  autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl*
  controller() {
    return controller_.get();
  }

  // An SMS autofill challenge option. Matching the IOS version below.
  autofill::CardUnmaskChallengeOption SmsAutofillChallengeOption() {
    return autofill::CardUnmaskChallengeOption(
        ChallengeOptionId("id1"),
        autofill::CardUnmaskChallengeOptionType::kSmsOtp,
        /*challenge_info=*/u"challenge_info1",
        /*challenge_length_input=*/1U);
  }
  // An SMS challenge option. Matching the autofill version above.
  CardUnmaskChallengeOptionIOS* SmsIOSChallengeOption() {
    return [[CardUnmaskChallengeOptionIOS alloc]
           initWithId:ChallengeOptionId("id1")
            modeLabel:l10n_util::GetNSString(
                          IDS_AUTOFILL_AUTHENTICATION_MODE_GET_TEXT_MESSAGE)
        challengeInfo:@"challenge_info1"];
  }

  // A CVC autofill challenge option. Matching the IOS version below.
  autofill::CardUnmaskChallengeOption CvcAutofillChallengeOption() {
    return autofill::CardUnmaskChallengeOption(
        ChallengeOptionId("id2"), autofill::CardUnmaskChallengeOptionType::kCvc,
        /*challenge_info=*/u"challenge_info2",
        /*challenge_length_input=*/2U);
  }
  // A CVC challenge option. Matching the autofill version above.
  CardUnmaskChallengeOptionIOS* CvcIOSChallengeOption() {
    return [[CardUnmaskChallengeOptionIOS alloc]
           initWithId:ChallengeOptionId("id2")
            modeLabel:l10n_util::GetNSString(
                          IDS_AUTOFILL_AUTHENTICATION_MODE_SECURITY_CODE)
        challengeInfo:@"challenge_info2"];
  }

 protected:
  base::MockOnceCallback<void(const std::string&)>
      confirm_unmasking_method_callback_;
  base::MockOnceClosure cancel_unmasking_closure_;

 private:
  id<CardUnmaskAuthenticationSelectionConsumer> consumer_;
  id<CardUnmaskAuthenticationSelectionMediatorDelegate> delegate_;
  // Mediator listed first to destruct the controller (which holds a reference
  // to the mediator) before the mediator.
  std::unique_ptr<CardUnmaskAuthenticationSelectionMediator> mediator_;
  std::unique_ptr<
      autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>
      controller_;
};

TEST_F(CardUnmaskAuthenticationSelectionMediatorTest,
       SetsHeaderTitleAndHeaderTextAndOptionsAndFooterAndChallengeAcceptance) {
  OCMExpect([consumer()
      setHeaderTitle:
          l10n_util::GetNSString(
              IDS_AUTOFILL_CARD_AUTH_SELECTION_DIALOG_TITLE_MULTIPLE_OPTIONS)]);
  OCMExpect([consumer()
      setHeaderText:
          l10n_util::GetNSString(
              IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_ISSUER_CONFIRMATION_TEXT)]);
  NSArray<CardUnmaskChallengeOptionIOS*>* ios_challenge_options =
      @[ SmsIOSChallengeOption(), CvcIOSChallengeOption() ];
  OCMExpect([consumer() setCardUnmaskOptions:ios_challenge_options]);
  OCMExpect([consumer()
      setFooterText:
          l10n_util::GetNSString(
              IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CURRENT_INFO_NOT_SEEN_TEXT)]);
  OCMExpect([consumer()
      setChallengeAcceptanceLabel:
          l10n_util::GetNSString(
              IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL_SEND)]);

  InitializeMediator(
      {SmsAutofillChallengeOption(), CvcAutofillChallengeOption()});
}

TEST_F(CardUnmaskAuthenticationSelectionMediatorTest,
       SetsSendLabelInitiallyWhenSmsIsTheFirstChallengeOption) {
  OCMExpect([consumer()
      setChallengeAcceptanceLabel:
          l10n_util::GetNSString(
              IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL_SEND)]);

  InitializeMediator(
      {SmsAutofillChallengeOption(), CvcAutofillChallengeOption()});
}

TEST_F(CardUnmaskAuthenticationSelectionMediatorTest,
       SetsContinueLabelInitiallyWhenCvcIsTheFirstChallengeOption) {
  OCMExpect([consumer()
      setChallengeAcceptanceLabel:
          l10n_util::GetNSString(
              IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL_CONTINUE)]);

  InitializeMediator(
      {CvcAutofillChallengeOption(), SmsAutofillChallengeOption()});
}

TEST_F(CardUnmaskAuthenticationSelectionMediatorTest,
       OnDidSelectChallengeOption_SetsButtonLabel) {
  CardUnmaskAuthenticationSelectionMediator* mediator =
      InitializeMediator({SmsAutofillChallengeOption()});

  OCMExpect([consumer()
      setChallengeAcceptanceLabel:
          l10n_util::GetNSString(
              IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL_SEND)]);
  [mediator->AsMutator() didSelectChallengeOption:SmsIOSChallengeOption()];
}

TEST_F(CardUnmaskAuthenticationSelectionMediatorTest,
       OnUpdateContent_CallsEnterPendingState) {
  // The interface method CardUnmaskAuthenticationSelectionDialog::UpdateContent
  // is called to set the pending state.
  CardUnmaskAuthenticationSelectionMediator* mediator =
      InitializeMediator({SmsAutofillChallengeOption()});

  OCMExpect([consumer() enterPendingState]);

  mediator->UpdateContent();
}

TEST_F(CardUnmaskAuthenticationSelectionMediatorTest,
       OnAcceptedSelection_CallsConfirmUnmaskingMethodCallback) {
  CardUnmaskAuthenticationSelectionMediator* mediator = InitializeMediator(
      {SmsAutofillChallengeOption(), CvcAutofillChallengeOption()});
  mediator->DidSelectChallengeOption(CvcIOSChallengeOption());

  EXPECT_CALL(confirm_unmasking_method_callback_,
              Run(CvcAutofillChallengeOption().id.value()));
  [mediator->AsMutator() didAcceptSelection];
}

TEST_F(CardUnmaskAuthenticationSelectionMediatorTest,
       OnCancelSelection_CallsCancelUnmaskingClosure) {
  CardUnmaskAuthenticationSelectionMediator* mediator = InitializeMediator(
      {SmsAutofillChallengeOption(), CvcAutofillChallengeOption()});
  mediator->DidSelectChallengeOption(CvcIOSChallengeOption());

  EXPECT_CALL(cancel_unmasking_closure_, Run());
  [mediator->AsMutator() didCancelSelection];
}

TEST_F(CardUnmaskAuthenticationSelectionMediatorTest,
       OnCancelSelection_CallsDelegateToDismiss) {
  CardUnmaskAuthenticationSelectionMediator* mediator =
      InitializeMediator({SmsAutofillChallengeOption()});

  OCMExpect([delegate() dismissAuthenticationSelection]);
  [mediator->AsMutator() didCancelSelection];
}

TEST_F(CardUnmaskAuthenticationSelectionMediatorTest,
       ServerProcessedAuthentication_DoesNotCallCancelUnmaskingClosure) {
  InitializeMediator({SmsAutofillChallengeOption()});

  EXPECT_CALL(cancel_unmasking_closure_, Run()).Times(0);
  controller()->DismissDialogUponServerProcessedAuthenticationMethodRequest(
      /*server_success=*/true);
}

TEST_F(CardUnmaskAuthenticationSelectionMediatorTest,
       ServerProcessedAuthentication_DoesNotDismissAuthenticationSelection) {
  id<CardUnmaskAuthenticationSelectionMediatorDelegate> delegate =
      OCMStrictProtocolMock(
          @protocol(CardUnmaskAuthenticationSelectionMediatorDelegate));
  CardUnmaskAuthenticationSelectionMediator* mediator =
      InitializeMediator({SmsAutofillChallengeOption()});
  mediator->set_delegate(delegate);

  // No calls to delegate() are expected, and OCMStrictProtocolMock will fail
  // this test if [delegate() dismissAuthenticationSelection] is called.
  controller()->DismissDialogUponServerProcessedAuthenticationMethodRequest(
      /*server_success=*/true);
}
