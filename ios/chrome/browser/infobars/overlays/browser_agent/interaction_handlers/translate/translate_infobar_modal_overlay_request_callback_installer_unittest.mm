// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_modal_overlay_request_callback_installer.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_translate_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/translate_overlay_tab_helper.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/translate/fake_translate_infobar_delegate.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for TranslateInfobarModalOverlayRequestCallbackInstaller.
class TranslateInfobarModalOverlayRequestCallbackInstallerTest
    : public PlatformTest {
 public:
  TranslateInfobarModalOverlayRequestCallbackInstallerTest()
      : installer_(&mock_handler_) {
    // Create the infobar and add it to the WebState's manager.
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    TranslateOverlayTabHelper::CreateForWebState(&web_state_);
    std::unique_ptr<FakeTranslateInfoBarDelegate> delegate =
        delegate_factory_.CreateFakeTranslateInfoBarDelegate("fr", "en");
    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeTranslate, std::move(delegate));

    infobar_ = infobar.get();
    manager()->AddInfoBar(std::move(infobar));
    // Create the request and add it to the WebState's queue.
    std::unique_ptr<OverlayRequest> added_request =
        OverlayRequest::CreateWithConfig<
            translate_infobar_overlays::TranslateModalRequestConfig>(infobar_);
    request_ = added_request.get();
    queue()->AddRequest(std::move(added_request));
    // Install the callbacks on the added request.
    installer_.InstallCallbacks(request_);
  }

  ~TranslateInfobarModalOverlayRequestCallbackInstallerTest() override {
    manager()->ShutDown();
  }

  InfoBarManagerImpl* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }
  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarModal);
  }

 protected:
  web::FakeWebState web_state_;
  InfoBarIOS* infobar_ = nullptr;
  OverlayRequest* request_ = nullptr;
  MockTranslateInfobarModalInteractionHandler mock_handler_;
  translate_infobar_overlay::ModalRequestCallbackInstaller installer_;
  FakeTranslateInfoBarDelegateFactory delegate_factory_;
};

TEST_F(TranslateInfobarModalOverlayRequestCallbackInstallerTest,
       UpdateLanguages) {
  int source_language_index = 0;
  int target_language_index = 1;
  // Just assert that the methods are called. The actual codes are unecessary to
  // mock since it is dependent on the Translate model.
  EXPECT_CALL(mock_handler_, UpdateLanguages(infobar_, source_language_index,
                                             target_language_index));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<
          translate_infobar_modal_responses::UpdateLanguageInfo>(
          source_language_index, target_language_index));
}

TEST_F(TranslateInfobarModalOverlayRequestCallbackInstallerTest,
       ToggleAlwaysTranslate) {
  EXPECT_CALL(mock_handler_, ToggleAlwaysTranslate(infobar_));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<
          translate_infobar_modal_responses::ToggleAlwaysTranslate>());
}

TEST_F(TranslateInfobarModalOverlayRequestCallbackInstallerTest,
       RevertTranslation) {
  EXPECT_CALL(mock_handler_, RevertTranslation(infobar_));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<
          translate_infobar_modal_responses::RevertTranslation>());
}

TEST_F(TranslateInfobarModalOverlayRequestCallbackInstallerTest,
       ToggleNeverTranslateSite) {
  EXPECT_CALL(mock_handler_, ToggleNeverTranslateSite(infobar_));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<
          translate_infobar_modal_responses::ToggleNeverPromptSite>());
}

TEST_F(TranslateInfobarModalOverlayRequestCallbackInstallerTest,
       ToggleNeverTranslateLanguage) {
  EXPECT_CALL(mock_handler_, ToggleNeverTranslateLanguage(infobar_));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<
          translate_infobar_modal_responses::
              ToggleNeverTranslateSourceLanguage>());
}

TEST_F(TranslateInfobarModalOverlayRequestCallbackInstallerTest,
       PerformMainAction) {
  EXPECT_CALL(mock_handler_, PerformMainAction(infobar_));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<InfobarModalMainActionResponse>());
}
