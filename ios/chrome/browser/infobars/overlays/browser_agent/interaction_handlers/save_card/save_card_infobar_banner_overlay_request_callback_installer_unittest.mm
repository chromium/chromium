// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_banner_overlay_request_callback_installer.h"

#import "base/guid.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_save_card_banner_infobar_interaction_handler.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_card_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_card_infobar_overlays::SaveCardBannerRequestConfig;

// Test fixture for SaveCardInfobarBannerOverlayRequestCallbackInstaller.
class SaveCardInfobarBannerOverlayRequestCallbackInstallerTest
    : public PlatformTest {
 public:
  SaveCardInfobarBannerOverlayRequestCallbackInstallerTest()
      : card_(base::GenerateGUID(), "https://www.example.com/"),
        installer_(&mock_handler_),
        delegate_factory_() {
    // Create the infobar and add it to the WebState's manager.
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile> delegate =
        delegate_factory_
            .CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(false,
                                                                    card_);
    delegate_ = delegate.get();
    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeTranslate, std::move(delegate));

    infobar_ = infobar.get();
    manager()->AddInfoBar(std::move(infobar));
    // Create the request and add it to the WebState's queue.
    std::unique_ptr<OverlayRequest> added_request =
        OverlayRequest::CreateWithConfig<SaveCardBannerRequestConfig>(infobar_);
    request_ = added_request.get();
    queue()->AddRequest(std::move(added_request));
    // Install the callbacks on the added request.
    installer_.InstallCallbacks(request_);
  }

  InfoBarManagerImpl* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }
  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarModal);
  }

 protected:
  autofill::CreditCard card_;
  web::FakeWebState web_state_;
  InfoBarIOS* infobar_ = nullptr;
  OverlayRequest* request_ = nullptr;
  MockSaveCardInfobarBannerInteractionHandler mock_handler_;
  SaveCardInfobarBannerOverlayRequestCallbackInstaller installer_;
  MockAutofillSaveCardInfoBarDelegateMobileFactory delegate_factory_;
  MockAutofillSaveCardInfoBarDelegateMobile* delegate_;
};

TEST_F(SaveCardInfobarBannerOverlayRequestCallbackInstallerTest,
       SaveCardCredentials) {
  NSString* cardholder_name = @"test name";
  NSString* expiration_date_month = @"06";
  NSString* expiration_date_year = @"2023";
  EXPECT_CALL(
      mock_handler_,
      SaveCredentials(infobar_, base::SysNSStringToUTF16(cardholder_name),
                      base::SysNSStringToUTF16(expiration_date_month),
                      base::SysNSStringToUTF16(expiration_date_year)));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<
          save_card_infobar_overlays::SaveCardMainAction>(
          cardholder_name, expiration_date_month, expiration_date_year));
}
