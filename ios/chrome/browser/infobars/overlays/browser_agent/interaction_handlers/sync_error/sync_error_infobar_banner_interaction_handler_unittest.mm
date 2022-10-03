// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/sync_error/sync_error_infobar_banner_interaction_handler.h"

#import "components/sync/driver/sync_service_utils.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_error_infobar_delegate.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_presenter.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
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

namespace {
// Mock class of `SyncErrorInfoBarDelegate`.
class MockSyncErrorInfoBarDelegate : public SyncErrorInfoBarDelegate {
 public:
  MockSyncErrorInfoBarDelegate(ChromeBrowserState* browser_state,
                               id<SyncPresenter> presenter)
      : SyncErrorInfoBarDelegate(browser_state, presenter) {}

  MOCK_METHOD0(Accept, bool());
  MOCK_METHOD0(InfoBarDismissed, void());
};
}  // namespace

// Test fixture for SyncErrorInfobarBannerInteractionHandler.
class SyncErrorInfobarBannerInteractionHandlerTest : public PlatformTest {
 public:
  SyncErrorInfobarBannerInteractionHandlerTest() {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &DefaultInfobarOverlayRequestFactory);
    InfoBarManagerImpl::CreateForWebState(&web_state_);

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    chrome_browser_state_ = builder.Build();

    id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
    [[presenter expect] showReauthenticateSignin];

    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSyncError,
        std::make_unique<MockSyncErrorInfoBarDelegate>(
            chrome_browser_state_.get(), presenter));
    infobar_ = infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(infobar));
  }

  ~SyncErrorInfobarBannerInteractionHandlerTest() override {
    // Will de-register MockSyncErrorInfoBarDelegate and prevent crash in
    // `chrome_browser_state_.reset`
    InfoBarManagerImpl::FromWebState(&web_state_)->RemoveInfoBar(infobar_);
    chrome_browser_state_.reset();
  }

  MockSyncErrorInfoBarDelegate& mock_delegate() {
    return *static_cast<MockSyncErrorInfoBarDelegate*>(
        infobar_->delegate()->AsConfirmInfoBarDelegate());
  }

 protected:
  web::WebTaskEnvironment task_environment_;

  SyncErrorInfobarBannerInteractionHandler handler_;
  web::FakeWebState web_state_;
  InfoBarIOS* infobar_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests MainButtonTapped() calls Accept() on the mock delegate which returns
// false.
TEST_F(SyncErrorInfobarBannerInteractionHandlerTest, MainButton) {
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(false));
  handler_.MainButtonTapped(infobar_);
}

// Tests that BannerVisibilityChanged() calls InfobarDismissed() on the mock
// delegate when the visibility becomes false.
TEST_F(SyncErrorInfobarBannerInteractionHandlerTest, Presentation) {
  EXPECT_CALL(mock_delegate(), InfoBarDismissed());
  handler_.BannerVisibilityChanged(infobar_, false);
}
