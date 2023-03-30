// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_overlay_request_callback_installer.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/test/mock_password_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for PasswordInfobarBannerOverlayRequestCallbackInstaller.
class PasswordInfobarBannerOverlayRequestCallbackInstallerTest
    : public PlatformTest {
 public:
  PasswordInfobarBannerOverlayRequestCallbackInstallerTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(browser_state_.get())),
        mock_handler_(browser_.get(), password_modal::PasswordAction::kSave),
        save_installer_(&mock_handler_, password_modal::PasswordAction::kSave),
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
            PasswordInfobarBannerOverlayRequestConfig>(infobar_);
    request_ = added_request.get();
    queue()->AddRequest(std::move(added_request));
    // Install the callbacks on the added request.
    save_installer_.InstallCallbacks(request_);
    // Install the callbacks on an update password installer to verify that this
    // installer doesn't also call its interaction handler when the responses
    // are dispatched.
    update_installer_.InstallCallbacks(request_);
  }

  InfoBarManagerImpl* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }
  OverlayCallbackManager* callback_manager() const {
    return request_->GetCallbackManager();
  }
  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarModal);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  InfoBarIOS* infobar_ = nullptr;
  OverlayRequest* request_ = nullptr;
  MockPasswordInfobarBannerInteractionHandler mock_handler_;
  PasswordInfobarBannerOverlayRequestCallbackInstaller save_installer_;
  PasswordInfobarBannerOverlayRequestCallbackInstaller update_installer_;
};

// Tests that a dispatched InfobarBannerMainActionResponse for a save password
// request calls InfobarBannerInteractionHandler::MainButtonTapped(). Also
// verifies that InfobarBannerInteractionHandler::MainButtonTapped() is only
// called once, meaning that only the `save_installer_` called its interaction
// handler.
TEST_F(PasswordInfobarBannerOverlayRequestCallbackInstallerTest, MainAction) {
  EXPECT_CALL(mock_handler_, MainButtonTapped(infobar_)).Times(1);
  callback_manager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<InfobarBannerMainActionResponse>());
}

// Tests that a dispatched InfobarBannerShowModalResponse for a save password
// request calls InfobarBannerInteractionHandler::ShowModalButtonTapped(). Also
// verifies that InfobarBannerInteractionHandler::ShowModalButtonTapped() is
// only called once, meaning that only the `save_installer_` called its
// interaction handler.
TEST_F(PasswordInfobarBannerOverlayRequestCallbackInstallerTest, ShowModal) {
  EXPECT_CALL(mock_handler_, ShowModalButtonTapped(infobar_, &web_state_))
      .Times(1);
  callback_manager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<InfobarBannerShowModalResponse>());
}

// Tests that a dispatched InfobarBannerShowModalResponse for a save password
// request calls InfobarBannerInteractionHandler::BannerDismissedByUser(). Also
// verifies that InfobarBannerInteractionHandler::BannerDismissedByUser() is
// only called once, meaning that only the `save_installer_` called its
// interaction handler.
TEST_F(PasswordInfobarBannerOverlayRequestCallbackInstallerTest,
       UserInitiatedDismissal) {
  EXPECT_CALL(mock_handler_, BannerDismissedByUser(infobar_)).Times(1);
  callback_manager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<
          InfobarBannerUserInitiatedDismissalResponse>());
}
