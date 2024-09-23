// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_modal_overlay_request_cancel_handler.h"

#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/fake_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_user_data.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

using infobars::InfoBarDelegate;
using infobars::InfoBarManager;

// Test fixture for InfobarModalOverlayRequestCancelHandler.
class InfobarModalOverlayRequestCancelHandlerTest : public PlatformTest {
 public:
  InfobarModalOverlayRequestCancelHandlerTest() {
    // Set up WebState and InfoBarManager.
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    OverlayRequestQueue::CreateForWebState(&web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &FakeInfobarOverlayRequestFactory);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
  }

  // Returns the banner queue.
  OverlayRequestQueue* banner_queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarBanner);
  }
  InfoBarManager* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }
  InfobarOverlayRequestInserter* inserter() {
    return InfobarOverlayRequestInserter::FromWebState(&web_state_);
  }

  // Returns the InfoBar used to create the front request in queue().
  InfoBarIOS* GetFrontRequestInfobar() {
    OverlayRequest* front_request = banner_queue()->front_request();
    return front_request ? GetOverlayRequestInfobar(front_request) : nullptr;
  }

 protected:
  web::FakeWebState web_state_;
};

// Tests that the request is cancelled after all modals originating from the
// banner have been completed.
TEST_F(InfobarModalOverlayRequestCancelHandlerTest, CancelForModalCompletion) {
  // Create an InfoBarIOS, add it to the InfoBarManager, and insert a modal
  // request.
  std::unique_ptr<InfoBarIOS> passed_infobar =
      std::make_unique<FakeInfobarIOS>();
  InfoBarIOS* infobar = passed_infobar.get();
  manager()->AddInfoBar(std::move(passed_infobar));
  InsertParams params(infobar);
  params.overlay_type = InfobarOverlayType::kModal;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kBanner;
  inserter()->InsertOverlayRequest(params);
  ASSERT_EQ(infobar, GetFrontRequestInfobar());
  // Cancel request and assert the banner placeholder has been removed.
  OverlayRequestQueue::FromWebState(&web_state_, OverlayModality::kInfobarModal)
      ->CancelAllRequests();
  EXPECT_FALSE(banner_queue()->front_request());
}
