// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/sync_error/sync_error_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/ui/settings/sync/utils/test/mock_sync_error_infobar_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ChromeBrowserState;
@protocol SyncPresenter;

// Test fixture for SyncErrorInfobarBannerInteractionHandler.
class SyncErrorInfobarBannerInteractionHandlerTest : public PlatformTest {
 public:
  SyncErrorInfobarBannerInteractionHandlerTest() {
    TestChromeBrowserState::Builder builder;
    chrome_browser_state_ = builder.Build();

    id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSyncError,
        std::make_unique<MockSyncErrorInfoBarDelegate>(
            chrome_browser_state_.get(), presenter));
  }

  MockSyncErrorInfoBarDelegate& mock_delegate() {
    return *static_cast<MockSyncErrorInfoBarDelegate*>(
        infobar_->delegate()->AsConfirmInfoBarDelegate());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;

  std::unique_ptr<InfoBarIOS> infobar_;
};

// Tests MainButtonTapped() calls Accept() on the mock delegate which returns
// false.
TEST_F(SyncErrorInfobarBannerInteractionHandlerTest, MainButton) {
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(false));
  SyncErrorInfobarBannerInteractionHandler handler;
  handler.MainButtonTapped(infobar_.get());
}

// Tests that BannerVisibilityChanged() calls InfobarDismissed() on the mock
// delegate when the visibility becomes false.
TEST_F(SyncErrorInfobarBannerInteractionHandlerTest, Presentation) {
  EXPECT_CALL(mock_delegate(), InfoBarDismissed());
  SyncErrorInfobarBannerInteractionHandler handler;
  handler.BannerVisibilityChanged(infobar_.get(), false);
}
