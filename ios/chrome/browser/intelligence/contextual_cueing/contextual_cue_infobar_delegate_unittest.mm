// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cue_infobar_delegate.h"

#import <memory>

#import "base/strings/utf_string_conversions.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace contextual_cueing {

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

class ContextualCueInfobarDelegateTest : public PlatformTest {
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

    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state_->SetCurrentURL(GURL("https://example.com/page"));

    InfoBarManagerImpl::CreateForWebState(web_state_.get());
    ContextualCueingTabHelper::CreateForWebState(web_state_.get());

    mock_gemini_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
  }

  void TearDown() override {
    mock_gemini_handler_ = nil;
    mock_tracker_ = nullptr;
    web_state_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

 protected:
  void SetUpCue(const std::string& title,
                const std::string& chip_label,
                const std::string& action_button_label,
                const std::string& prompt) {
    optimization_guide::proto::ContextualCue cue;
    cue.set_suggested_cuj(title);
    auto* surface = cue.mutable_gemini_in_chrome_surface();
    surface->set_prompt(prompt);
    auto* message_cue = cue.mutable_anchored_message_cue();
    message_cue->set_anchored_message_text(chip_label);
    if (!action_button_label.empty()) {
      message_cue->set_action_text(action_button_label);
    }

    ContextualCueingTabHelper* tab_helper =
        ContextualCueingTabHelper::FromWebState(web_state_.get());
    ASSERT_TRUE(tab_helper);
    tab_helper->NotifyContextualCueReceived(cue);
  }

  infobars::InfoBarManager* GetInfoBarManager() {
    return InfoBarManagerImpl::FromWebState(web_state_.get());
  }

  ContextualCueInfobarDelegate* GetDelegate() {
    infobars::InfoBarManager* manager = GetInfoBarManager();
    if (!manager) {
      return nullptr;
    }
    for (infobars::InfoBar* infobar : manager->infobars()) {
      if (infobar->delegate()->GetIdentifier() ==
          infobars::InfoBarDelegate::CONTEXTUAL_CUE_INFOBAR_DELEGATE_IOS) {
        return static_cast<ContextualCueInfobarDelegate*>(infobar->delegate());
      }
    }
    return nullptr;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_ = nullptr;
  id mock_gemini_handler_;
};

// Test that Create adds an infobar to the manager and Remove clears it.
TEST_F(ContextualCueInfobarDelegateTest, TestCreateAndRemove) {
  SetUpCue("Summarize Page", "Summarize", "Summarize", "Summarize this page");

  ASSERT_EQ(GetInfoBarManager()->infobars().size(), 0u);
  EXPECT_TRUE(ContextualCueInfobarDelegate::Create(web_state_.get(),
                                                   mock_gemini_handler_));
  EXPECT_EQ(GetInfoBarManager()->infobars().size(), 1u);
  EXPECT_NE(GetDelegate(), nullptr);

  ContextualCueInfobarDelegate::Remove(web_state_.get());
  EXPECT_EQ(GetInfoBarManager()->infobars().size(), 0u);
}

// Test that delegate UI properties return the expected values.
TEST_F(ContextualCueInfobarDelegateTest, TestDelegateUIProperties) {
  SetUpCue("Summarize Page", "Summarize Chip", "Take Action",
           "Summarize this page");

  EXPECT_TRUE(ContextualCueInfobarDelegate::Create(web_state_.get(),
                                                   mock_gemini_handler_));
  ContextualCueInfobarDelegate* delegate = GetDelegate();
  ASSERT_NE(delegate, nullptr);

  EXPECT_EQ(delegate->GetIdentifier(),
            infobars::InfoBarDelegate::CONTEXTUAL_CUE_INFOBAR_DELEGATE_IOS);
  EXPECT_EQ(delegate->GetTitleText(), u"Summarize Page");
  EXPECT_EQ(delegate->GetMessageText(), u"Summarize Chip");
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            u"Take Action");
  EXPECT_FALSE(delegate->UseIconBackgroundTint());
  EXPECT_FALSE(delegate->GetIcon().IsEmpty());
}

// Test that if any required field is missing (empty), Create rejects infobar
// creation.
TEST_F(ContextualCueInfobarDelegateTest,
       TestMissingFieldsRejectsInfobarCreation) {
  // Empty action_button_label.
  SetUpCue("Explore Topic", "Explore message", /*action_button_label=*/"",
           "Explore this topic");
  EXPECT_FALSE(ContextualCueInfobarDelegate::Create(web_state_.get(),
                                                    mock_gemini_handler_));
  EXPECT_EQ(GetInfoBarManager()->infobars().size(), 0u);

  // Empty title.
  SetUpCue(/*title=*/"", "Explore message", "Explore", "Explore this topic");
  EXPECT_FALSE(ContextualCueInfobarDelegate::Create(web_state_.get(),
                                                    mock_gemini_handler_));
  EXPECT_EQ(GetInfoBarManager()->infobars().size(), 0u);

  // Empty message.
  SetUpCue("Explore Topic", /*chip_label=*/"", "Explore", "Explore this topic");
  EXPECT_FALSE(ContextualCueInfobarDelegate::Create(web_state_.get(),
                                                    mock_gemini_handler_));
  EXPECT_EQ(GetInfoBarManager()->infobars().size(), 0u);

  // Empty prompt.
  SetUpCue("Explore Topic", "Explore message", "Explore", /*prompt=*/"");
  EXPECT_FALSE(ContextualCueInfobarDelegate::Create(web_state_.get(),
                                                    mock_gemini_handler_));
  EXPECT_EQ(GetInfoBarManager()->infobars().size(), 0u);
}

// Test that accepting the infobar triggers
// startGeminiEntryFlowWithStartupState: with EntryPoint::ContextualCueInfobar,
// shouldAutoSubmit = YES, and the prompt.
TEST_F(ContextualCueInfobarDelegateTest,
       TestAcceptDispatchesGeminiFlowWithAutoSubmit) {
  SetUpCue("Summarize Page", "Summarize", "Summarize", "Summarize this page");

  EXPECT_TRUE(ContextualCueInfobarDelegate::Create(web_state_.get(),
                                                   mock_gemini_handler_));
  ContextualCueInfobarDelegate* delegate = GetDelegate();
  ASSERT_NE(delegate, nullptr);

  OCMExpect([mock_gemini_handler_
      startGeminiEntryFlowWithStartupState:[OCMArg checkWithBlock:^BOOL(
                                                       GeminiStartupState*
                                                           state) {
        return state.entryPoint == gemini::EntryPoint::ContextualCueInfobar &&
               state.shouldAutoSubmit == YES &&
               [state.prepopulatedPrompt
                   isEqualToString:@"Summarize this page"];
      }]
                        baseViewController:[OCMArg isNil]
                  showSnackbarOnCompletion:YES
                                completion:[OCMArg isNil]]);

  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(feature_engagement::events::kIOSGeminiContextualCueChipUsed))
      .Times(1);
  EXPECT_CALL(*mock_tracker_,
              Dismissed(testing::Ref(
                  feature_engagement::kIPHiOSGeminiContextualCueChip)))
      .Times(1);

  EXPECT_TRUE(delegate->Accept());
  EXPECT_OCMOCK_VERIFY(mock_gemini_handler_);
}

// Test that dismissing the infobar dismisses the feature engagement tracker.
TEST_F(ContextualCueInfobarDelegateTest, TestDismissRecordsMetric) {
  SetUpCue("Summarize Page", "Summarize", "Summarize", "Summarize this page");

  EXPECT_TRUE(ContextualCueInfobarDelegate::Create(web_state_.get(),
                                                   mock_gemini_handler_));
  ContextualCueInfobarDelegate* delegate = GetDelegate();
  ASSERT_NE(delegate, nullptr);

  EXPECT_CALL(*mock_tracker_,
              Dismissed(testing::Ref(
                  feature_engagement::kIPHiOSGeminiContextualCueChip)))
      .Times(1);

  delegate->InfoBarDismissed();
}

// Test that accepting before dismissal prevents double dismissing tracker.
TEST_F(ContextualCueInfobarDelegateTest, TestAcceptDoesNotRecordDismiss) {
  SetUpCue("Summarize Page", "Summarize", "Summarize", "Summarize this page");

  EXPECT_TRUE(ContextualCueInfobarDelegate::Create(web_state_.get(),
                                                   mock_gemini_handler_));
  ContextualCueInfobarDelegate* delegate = GetDelegate();
  ASSERT_NE(delegate, nullptr);

  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(feature_engagement::events::kIOSGeminiContextualCueChipUsed))
      .Times(1);
  EXPECT_CALL(*mock_tracker_,
              Dismissed(testing::Ref(
                  feature_engagement::kIPHiOSGeminiContextualCueChip)))
      .Times(1);

  EXPECT_TRUE(delegate->Accept());
  // Subsequent dismissal should be ignored because user already interacted.
  delegate->InfoBarDismissed();
}

// Test that ShowContextualCueInfobar on ContextualCueingTabHelper displays the
// infobar.
TEST_F(ContextualCueInfobarDelegateTest,
       TestTabHelperShowContextualCueInfobar) {
  SetUpCue("Summarize Page", "Summarize", "Summarize", "Summarize this page");

  ContextualCueingTabHelper* tab_helper =
      ContextualCueingTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(tab_helper, nullptr);

  EXPECT_TRUE(tab_helper->ShowContextualCueInfobar(mock_gemini_handler_));
  EXPECT_EQ(GetInfoBarManager()->infobars().size(), 1u);
}

// Test that navigation or invalidation cleans up the infobar.
TEST_F(ContextualCueInfobarDelegateTest,
       TestNavigationInvalidationRemovesInfobar) {
  SetUpCue("Summarize Page", "Summarize", "Summarize", "Summarize this page");

  ContextualCueingTabHelper* tab_helper =
      ContextualCueingTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(tab_helper, nullptr);

  EXPECT_TRUE(tab_helper->ShowContextualCueInfobar(mock_gemini_handler_));
  EXPECT_EQ(GetInfoBarManager()->infobars().size(), 1u);

  // Trigger navigation to a different page.
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetUrl(GURL("https://example.com/other_page"));
  tab_helper->DidFinishNavigation(web_state_.get(), &context);

  EXPECT_EQ(GetInfoBarManager()->infobars().size(), 0u);
}

}  // namespace contextual_cueing
