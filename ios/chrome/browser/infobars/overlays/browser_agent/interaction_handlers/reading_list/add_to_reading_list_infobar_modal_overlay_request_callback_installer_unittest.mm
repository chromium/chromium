// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_infobar_modal_overlay_request_callback_installer.h"

#include "base/test/task_environment.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_ios_add_to_reading_list_infobar_delegate.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_reading_list_infobar_interaction_handler.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/reading_list_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/reading_list_modal_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/reading_list/fake_reading_list_model.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for TranslateInfobarModalOverlayRequestCallbackInstaller.
class ReadingListInfobarModalOverlayRequestCallbackInstallerTest
    : public PlatformTest {
 public:
  ReadingListInfobarModalOverlayRequestCallbackInstallerTest()
      : test_browser_(std::make_unique<TestBrowser>()),
        mock_handler_(test_browser_.get()),
        installer_(&mock_handler_) {
    // Create the infobar and add it to the WebState's manager.
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    fake_reading_list_model_ = std::make_unique<FakeReadingListModel>();
    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeAddToReadingList,
        std::make_unique<MockIOSAddToReadingListInfobarDelegate>(
            fake_reading_list_model_.get(), &web_state_));

    infobar_ = infobar.get();
    manager()->AddInfoBar(std::move(infobar));
    // Create the request and add it to the WebState's queue.
    std::unique_ptr<OverlayRequest> added_request =
        OverlayRequest::CreateWithConfig<
            ReadingListInfobarModalOverlayRequestConfig>(infobar_);
    request_ = added_request.get();
    queue()->AddRequest(std::move(added_request));
    // Install the callbacks on the added request.
    installer_.InstallCallbacks(request_);
  }

  void TearDown() override {
    manager()->ShutDown();
    PlatformTest::TearDown();
  }

  InfoBarManagerImpl* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }
  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarModal);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestBrowser> test_browser_;
  web::FakeWebState web_state_;
  std::unique_ptr<FakeReadingListModel> fake_reading_list_model_;
  InfoBarIOS* infobar_ = nullptr;
  OverlayRequest* request_ = nullptr;
  MockReadingListInfobarModalInteractionHandler mock_handler_;
  reading_list_infobar_overlay::ModalRequestCallbackInstaller installer_;
};

// Tests that dispatching the NeverAsk Overlay response calls the NeverAsk()
// InteractionHandler method.
TEST_F(ReadingListInfobarModalOverlayRequestCallbackInstallerTest, NeverAsk) {
  EXPECT_CALL(mock_handler_, NeverAsk(infobar_));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<
          reading_list_infobar_modal_responses::NeverAsk>());
}

// Tests that dispatching the InfobarModalMainActionResponse Overlay response
// calls the PerformMainAction() InteractionHandler method.
TEST_F(ReadingListInfobarModalOverlayRequestCallbackInstallerTest,
       PerformMainAction) {
  EXPECT_CALL(mock_handler_, PerformMainAction(infobar_));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<InfobarModalMainActionResponse>());
}
