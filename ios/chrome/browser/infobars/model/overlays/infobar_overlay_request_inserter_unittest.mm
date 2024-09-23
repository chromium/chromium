// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"

#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/fake_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
// The two infobar message text used in tests.  Both support badges.
std::u16string kFirstInfobarMessageText = u"FakeInfobarDelegate1";
std::u16string kSecondInfobarMessageText = u"FakeInfobarDelegate2";
}

using infobars::InfoBar;
using infobars::InfoBarManager;

// Test fixture for InfobarOverlayRequestInserter.
class InfobarOverlayRequestInserterTest : public PlatformTest {
 public:
  InfobarOverlayRequestInserterTest() {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    OverlayRequestQueue::CreateForWebState(&web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &FakeInfobarOverlayRequestFactory);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
  }

  // Accessors.
  InfobarOverlayRequestInserter* inserter() {
    return InfobarOverlayRequestInserter::FromWebState(&web_state_);
  }
  InfoBarManager* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }
  OverlayRequestQueue* GetQueue(InfobarOverlayType type) {
    OverlayModality modality = type == InfobarOverlayType::kBanner
                                   ? OverlayModality::kInfobarBanner
                                   : OverlayModality::kInfobarModal;
    return OverlayRequestQueue::FromWebState(&web_state_, modality);
  }

  // Adds an InfoBar created with a test delegate to the manager.  Returns a
  // pointer to the added InfoBar.  If `message_text` matches an infobar already
  // added, then it the new one will be ignored.
  InfoBar* CreateInfobar(std::u16string message_text) {
    std::unique_ptr<InfoBar> added_infobar = std::make_unique<FakeInfobarIOS>(
        InfobarType::kInfobarTypeConfirm, message_text);
    InfoBar* infobar = added_infobar.get();
    manager()->AddInfoBar(std::move(added_infobar));
    return infobar;
  }

 private:
  web::FakeWebState web_state_;
};

// Tests that the inserter adds banner OverlayRequests to the correct queue.
TEST_F(InfobarOverlayRequestInserterTest, InsertBanner) {
  OverlayRequestQueue* queue = GetQueue(InfobarOverlayType::kBanner);
  ASSERT_EQ(0U, queue->size());
  // Insert `infobar` at front of queue and check that the queue is updated
  // correctly.
  InfoBar* infobar = CreateInfobar(kFirstInfobarMessageText);
  InsertParams params(static_cast<InfoBarIOS*>(infobar));
  params.overlay_type = InfobarOverlayType::kBanner;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kInfoBarManager;
  inserter()->InsertOverlayRequest(params);
  EXPECT_EQ(1U, queue->size());
  EXPECT_EQ(infobar, queue->front_request()
                         ->GetConfig<InfobarOverlayRequestConfig>()
                         ->infobar());
  // Insert `inserted_infobar` in front of `infobar` and check that it is now
  // the front request.
  InfoBar* inserted_infobar = CreateInfobar(kSecondInfobarMessageText);
  params.infobar = static_cast<InfoBarIOS*>(inserted_infobar);
  inserter()->InsertOverlayRequest(params);
  EXPECT_EQ(2U, queue->size());
  EXPECT_EQ(inserted_infobar, queue->front_request()
                                  ->GetConfig<InfobarOverlayRequestConfig>()
                                  ->infobar());
}

// Tests that the inserter adds banner OverlayRequests to the correct queue.
TEST_F(InfobarOverlayRequestInserterTest, AddBanner) {
  OverlayRequestQueue* queue = GetQueue(InfobarOverlayType::kBanner);
  ASSERT_EQ(0U, queue->size());
  // Add `infobar` to the back of the queue and check that the it is updated
  // correctly.
  InfoBar* infobar = CreateInfobar(kFirstInfobarMessageText);
  InsertParams params(static_cast<InfoBarIOS*>(infobar));
  params.overlay_type = InfobarOverlayType::kBanner;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kInfoBarManager;
  inserter()->InsertOverlayRequest(params);
  EXPECT_EQ(1U, queue->size());
  EXPECT_EQ(infobar, queue->front_request()
                         ->GetConfig<InfobarOverlayRequestConfig>()
                         ->infobar());
  // Add `second_infobar` in to the queue and check that it is second in the
  // queue.
  InfoBar* second_infobar = CreateInfobar(kSecondInfobarMessageText);
  params.infobar = static_cast<InfoBarIOS*>(second_infobar);
  params.insertion_index = 1;
  inserter()->InsertOverlayRequest(params);
  EXPECT_EQ(2U, queue->size());
  EXPECT_EQ(second_infobar, queue->GetRequest(1)
                                ->GetConfig<InfobarOverlayRequestConfig>()
                                ->infobar());
}
