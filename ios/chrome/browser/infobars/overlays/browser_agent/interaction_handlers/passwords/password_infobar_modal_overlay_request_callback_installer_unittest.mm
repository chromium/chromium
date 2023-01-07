// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_modal_overlay_request_callback_installer.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/test/mock_password_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_infobar_modal_responses::UpdateCredentialsInfo;
using password_infobar_modal_responses::NeverSaveCredentials;
using password_infobar_modal_responses::PresentPasswordSettings;

// Test fixture for PasswordInfobarModalOverlayRequestCallbackInstaller.
class PasswordInfobarModalOverlayRequestCallbackInstallerTest
    : public PlatformTest {
 public:
  PasswordInfobarModalOverlayRequestCallbackInstallerTest()
      : installer_(&mock_handler_, password_modal::PasswordAction::kSave),
        update_installer_(&mock_handler_,
                          password_modal::PasswordAction::kUpdate) {
    // Create the infobar and add it to the WebState's manager.
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    std::unique_ptr<InfoBarIOS> added_infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypePasswordSave,
        MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username",
                                                         @"password"));
    infobar_ = added_infobar.get();
    manager()->AddInfoBar(std::move(added_infobar));
    // Create the request and add it to the WebState's queue.
    std::unique_ptr<OverlayRequest> added_request =
        OverlayRequest::CreateWithConfig<
            PasswordInfobarModalOverlayRequestConfig>(infobar_);
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
  web::FakeWebState web_state_;
  InfoBarIOS* infobar_ = nullptr;
  OverlayRequest* request_ = nullptr;
  MockPasswordInfobarModalInteractionHandler mock_handler_;
  PasswordInfobarModalOverlayRequestCallbackInstaller installer_;
  PasswordInfobarModalOverlayRequestCallbackInstaller update_installer_;
};

// Tests that a dispatched InfobarBannerMainActionResponse calls
// PasswordInfobarModalInteractionHandler::MainButtonTapped().
TEST_F(PasswordInfobarModalOverlayRequestCallbackInstallerTest, MainAction) {
  EXPECT_CALL(mock_handler_, PerformMainAction(infobar_));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<InfobarModalMainActionResponse>());
}

// Tests that a dispatched UpdateCredentialsInfo responses calls
// PasswordInfobarModalInteractionHandler::UpdateCredentials().
TEST_F(PasswordInfobarModalOverlayRequestCallbackInstallerTest,
       UpdateCredentials) {
  NSString* username = @"update_username";
  NSString* password = @"update_password";
  EXPECT_CALL(mock_handler_, UpdateCredentials(infobar_, username, password));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<UpdateCredentialsInfo>(username,
                                                             password));
}

// Tests the flow for the NeverSaveCredentials response.
TEST_F(PasswordInfobarModalOverlayRequestCallbackInstallerTest,
       NeverSaveCredentials) {
  // Send an NeverSaveCredentials response, expecting the interaction handler
  // update to be called.
  EXPECT_CALL(mock_handler_, NeverSaveCredentials(infobar_));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<NeverSaveCredentials>());

  // When the installer handles the NeverSaveCredentials response, it adds a
  // completion callback to the request that removes its infobar from the
  // manager so that the badge/infobar is no longer shown for this page.  Cancel
  // the request to trigger this completion callback and verify that the infobar
  // is successfully removed from its manager.
  queue()->CancelAllRequests();
  EXPECT_EQ(0U, manager()->infobar_count());
}

// Tests the flow for the PresentPasswordSettings response.
TEST_F(PasswordInfobarModalOverlayRequestCallbackInstallerTest,
       PresentPasswordSettings) {
  // Dispatch a PresentPasswordSettings response.
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<PresentPasswordSettings>());

  // When the installer handles the PresentPasswordSettings response, it adds a
  // completion callback to the request that instructs the interaction handler
  // to present settings when the dismissal finishes.  Cancel the request to
  // trigger this completion callback and verify that the interaction handler's
  // PresentPasswordSettings() was called.
  EXPECT_CALL(mock_handler_, PresentPasswordsSettings(infobar_));
  queue()->CancelAllRequests();
}

// Tests that dispatch responses for a save password RequestConfig do not cause
// the update callback installer to call it's interaction handler.
TEST_F(PasswordInfobarModalOverlayRequestCallbackInstallerTest, SaveNotUpdate) {
  update_installer_.InstallCallbacks(request_);
  // Dispatch a PresentPasswordSettings response.
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<PresentPasswordSettings>());

  // When the installer handles the PresentPasswordSettings response, it adds a
  // completion callback to the request that instructs the interaction handler
  // to present settings when the dismissal finishes.  Cancel the request to
  // trigger this completion callback and verify that the interaction handler's
  // PresentPasswordSettings() was called.
  EXPECT_CALL(mock_handler_, PresentPasswordsSettings(infobar_)).Times(1);
  queue()->CancelAllRequests();
}
