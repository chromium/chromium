// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/page_context.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_load_navigation_user_data.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class SendTabToSelfTabHelperTest : public PlatformTest {
 protected:
  SendTabToSelfTabHelperTest() {
    feature_list_.InitWithFeatures(
        {send_tab_to_self::kSendTabToSelfPropagateScrollPosition,
         send_tab_to_self::kSendTabToSelfPropagateFormFields},
        {});

    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  send_tab_to_self::StubSendTabToSelfSyncService>();
            }));
    profile_ = std::move(test_profile_builder).Build();
    web_state_.SetBrowserState(profile_.get());

    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_state_.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                   std::move(frames_manager));

    model_ = static_cast<send_tab_to_self::FakeSendTabToSelfModel*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(profile_.get())
            ->GetSendTabToSelfModel());
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_;
  raw_ptr<send_tab_to_self::FakeSendTabToSelfModel> model_;
};

// Tests that the tab helper handles cases where there is no navigation item.
// The user data should be successfully cleaned up even if there are no items.
TEST_F(SendTabToSelfTabHelperTest, NoNavigationItem) {
  SendTabToSelfLoadNavigationUserData::CreateForWebState(&web_state_,
                                                         "test_guid");
  SendTabToSelfTabHelper::CreateForWebState(&web_state_);

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_EQ(nullptr,
            SendTabToSelfLoadNavigationUserData::FromWebState(&web_state_));
}

// Tests that the tab helper does not remove the user data when the page load
// fails, allowing it to be retried on a subsequent navigation or reload.
TEST_F(SendTabToSelfTabHelperTest, PageLoadFailed) {
  SendTabToSelfLoadNavigationUserData::CreateForWebState(&web_state_,
                                                         "test_guid");
  SendTabToSelfTabHelper::CreateForWebState(&web_state_);

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);

  EXPECT_NE(nullptr,
            SendTabToSelfLoadNavigationUserData::FromWebState(&web_state_));
}

// Tests that the tab helper handles the case where there is no scroll position
// to restore. The user data is successfully consumed and removed.
TEST_F(SendTabToSelfTabHelperTest, NoScrollPosition) {
  SendTabToSelfTabHelper::CreateForWebState(&web_state_);

  send_tab_to_self::PageContext page_context;
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", "device1", page_context,
      send_tab_to_self::NavigationHistory());

  SendTabToSelfLoadNavigationUserData::CreateForWebState(&web_state_,
                                                         entry->GetGUID());

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_EQ(nullptr,
            SendTabToSelfLoadNavigationUserData::FromWebState(&web_state_));
}

// Tests that the tab helper handles an empty text fragment correctly, clearing
// the user data after execution.
TEST_F(SendTabToSelfTabHelperTest, EmptyTextFragment) {
  SendTabToSelfTabHelper::CreateForWebState(&web_state_);

  send_tab_to_self::PageContext page_context;
  page_context.scroll_position.text_fragment =
      send_tab_to_self::TextFragmentData("", "", "", "");
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", "device1", page_context,
      send_tab_to_self::NavigationHistory());

  SendTabToSelfLoadNavigationUserData::CreateForWebState(&web_state_,
                                                         entry->GetGUID());

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_EQ(nullptr,
            SendTabToSelfLoadNavigationUserData::FromWebState(&web_state_));
}

// Tests that the tab helper successfully processes the STTS load when a valid
// text fragment is present, and ensures the user data is cleared afterwards.
TEST_F(SendTabToSelfTabHelperTest, SttsLoad) {
  send_tab_to_self::PageContext page_context;
  page_context.scroll_position.text_fragment =
      send_tab_to_self::TextFragmentData("start", "end", "", "");
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", "device1", page_context,
      send_tab_to_self::NavigationHistory());

  SendTabToSelfLoadNavigationUserData::CreateForWebState(&web_state_,
                                                         entry->GetGUID());
  SendTabToSelfTabHelper::CreateForWebState(&web_state_);

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_EQ(nullptr,
            SendTabToSelfLoadNavigationUserData::FromWebState(&web_state_));
}
