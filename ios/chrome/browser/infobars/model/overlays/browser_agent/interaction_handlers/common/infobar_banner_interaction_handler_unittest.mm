// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/passwords/model/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

// Test fixture for InfobarBannerInteractionHandler.
class InfobarBannerInteractionHandlerTest : public PlatformTest {
 public:
  InfobarBannerInteractionHandlerTest()
      : handler_(DefaultInfobarOverlayRequestConfig::RequestSupport()) {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    OverlayRequestQueue::CreateForWebState(&web_state_);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &DefaultInfobarOverlayRequestFactory);

    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypePasswordSave,
        MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username",
                                                         @"password"));
    infobar_ = infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(infobar));
  }

  MockIOSChromeSavePasswordInfoBarDelegate& mock_delegate() {
    return *static_cast<MockIOSChromeSavePasswordInfoBarDelegate*>(
        infobar_->delegate());
  }

 protected:
  InfobarBannerInteractionHandler handler_;
  web::FakeWebState web_state_;
  raw_ptr<InfoBarIOS> infobar_;
};

// Tests that pressing the modal button calls adds an OverlayRequest for the
// modal UI to the WebState's queue at OverlayModality::kInfobarModal.
TEST_F(InfobarBannerInteractionHandlerTest, ShowModal) {
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kInfobarModal);
  ASSERT_EQ(0U, queue->size());
  handler_.ShowModalButtonTapped(infobar_, &web_state_);

  OverlayRequest* modal_request = queue->front_request();
  EXPECT_TRUE(modal_request);
  EXPECT_EQ(infobar_, GetOverlayRequestInfobar(modal_request));
}

// Tests that BannerVisibilityChanged() calls InfobarDismissed() on the mock
// delegate.
TEST_F(InfobarBannerInteractionHandlerTest, UserInitiatedDismissal) {
  EXPECT_CALL(mock_delegate(), InfoBarDismissed());
  handler_.BannerDismissedByUser(infobar_);
}
