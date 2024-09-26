// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/sync_error/sync_error_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/settings/model/sync/utils/test/mock_sync_error_infobar_delegate.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@protocol SyncPresenter;

// Test fixture for SyncErrorInfobarBannerInteractionHandler.
class SyncErrorInfobarBannerInteractionHandlerTest : public PlatformTest {
 public:
  SyncErrorInfobarBannerInteractionHandlerTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSyncError,
        std::make_unique<MockSyncErrorInfoBarDelegate>(profile_.get(),
                                                       presenter));
  }

  MockSyncErrorInfoBarDelegate& mock_delegate() {
    return *static_cast<MockSyncErrorInfoBarDelegate*>(
        infobar_->delegate()->AsConfirmInfoBarDelegate());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;

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
