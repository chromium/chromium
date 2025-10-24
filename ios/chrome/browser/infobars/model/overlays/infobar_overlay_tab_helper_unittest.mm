// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_tab_helper.h"

#import <Foundation/Foundation.h>

#import "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/fake_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_user_data.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using infobars::InfoBar;
using infobars::InfoBarDelegate;
using infobars::InfoBarManager;

// Test fixture for InfobarOverlayTabHelper.
class InfobarOverlayTabHelperTest : public PlatformTest {
 public:
  InfobarOverlayTabHelperTest() {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    OverlayRequestQueue::CreateForWebState(&web_state_);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &FakeInfobarOverlayRequestFactory);
    InfobarOverlayTabHelper::CreateForWebState(&web_state_);
  }

  // Returns the front request of `web_state_`'s OverlayRequestQueue.
  OverlayRequest* front_request() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarBanner)
        ->front_request();
  }
  InfoBarManager* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }

 private:
  web::FakeWebState web_state_;
};

// Tests that adding an InfoBar to the manager creates a fake banner request.
TEST_F(InfobarOverlayTabHelperTest, AddInfoBar) {
  ASSERT_FALSE(front_request());
  manager()->AddInfoBar(std::make_unique<FakeInfobarIOS>());
  ASSERT_TRUE(front_request());
}

TEST_F(InfobarOverlayTabHelperTest, HighPriorityInfoBar) {
  ASSERT_FALSE(front_request());
  manager()->AddInfoBar(std::make_unique<FakeInfobarIOS>());
  ASSERT_TRUE(front_request());

  std::unique_ptr<FakeInfobarIOS> high_priority_infobar =
      std::make_unique<FakeInfobarIOS>(InfobarType::kInfobarTypeTranslate,
                                       u"FakeTranslateInfobar");
  high_priority_infobar->set_high_priority(true);
  manager()->AddInfoBar(std::move(high_priority_infobar));
  OverlayRequest* request = front_request();
  InfobarOverlayRequestConfig* config =
      request->GetConfig<InfobarOverlayRequestConfig>();
  ASSERT_TRUE(config->is_high_priority());
}
