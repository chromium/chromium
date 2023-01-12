// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/autofill_address_profile/save_address_profile_infobar_banner_overlay_mediator.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/guid.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileBannerRequestConfig;

// Test fixture for SaveAddressProfileInfobarBannerOverlayMediator.
class SaveAddressProfileInfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  SaveAddressProfileInfobarBannerOverlayMediatorTest()
      : callback_installer_(
            &callback_receiver_,
            {InfobarBannerShowModalResponse::ResponseSupport()}) {}

 protected:
  MockOverlayRequestCallbackReceiver callback_receiver_;
  FakeOverlayRequestCallbackInstaller callback_installer_;
};

// Tests that a SaveAddressProfileInfobarBannerOverlayMediator correctly sets up
// its consumer.
TEST_F(SaveAddressProfileInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  autofill::AutofillProfile profile(base::GenerateGUID(),
                                    "https://www.example.com/");
  std::unique_ptr<autofill::AutofillSaveUpdateAddressProfileDelegateIOS>
      passed_delegate = std::make_unique<
          autofill::AutofillSaveUpdateAddressProfileDelegateIOS>(
          profile, /*original_profile=*/nullptr, /*locale=*/"en-US",
          base::DoNothing());
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      passed_delegate.get();
  InfoBarIOS infobar(InfobarType::kInfobarTypeSaveAutofillAddressProfile,
                     std::move(passed_delegate));

  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<SaveAddressProfileBannerRequestConfig>(
          &infobar);
  SaveAddressProfileInfobarBannerOverlayMediator* mediator =
      [[SaveAddressProfileInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate->GetMessageText()),
              consumer.titleText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate->GetMessageActionText()),
              consumer.buttonText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate->GetDescription()),
              consumer.subtitleText);
  if (UseSymbols()) {
    EXPECT_NSEQ(
        DefaultSymbolWithPointSize(kPinFillSymbol, kInfobarSymbolPointSize),
        consumer.iconImage);
  } else {
    EXPECT_NSEQ([UIImage imageNamed:@"ic_place"], consumer.iconImage);
  }
}

// Tests that the modal is shown when infobar button is pressed.
TEST_F(SaveAddressProfileInfobarBannerOverlayMediatorTest,
       PresentModalWhenInfobarButtonIsPressed) {
  autofill::AutofillProfile profile(base::GenerateGUID(),
                                    "https://www.example.com/");
  std::unique_ptr<autofill::AutofillSaveUpdateAddressProfileDelegateIOS>
      passed_delegate = std::make_unique<
          autofill::AutofillSaveUpdateAddressProfileDelegateIOS>(
          profile, /*original_profile=*/nullptr, /*locale=*/"en-US",
          base::DoNothing());
  InfoBarIOS infobar(InfobarType::kInfobarTypeSaveAutofillAddressProfile,
                     std::move(passed_delegate));

  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<SaveAddressProfileBannerRequestConfig>(
          &infobar);
  callback_installer_.InstallCallbacks(request.get());
  SaveAddressProfileInfobarBannerOverlayMediator* mediator =
      [[SaveAddressProfileInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];

  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request.get(),
                       InfobarBannerShowModalResponse::ResponseSupport()));
  [mediator bannerInfobarButtonWasPressed:nil];
}
