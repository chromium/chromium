// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/contextual_cue/contextual_cue_infobar_banner_overlay_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/infobars/core/infobar.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cue_infobar_delegate.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

std::unique_ptr<KeyedService> BuildFakeGeminiService(ProfileIOS* profile) {
  auto fake_service = std::make_unique<FakeGeminiService>();
  fake_service->SetIsEligible(true);
  return fake_service;
}

std::unique_ptr<KeyedService> CreateTestTracker(ProfileIOS* context) {
  auto tracker = std::make_unique<feature_engagement::test::MockTracker>();
  ON_CALL(*tracker, ShouldTriggerHelpUI(testing::_))
      .WillByDefault(testing::Return(true));
  return tracker;
}

}  // namespace

// Test fixture for ContextualCueInfobarBannerOverlayMediator.
class ContextualCueInfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(GeminiServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildFakeGeminiService));
    builder.AddTestingFactory(feature_engagement::TrackerFactory::GetInstance(),
                              base::BindRepeating(&CreateTestTracker));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();

    auto* sync_service = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
    sync_service->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/true, syncer::UserSelectableTypeSet::All());

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state_->SetCurrentURL(GURL("https://example.com/page"));

    InfoBarManagerImpl::CreateForWebState(web_state_.get());

    mock_gemini_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
    mock_settings_handler_ = OCMStrictProtocolMock(@protocol(SettingsCommands));

    contextual_cueing::ContextualCueInfobarConfig config{
        .title = u"Page Insights",
        .button_text = u"Explore",
        .prompt = "Summarize this page",
    };
    auto delegate =
        std::make_unique<contextual_cueing::ContextualCueInfobarDelegate>(
            web_state_.get(), mock_gemini_handler_, std::move(config));
    delegate_ = delegate.get();

    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeContextualCue, std::move(delegate));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kBanner);
    consumer_ = [[FakeInfobarBannerConsumer alloc] init];
    mediator_ = [[ContextualCueInfobarBannerOverlayMediator alloc]
        initWithRequest:request_.get()];
    mediator_.consumer = consumer_;
    mediator_.settingsHandler = mock_settings_handler_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    consumer_ = nil;
    request_.reset();
    delegate_ = nullptr;
    infobar_.reset();
    mock_settings_handler_ = nil;
    mock_gemini_handler_ = nil;
    web_state_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<InfoBarIOS> infobar_;
  raw_ptr<contextual_cueing::ContextualCueInfobarDelegate> delegate_ = nullptr;
  std::unique_ptr<OverlayRequest> request_;
  FakeInfobarBannerConsumer* consumer_ = nil;
  ContextualCueInfobarBannerOverlayMediator* mediator_ = nil;
  id mock_gemini_handler_;
  id mock_settings_handler_;
};

// Test that the consumer is correctly set up from the contextual cue delegate.
TEST_F(ContextualCueInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->GetTitleText()),
              consumer_.titleText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK)),
              consumer_.buttonText);
  EXPECT_TRUE(consumer_.presentsModal);
  EXPECT_TRUE(consumer_.useIconBackgroundTint);
  EXPECT_NSEQ(delegate_->GetIcon().GetImage().ToUIImage(), consumer_.iconImage);
}

// Test that tapping the action button accepts the delegate and starts the
// Gemini entry flow.
TEST_F(ContextualCueInfobarBannerOverlayMediatorTest, ActionButtonAccepted) {
  OCMExpect([mock_gemini_handler_
      startGeminiEntryFlowWithStartupState:[OCMArg any]
                        baseViewController:nil
                  showSnackbarOnCompletion:YES
                                completion:nil]);

  [mediator_ bannerInfobarButtonWasPressed:nil];

  EXPECT_OCMOCK_VERIFY(mock_gemini_handler_);
}

// Test that presenting modal from banner triggers Gemini contextual cue
// settings.
TEST_F(ContextualCueInfobarBannerOverlayMediatorTest, PresentsSettings) {
  OCMExpect([mock_settings_handler_ showGeminiContextualCueSettings]);

  [mediator_ presentInfobarModalFromBanner];

  EXPECT_OCMOCK_VERIFY(mock_settings_handler_);
}
