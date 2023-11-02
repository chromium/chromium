// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_overlay_request_callback_installer.h"

#import "base/guid.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_update_address_profile_delegate_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_save_address_profile_modal_infobar_interaction_handler.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_responses.h"
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

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;
using save_address_profile_infobar_modal_responses::EditedProfileSaveAction;
using save_address_profile_infobar_modal_responses::CancelViewAction;

// Test fixture for
// SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller.
class SaveAddressProfileInfobarModalOverlayRequestCallbackInstallerTest
    : public PlatformTest {
 public:
  SaveAddressProfileInfobarModalOverlayRequestCallbackInstallerTest()
      : profile_(base::GenerateGUID(), "https://www.example.com/"),
        installer_(&mock_handler_),
        delegate_factory_() {
    // Create the infobar and add it to the WebState's manager.
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    std::unique_ptr<MockAutofillSaveUpdateAddressProfileDelegateIOS> delegate =
        delegate_factory_
            .CreateMockAutofillSaveUpdateAddressProfileDelegateIOSFactory(
                profile_);
    delegate_ = delegate.get();
    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSaveAutofillAddressProfile,
        std::move(delegate));

    infobar_ = infobar.get();
    manager()->AddInfoBar(std::move(infobar));
    // Create the request and add it to the WebState's queue.
    std::unique_ptr<OverlayRequest> added_request =
        OverlayRequest::CreateWithConfig<SaveAddressProfileModalRequestConfig>(
            infobar_);
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
  autofill::AutofillProfile profile_;
  web::FakeWebState web_state_;
  InfoBarIOS* infobar_ = nullptr;
  OverlayRequest* request_ = nullptr;
  MockSaveAddressProfileInfobarModalInteractionHandler mock_handler_;
  SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller installer_;
  MockAutofillSaveUpdateAddressProfileDelegateIOSFactory delegate_factory_;
  MockAutofillSaveUpdateAddressProfileDelegateIOS* delegate_;
};

TEST_F(SaveAddressProfileInfobarModalOverlayRequestCallbackInstallerTest,
       SaveEditedProfile) {
  NSDictionary* empty = @{}.mutableCopy;
  EXPECT_CALL(mock_handler_, SaveEditedProfile(infobar_, empty));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<EditedProfileSaveAction>(empty));
}

TEST_F(SaveAddressProfileInfobarModalOverlayRequestCallbackInstallerTest,
       CancelAction) {
  BOOL fakeFromEditModal = NO;
  EXPECT_CALL(mock_handler_, CancelModal(infobar_, fakeFromEditModal));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<CancelViewAction>(fakeFromEditModal));
}
