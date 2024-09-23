// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_type_util.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_modal/save_address_profile_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_edit_address_profile_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/test/fake_infobar_save_address_profile_modal_consumer.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;
using save_address_profile_infobar_modal_responses::CancelViewAction;
using save_address_profile_infobar_modal_responses::EditedProfileSaveAction;
using save_address_profile_infobar_modal_responses::NoThanksViewAction;

// Test fixture for SaveAddressProfileInfobarModalOverlayMediator.
class SaveAddressProfileInfobarModalOverlayMediatorTest : public PlatformTest {
 public:
  SaveAddressProfileInfobarModalOverlayMediatorTest()
      : callback_installer_(&callback_receiver_,
                            {EditedProfileSaveAction::ResponseSupport(),
                             CancelViewAction::ResponseSupport(),
                             NoThanksViewAction::ResponseSupport()}),
        mediator_delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))) {
    autofill::AutofillProfile profile = autofill::test::GetFullProfile();
    std::unique_ptr<autofill::AutofillSaveUpdateAddressProfileDelegateIOS>
        delegate = std::make_unique<
            autofill::AutofillSaveUpdateAddressProfileDelegateIOS>(
            profile, /*original_profile=*/nullptr,
            /*user_email=*/std::nullopt,
            /*locale=*/"en-US",
            /*is_migration_to_account=*/false, base::DoNothing());
    delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSaveAutofillAddressProfile,
        std::move(delegate));

    request_ =
        OverlayRequest::CreateWithConfig<SaveAddressProfileModalRequestConfig>(
            infobar_.get());
    callback_installer_.InstallCallbacks(request_.get());

    mediator_ = [[SaveAddressProfileInfobarModalOverlayMediator alloc]
        initWithRequest:request_.get()];
    mediator_.delegate = mediator_delegate_;
  }

  ~SaveAddressProfileInfobarModalOverlayMediatorTest() override {
    EXPECT_CALL(callback_receiver_, CompletionCallback(request_.get()));
    EXPECT_OCMOCK_VERIFY(mediator_delegate_);
  }

 protected:
  raw_ptr<autofill::AutofillSaveUpdateAddressProfileDelegateIOS> delegate_;
  std::unique_ptr<InfoBarIOS> infobar_;
  MockOverlayRequestCallbackReceiver callback_receiver_;
  FakeOverlayRequestCallbackInstaller callback_installer_;
  std::unique_ptr<OverlayRequest> request_;
  SaveAddressProfileInfobarModalOverlayMediator* mediator_ = nil;
  id<OverlayRequestMediatorDelegate> mediator_delegate_ = nil;
};

// Tests that a SaveAddressProfileInfobarModalOverlayMediator correctly sets up
// its consumer.
TEST_F(SaveAddressProfileInfobarModalOverlayMediatorTest, SetUpConsumer) {
  FakeInfobarSaveAddressProfileModalConsumer* consumer =
      [[FakeInfobarSaveAddressProfileModalConsumer alloc] init];
  mediator_.consumer = consumer;
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->GetEnvelopeStyleAddress()),
              consumer.address);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->GetPhoneNumber()),
              consumer.phoneNumber);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->GetEmailAddress()),
              consumer.emailAddress);
  EXPECT_FALSE(consumer.currentAddressProfileSaved);
  EXPECT_FALSE(consumer.isUpdateModal);
  EXPECT_EQ(0U, [consumer.profileDataDiff count]);
  EXPECT_NSEQ(@"", consumer.updateModalDescription);
}

// Tests that calling saveEditedProfileWithProfileData: triggers a
// EditedProfileSaveAction response.
TEST_F(SaveAddressProfileInfobarModalOverlayMediatorTest, EditAction) {
  autofill::AutofillProfile profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  EXPECT_CALL(callback_receiver_,
              DispatchCallback(request_.get(),
                               EditedProfileSaveAction::ResponseSupport()));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ saveEditedProfileWithProfileData:&profile];
}

// Tests that calling dismissInfobarModal triggers a CancelViewAction response.
TEST_F(SaveAddressProfileInfobarModalOverlayMediatorTest, CancelAction) {
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request_.get(), CancelViewAction::ResponseSupport()));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissInfobarModal:nil];
}

// Tests that calling noThanksButtonWasPressed triggers a NoThanksViewAction
// response.
TEST_F(SaveAddressProfileInfobarModalOverlayMediatorTest, NoThanksViewAction) {
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request_.get(), NoThanksViewAction::ResponseSupport()));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ noThanksButtonWasPressed];
}
