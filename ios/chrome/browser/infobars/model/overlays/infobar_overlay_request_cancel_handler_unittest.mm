// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_cancel_handler.h"

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

using infobars::InfoBar;
using infobars::InfoBarDelegate;
using infobars::InfoBarManager;

// Test fixture for InfobarOverlayRequestCancelHandler.
class InfobarOverlayRequestCancelHandlerTest : public PlatformTest {
 public:
  InfobarOverlayRequestCancelHandlerTest() {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    OverlayRequestQueue::CreateForWebState(&web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &FakeInfobarOverlayRequestFactory);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
  }

  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarModal);
  }
  InfoBarManager* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }
  InfobarOverlayRequestInserter* inserter() {
    return InfobarOverlayRequestInserter::FromWebState(&web_state_);
  }

  // Returns the InfoBar used to create the front request in queue().
  InfoBar* GetFrontRequestInfobar() {
    OverlayRequest* front_request = queue()->front_request();
    return front_request ? GetOverlayRequestInfobar(front_request) : nullptr;
  }

 private:
  web::FakeWebState web_state_;
};

// Tests that the request is cancelled when its corresponding InfoBar is removed
// from its InfoBarManager.
TEST_F(InfobarOverlayRequestCancelHandlerTest, CancelForInfobarRemoval) {
  std::unique_ptr<InfoBar> added_infobar = std::make_unique<FakeInfobarIOS>();
  InfoBar* infobar = added_infobar.get();
  manager()->AddInfoBar(std::move(added_infobar));
  InsertParams params(static_cast<InfoBarIOS*>(infobar));
  params.overlay_type = InfobarOverlayType::kModal;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kBanner;
  inserter()->InsertOverlayRequest(params);
  ASSERT_EQ(infobar, GetFrontRequestInfobar());
  // Remove the InfoBar from its manager and verify that the request has been
  // removed from the queue.
  manager()->RemoveInfoBar(infobar);
  EXPECT_FALSE(queue()->front_request());
}

// Tests that the request is cancelled if its corresponding InfoBar is replaced
// in its manager.
TEST_F(InfobarOverlayRequestCancelHandlerTest, CancelForInfobarReplacement) {
  std::unique_ptr<InfoBar> first_passed_infobar =
      std::make_unique<FakeInfobarIOS>();
  InfoBar* first_infobar = first_passed_infobar.get();
  manager()->AddInfoBar(std::move(first_passed_infobar));
  InsertParams params(static_cast<InfoBarIOS*>(first_infobar));
  params.overlay_type = InfobarOverlayType::kModal;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kBanner;
  inserter()->InsertOverlayRequest(params);
  ASSERT_EQ(first_infobar, GetFrontRequestInfobar());
  // Replace with a new infobar and verify that the request has been cancelled.
  manager()->ReplaceInfoBar(first_infobar, std::make_unique<FakeInfobarIOS>());
  EXPECT_FALSE(queue()->front_request());
}
