// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#import "components/send_tab_to_self/page_context.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_load_navigation_user_data.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

using send_tab_to_self::FakeSendTabToSelfModel;
using send_tab_to_self::SendTabToSelfEntry;

namespace {


class SendTabToSelfBrowserAgentTest : public PlatformTest {
 public:
  SendTabToSelfBrowserAgentTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  send_tab_to_self::StubSendTabToSelfSyncService>();
            }));

    profile_ = std::move(test_profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    mock_scene_commands_ =
        [OCMockObject mockForProtocol:@protocol(SceneCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_commands_
                     forProtocol:@protocol(SceneCommands)];
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    SendTabToSelfBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = SendTabToSelfBrowserAgent::FromBrowser(browser_.get());
    model_ = static_cast<FakeSendTabToSelfModel*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(browser_->GetProfile())
            ->GetSendTabToSelfModel());
  }

  web::FakeWebState* AppendNewWebState(const GURL& url,
                                       bool activate = true,
                                       bool is_visible = true) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetCurrentURL(url);
    // Create a navigation item to match the URL and give it a title.
    std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
    item->SetURL(url);
    item->SetTitle(u"Page title");
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetLastCommittedItem(item.get());
    // Test nav manager doesn't own its items, so move `item` into the storage
    // vector to define its lifetime.
    navigation_items_.push_back(std::move(item));
    fake_web_state->SetNavigationManager(std::move(navigation_manager));

    // Capture a pointer to the created web state to return.
    web::FakeWebState* inserted_web_state = fake_web_state.get();
    InfoBarManagerImpl::CreateForWebState(inserted_web_state);
    browser_->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::Automatic().Activate(activate));

    if (is_visible) {
      inserted_web_state->WasShown();
    }

    return inserted_web_state;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<SendTabToSelfBrowserAgent> agent_;
  raw_ptr<FakeSendTabToSelfModel> model_;
  // Storage vector for navigation items created for test cases.
  std::vector<std::unique_ptr<web::NavigationItem>> navigation_items_;

  // All infobar managers created during tests, for ease of clean-up.
  std::vector<infobars::InfoBarManager*> infobar_managers_;
  id mock_scene_commands_;
};

TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteAddSimple) {
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  model_->AddEntryRemotely(GURL("http://www.test.com/test-1"), "title",
                           "device1", send_tab_to_self::PageContext(),
                           send_tab_to_self::NavigationHistory());

  // An infobar for the entry should have been added.
  EXPECT_EQ(1UL, infobar_manager->infobars().size());
}

TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteAddNoTab) {
  // Remote entries added when there are no web states.
  model_->AddEntryRemotely(GURL("http://www.test.com/test-1"), "title",
                           "device1", send_tab_to_self::PageContext(),
                           send_tab_to_self::NavigationHistory());

  // Add a web state, active and visible.
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  // An infobar for the entry should have been added.
  EXPECT_EQ(1UL, infobar_manager->infobars().size());
}

TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteAddTabNotVisible) {
  // Add a web state, not visible.
  web::WebState* web_state =
      AppendNewWebState(GURL("http://www.blank.com"),
                        /*activate=*/true, /*is_visible=*/false);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  // Remote entries added.
  model_->AddEntryRemotely(GURL("http://www.test.com/test-1"), "title",
                           "device1", send_tab_to_self::PageContext(),
                           send_tab_to_self::NavigationHistory());

  // No visible web state, so expect no infobar.
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  // Show the web state.
  web_state->WasShown();

  // An infobar for the entry should have been added.
  EXPECT_EQ(1UL, infobar_manager->infobars().size());
}

TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteAddTabNotActive) {
  // Add a web state, not visible or active.
  web::WebState* web_state =
      AppendNewWebState(GURL("http://www.blank.com"),
                        /*activate=*/false, /*is_visible=*/false);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  // Remote entries added.
  model_->AddEntryRemotely(GURL("http://www.test.com/test-1"), "title",
                           "device1", send_tab_to_self::PageContext(),
                           send_tab_to_self::NavigationHistory());

  // No active web state, so expect no infobar.
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  // Show the web state. Since it was not active, still don't expect an infobar.
  web_state->WasShown();
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  // Activate the web state.
  browser_->GetWebStateList()->ActivateWebStateAt(0);
  // An infobar for the entry should have been added.
  EXPECT_EQ(1UL, infobar_manager->infobars().size());
}

TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteAddTabNotVisibleActivated) {
  // Add a web state, active but not visible.
  web::WebState* web_state =
      AppendNewWebState(GURL("http://www.blank.com"),
                        /*activate=*/true, /*is_visible=*/false);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  // Remote entries added.
  model_->AddEntryRemotely(GURL("http://www.test.com/test-1"), "title",
                           "device1", send_tab_to_self::PageContext(),
                           send_tab_to_self::NavigationHistory());

  // No visible web state, so expect no infobar.
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  // Add and activate a second web state.
  web::WebState* second_web_state =
      AppendNewWebState(GURL("http://www.blank.com"));
  InfoBarManagerImpl* second_infobar_manager =
      InfoBarManagerImpl::FromWebState(second_web_state);

  // An infobar for the entry should have been added to the second web state,
  // but not the first.
  EXPECT_EQ(0UL, infobar_manager->infobars().size());
  EXPECT_EQ(1UL, second_infobar_manager->infobars().size());
}

TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteRemoveSimple) {
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  const SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com/test-1"), "title", "device1",
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());

  // An infobar for the entry should have been added.
  EXPECT_EQ(1UL, infobar_manager->infobars().size());

  // Remove the entry remotely.
  model_->RemoveEntryRemotely(entry->GetGUID());

  // The infobar should have been removed.
  EXPECT_EQ(0UL, infobar_manager->infobars().size());
}

TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteRemovePending) {
  // Remote entry added when there are no web states (so it's pending).
  const SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com/test-1"), "title", "device1",
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());

  // Remove the entry remotely before any tab is shown.
  model_->RemoveEntryRemotely(entry->GetGUID());

  // Add a web state, active and visible.
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  // No infobar should be added since the pending entry was removed.
  EXPECT_EQ(0UL, infobar_manager->infobars().size());
}

// Tests that SendTabToSelfLoadNavigationUserData is correctly attached or
// detached when TabWillLoadUrl is triggered.
TEST_F(SendTabToSelfBrowserAgentTest, TestTabWillLoadUrl) {
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));

  // 1. Trigger with non-STTS parameters. No user data should be attached.
  UrlLoadParams params =
      UrlLoadParams::InCurrentTab(GURL("http://www.test.com"));
  EXPECT_FALSE(params.is_from_send_tab_to_self());
  UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get())
      ->TabWillLoadUrl(params, web_state->GetWeakPtr());
  EXPECT_EQ(nullptr,
            SendTabToSelfLoadNavigationUserData::FromWebState(web_state));

  // 2. Trigger with STTS parameters. User data should be attached.
  UrlLoadParams stts_params =
      UrlLoadParams::InCurrentTab(GURL("http://www.test.com"));
  stts_params.send_tab_to_self_entry_guid = "stts_guid_123";
  EXPECT_TRUE(stts_params.is_from_send_tab_to_self());
  UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get())
      ->TabWillLoadUrl(stts_params, web_state->GetWeakPtr());

  SendTabToSelfLoadNavigationUserData* user_data =
      SendTabToSelfLoadNavigationUserData::FromWebState(web_state);
  ASSERT_NE(nullptr, user_data);
  EXPECT_EQ("stts_guid_123", user_data->entry_guid());

  // 3. Trigger again with non-STTS parameters. The existing user data should be
  // removed.
  UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get())
      ->TabWillLoadUrl(params, web_state->GetWeakPtr());
  EXPECT_EQ(nullptr,
            SendTabToSelfLoadNavigationUserData::FromWebState(web_state));
}

}  // anonymous namespace
