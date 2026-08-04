// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/infobars/core/infobar.h"
#import "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/page_context.h"
#import "base/run_loop.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/send_tab_to_self/model/ios_send_tab_to_self_infobar_delegate.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_load_navigation_user_data.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_card_label_data.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
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
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

using send_tab_to_self::FakeSendTabToSelfModel;
using send_tab_to_self::SendTabToSelfEntry;

namespace {

const char kBlankURL[] = "about:blank";
const char kExampleURL[] = "https://www.example.com/";
const char kDeviceID[] = "device_id";

class SendTabToSelfBrowserAgentTest : public PlatformTest {
 public:
  explicit SendTabToSelfBrowserAgentTest(
      const std::vector<base::test::FeatureRef>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features = {}) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
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
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
    SendTabToSelfBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = SendTabToSelfBrowserAgent::FromBrowser(browser_.get());
    model_ = static_cast<FakeSendTabToSelfModel*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(browser_->GetProfile())
            ->GetSendTabToSelfModel());
  }
  ~SendTabToSelfBrowserAgentTest() override = default;

  ProfileIOS* profile() { return profile_.get(); }
  id mock_scene_commands() { return mock_scene_commands_; }

  web::FakeWebState* AppendNewWebState(const GURL& url,
                                       bool activate = true,
                                       bool is_visible = true) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(profile_.get());
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

  void ExpectSceneCommandForBackgroundTabOpen(size_t count = 1) {
    if (!base::FeatureList::IsEnabled(
            send_tab_to_self::kSendTabToSelfSupportAutoOpenInTabGrid)) {
      for (size_t i = 0; i < count; ++i) {
        OCMExpect([mock_scene_commands_
            openURLInNewTab:[OCMArg checkWithBlock:^BOOL(
                                        OpenNewTabCommand* command) {
              return command.inBackground == YES;
            }]]);
      }
    }
  }

  void VerifyBackgroundTabOpened(
      const send_tab_to_self::SendTabToSelfEntry* entry,
      int expected_call_count = 1) {
    if (base::FeatureList::IsEnabled(
            send_tab_to_self::kSendTabToSelfSupportAutoOpenInTabGrid)) {
      EXPECT_EQ(expected_call_count, url_loader_->load_new_tab_call_count);
      if (entry) {
        EXPECT_EQ(entry->GetURL(), url_loader_->last_params.web_params.url);
        EXPECT_TRUE(url_loader_->last_params.in_background());
        EXPECT_EQ(OpenPosition::kCurrentTab,
                  url_loader_->last_params.append_to);
        EXPECT_EQ(entry->GetGUID(),
                  url_loader_->last_params.send_tab_to_self_entry_guid);
      }
    } else {
      [mock_scene_commands_ verify];
    }
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<SendTabToSelfBrowserAgent> agent_;
  raw_ptr<FakeSendTabToSelfModel> model_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
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
                           kDeviceID, send_tab_to_self::PageContext(),
                           send_tab_to_self::NavigationHistory());

  // An infobar for the entry should have been added.
  EXPECT_EQ(1UL, infobar_manager->infobars().size());
}

// Tests that when multiple remote entries are added simultaneously, an InfoBar
// is shown for the most recently shared entry rather than simply the last entry
// in the batch.
TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteAddMultiplePicksMostRecent) {
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  const base::Time now = base::Time::Now();
  std::vector<FakeSendTabToSelfModel::RemoteEntryParams> entry_params(3);
  entry_params[0].url = GURL("http://www.test.com/older");
  entry_params[0].shared_time = now - base::Seconds(10);
  entry_params[1].url = GURL("http://www.test.com/newest");
  entry_params[1].shared_time = now;
  entry_params[2].url = GURL("http://www.test.com/older-still");
  entry_params[2].shared_time = now - base::Seconds(5);

  std::vector<const SendTabToSelfEntry*> entries =
      model_->AddEntriesRemotely(std::move(entry_params));
  ASSERT_EQ(3UL, entries.size());

  // Only one infobar should be added, corresponding to the entry with the
  // latest shared timestamp (index 1), even though it is not at the end of the
  // vector.
  ASSERT_EQ(1UL, infobar_manager->infobars().size());
  infobars::InfoBar* infobar = infobar_manager->infobars()[0];
  auto* delegate =
      static_cast<send_tab_to_self::IOSSendTabToSelfInfoBarDelegate*>(
          infobar->delegate());
  EXPECT_EQ(entries[1]->GetGUID(), delegate->GetGUID());
}

TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteAddNoTab) {
  // Remote entries added when there are no web states.
  model_->AddEntryRemotely(GURL("http://www.test.com/test-1"), "title",
                           kDeviceID, send_tab_to_self::PageContext(),
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
                           kDeviceID, send_tab_to_self::PageContext(),
                           send_tab_to_self::NavigationHistory());

  // No visible web state, so expect no infobar.
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  // Show the web state.
  web_state->WasShown();

  // An infobar for the entry should have been added.
  EXPECT_EQ(1UL, infobar_manager->infobars().size());
}

// Tests that when multiple remote entries are added while the active WebState
// is not visible, showing the WebState creates an InfoBar for the most
// recently shared entry.
TEST_F(SendTabToSelfBrowserAgentTest,
       TestRemoteAddMultipleNotVisiblePicksMostRecent) {
  web::WebState* web_state =
      AppendNewWebState(GURL("http://www.blank.com"),
                        /*activate=*/true, /*is_visible=*/false);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  const base::Time now = base::Time::Now();
  std::vector<FakeSendTabToSelfModel::RemoteEntryParams> entry_params(3);
  entry_params[0].url = GURL("http://www.test.com/older");
  entry_params[0].shared_time = now - base::Seconds(10);
  entry_params[1].url = GURL("http://www.test.com/newest");
  entry_params[1].shared_time = now;
  entry_params[2].url = GURL("http://www.test.com/older-still");
  entry_params[2].shared_time = now - base::Seconds(5);

  std::vector<const SendTabToSelfEntry*> entries =
      model_->AddEntriesRemotely(std::move(entry_params));
  ASSERT_EQ(3UL, entries.size());

  // No visible web state, so expect no infobar.
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  // Show the web state.
  web_state->WasShown();

  // Only one infobar should be added, corresponding to the entry with the
  // latest shared timestamp (index 1), even though it is not at the end of the
  // vector.
  ASSERT_EQ(1UL, infobar_manager->infobars().size());
  infobars::InfoBar* infobar = infobar_manager->infobars()[0];
  auto* delegate =
      static_cast<send_tab_to_self::IOSSendTabToSelfInfoBarDelegate*>(
          infobar->delegate());
  EXPECT_EQ(entries[1]->GetGUID(), delegate->GetGUID());
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
                           kDeviceID, send_tab_to_self::PageContext(),
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
                           kDeviceID, send_tab_to_self::PageContext(),
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
      GURL("http://www.test.com/test-1"), "title", kDeviceID,
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
      GURL("http://www.test.com/test-1"), "title", kDeviceID,
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

class SendTabToSelfBrowserAgentAutoOpenTest
    : public SendTabToSelfBrowserAgentTest {
 public:
  SendTabToSelfBrowserAgentAutoOpenTest()
      : SendTabToSelfBrowserAgentTest(
            {send_tab_to_self::kSendTabToSelfAutoOpen,
             send_tab_to_self::kSendTabToSelfPropagateScrollPosition}) {
    model_->SetLocalCacheGuid(kDeviceID);
  }
};

TEST_F(SendTabToSelfBrowserAgentAutoOpenTest,
       ShouldAutoOpenNewEntriesInBackgroundIfActive) {
  base::HistogramTester histogram_tester;
  web::WebState* web_state = AppendNewWebState(GURL(kBlankURL));
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  ExpectSceneCommandForBackgroundTabOpen();

  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL(kExampleURL), "title", kDeviceID, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory());

  VerifyBackgroundTabOpened(entry);
  EXPECT_TRUE(model_->GetEntryByGUID(entry->GetGUID())->IsOpened());
  EXPECT_EQ(1UL, infobar_manager->infobars().size());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      send_tab_to_self::AutoOpenOutcome::kTabsOpenedImmediatelyInBackground, 1);
}

// Tests that auto-opening a received tab in the background passes the entry
// GUID on the OpenNewTabCommand (which will later be used to restore the scroll
// position).
TEST_F(SendTabToSelfBrowserAgentAutoOpenTest,
       ShouldAutoOpenNewEntriesInBackgroundWithScrollPosition) {
  web::WebState* web_state = AppendNewWebState(GURL(kBlankURL));
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  ExpectSceneCommandForBackgroundTabOpen();

  send_tab_to_self::PageContext page_context;
  page_context.scroll_position.text_fragment.text_start = "start";
  page_context.scroll_position.text_fragment.text_end = "end";

  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL(kExampleURL), "title", kDeviceID, page_context,
      send_tab_to_self::NavigationHistory());

  VerifyBackgroundTabOpened(entry);
  EXPECT_TRUE(model_->GetEntryByGUID(entry->GetGUID())->IsOpened());
  EXPECT_EQ(1UL, infobar_manager->infobars().size());
}

// Tests that entries are not auto-opened if there is no active WebState
// (e.g., during browser startup before tabs are restored).
TEST_F(SendTabToSelfBrowserAgentAutoOpenTest,
       ShouldNotAutoOpenNewEntriesIfNoActiveWebState) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(nullptr, browser_->GetWebStateList()->GetActiveWebState());

  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL(kExampleURL), "title", kDeviceID, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory());

  EXPECT_EQ(0, url_loader_->load_new_tab_call_count);
  EXPECT_FALSE(model_->GetEntryByGUID(entry->GetGUID())->IsOpened());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      send_tab_to_self::AutoOpenOutcome::kUnopenedImmediately, 1);
}

TEST_F(SendTabToSelfBrowserAgentAutoOpenTest,
       ShouldAutoOpenPendingEntriesInBackgroundOnActivation) {
  base::HistogramTester histogram_tester;
  const send_tab_to_self::SendTabToSelfEntry* entry1 = model_->AddEntryRemotely(
      GURL("https://www.google.com/"), "title", kDeviceID,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  const send_tab_to_self::SendTabToSelfEntry* entry2 = model_->AddEntryRemotely(
      GURL("https://www.youtube.com/"), "title", kDeviceID,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());

  ExpectSceneCommandForBackgroundTabOpen(2);
  web::WebState* web_state = AppendNewWebState(GURL(kBlankURL));

  VerifyBackgroundTabOpened(entry2, 2);
  EXPECT_TRUE(model_->GetEntryByGUID(entry1->GetGUID())->IsOpened());
  EXPECT_TRUE(model_->GetEntryByGUID(entry2->GetGUID())->IsOpened());
  EXPECT_EQ(1UL,
            InfoBarManagerImpl::FromWebState(web_state)->infobars().size());

  histogram_tester.ExpectBucketCount(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      send_tab_to_self::AutoOpenOutcome::kUnopenedImmediately, 2);
  histogram_tester.ExpectBucketCount(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      send_tab_to_self::AutoOpenOutcome::kTabsOpenedInBackgroundUponActivation,
      2);
}

// Tests that SendTabToSelfTabCardLabelData is attached when the tab is loaded
// in the background, but NOT when loaded in the foreground.
TEST_F(SendTabToSelfBrowserAgentAutoOpenTest,
       TestTabWillLoadUrlBackgroundOnly) {
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));

  ExpectSceneCommandForBackgroundTabOpen();

  // Create an entry in the model.
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", kDeviceID,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  VerifyBackgroundTabOpened(entry);
  std::string guid = entry->GetGUID();

  // Trigger TabWillLoadUrl with in_background = false (foreground).
  UrlLoadParams fg_params =
      UrlLoadParams::InCurrentTab(GURL("http://www.test.com"));
  fg_params.send_tab_to_self_entry_guid = guid;
  fg_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  EXPECT_FALSE(fg_params.in_background());

  UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get())
      ->TabWillLoadUrl(fg_params, web_state->GetWeakPtr());

  // Tracker should NOT be attached.
  EXPECT_EQ(nullptr, SendTabToSelfTabCardLabelData::FromWebState(web_state));

  // Trigger TabWillLoadUrl with in_background = true (background).
  UrlLoadParams bg_params =
      UrlLoadParams::InCurrentTab(GURL("http://www.test.com"));
  bg_params.send_tab_to_self_entry_guid = guid;
  bg_params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  EXPECT_TRUE(bg_params.in_background());

  UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get())
      ->TabWillLoadUrl(bg_params, web_state->GetWeakPtr());

  // Tracker SHOULD be attached.
  EXPECT_NE(nullptr, SendTabToSelfTabCardLabelData::FromWebState(web_state));
}

// Tests that closing a tab by user action (detaching it with is_user_action =
// true) with a tab card label attached logs the abandonment metric.
TEST_F(SendTabToSelfBrowserAgentAutoOpenTest, ClosingTabLogsAbandonment) {
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));

  ExpectSceneCommandForBackgroundTabOpen();

  // Create an entry and attach the label.
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", kDeviceID,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  VerifyBackgroundTabOpened(entry);
  std::string guid = entry->GetGUID();

  SendTabToSelfTabCardLabelData::CreateForWebState(web_state, guid,
                                                   "remote_device");

  // Verify it is attached.
  EXPECT_NE(nullptr, SendTabToSelfTabCardLabelData::FromWebState(web_state));

  // Close the tab (which detaches it with kUserAction).
  EXPECT_EQ(model_->last_activated_guid(), "");

  int index = browser_->GetWebStateList()->GetIndexOfWebState(web_state);
  ASSERT_NE(index, WebStateList::kInvalidIndex);

  browser_->GetWebStateList()->CloseWebStateAt(
      index, WebStateList::ClosingReason::kUserAction);

  // Verify that the abandonment metric was logged.
  EXPECT_EQ(model_->last_activated_guid(), guid);
  EXPECT_EQ(model_->last_activated_entry_point(),
            send_tab_to_self::ShareActivatedEntryPoint::
                kTabOrBrowserClosedWithoutActivation);
}

// Tests that closing a tab due to shutdown (detaching it with is_user_action =
// false) with a tab card label attached does NOT log the abandonment metric.
TEST_F(SendTabToSelfBrowserAgentAutoOpenTest, ShutdownDoesNotLogAbandonment) {
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));

  ExpectSceneCommandForBackgroundTabOpen();

  // Create an entry and attach the label.
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", kDeviceID,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  VerifyBackgroundTabOpened(entry);
  std::string guid = entry->GetGUID();

  SendTabToSelfTabCardLabelData::CreateForWebState(web_state, guid,
                                                   "remote_device");

  // Verify it is attached.
  EXPECT_NE(nullptr, SendTabToSelfTabCardLabelData::FromWebState(web_state));

  // Close the tab with kDefault (simulating shutdown/default close).
  EXPECT_EQ(model_->last_activated_guid(), "");

  int index = browser_->GetWebStateList()->GetIndexOfWebState(web_state);
  ASSERT_NE(index, WebStateList::kInvalidIndex);

  browser_->GetWebStateList()->CloseWebStateAt(
      index, WebStateList::ClosingReason::kDefault);

  // Verify that the abandonment metric was NOT logged.
  EXPECT_EQ(model_->last_activated_guid(), "");
}

// Tests that moving a tab to another window via drag-and-drop (detaching it
// with is_closing = false) does NOT log the abandonment metric.
TEST_F(SendTabToSelfBrowserAgentAutoOpenTest,
       DragAndDropDoesNotLogAbandonment) {
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));

  ExpectSceneCommandForBackgroundTabOpen();

  // Create an entry and attach the label.
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", kDeviceID,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  VerifyBackgroundTabOpened(entry);
  std::string guid = entry->GetGUID();

  SendTabToSelfTabCardLabelData::CreateForWebState(web_state, guid,
                                                   "remote_device");

  // Verify it is attached.
  EXPECT_NE(nullptr, SendTabToSelfTabCardLabelData::FromWebState(web_state));

  // Retrieve index.
  int index = browser_->GetWebStateList()->GetIndexOfWebState(web_state);
  ASSERT_NE(index, WebStateList::kInvalidIndex);

  // Detach the web state from the list, simulating dragging it to another
  // window (corresponds to reason kDetached).
  EXPECT_EQ(model_->last_activated_guid(), "");
  std::unique_ptr<web::WebState> detached_web_state =
      browser_->GetWebStateList()->DetachWebStateAt(index);

  // Verify that the abandonment metric was NOT logged because the tab is not
  // actually closing.
  EXPECT_EQ(model_->last_activated_guid(), "");
}

class SendTabToSelfBrowserAgentAutoOpenInTabGridTest
    : public SendTabToSelfBrowserAgentTest {
 public:
  SendTabToSelfBrowserAgentAutoOpenInTabGridTest()
      : SendTabToSelfBrowserAgentTest(
            {send_tab_to_self::kSendTabToSelfAutoOpen,
             send_tab_to_self::kSendTabToSelfSupportAutoOpenInTabGrid,
             send_tab_to_self::kSendTabToSelfPropagateScrollPosition}) {
    model_->SetLocalCacheGuid(kDeviceID);
  }
};

// Tests that when both auto-open and Tab Grid support flags are enabled, and
// the active WebState is not visible (e.g., user is in the Tab Grid), entries
// are opened in the background immediately, but the infobar banner is not
// displayed.
TEST_F(SendTabToSelfBrowserAgentAutoOpenInTabGridTest,
       ShouldAutoOpenNewEntriesInBackgroundIfNotVisible) {
  base::HistogramTester histogram_tester;

  web::WebState* web_state = AppendNewWebState(
      GURL(kBlankURL), /*activate=*/true, /*is_visible=*/false);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL(kExampleURL), "title", kDeviceID, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory());

  VerifyBackgroundTabOpened(entry);

  EXPECT_TRUE(model_->GetEntryByGUID(entry->GetGUID())->IsOpened());
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      send_tab_to_self::AutoOpenOutcome::kTabsOpenedImmediatelyInBackground, 1);
}

// Tests that sending a tab to a specified target device adds a corresponding
// entry to the Send Tab to Self model when the model is ready.
TEST_F(SendTabToSelfBrowserAgentTest,
       SendTabToTargetDevice_AddsEntryToModelWhenReady) {
  agent_->SendTabToTargetDevice(GURL("https://example.com"), "Title", "target",
                                "My Phone",
                                send_tab_to_self::ShareEntryPoint::kShareSheet);

  EXPECT_EQ(1u, model_->GetAllGuids().size());
  const send_tab_to_self::SendTabToSelfEntry* entry =
      model_->GetEntryByGUID(model_->GetAllGuids()[0]);
  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->GetURL(), GURL("https://example.com"));
  EXPECT_EQ(entry->GetTitle(), "Title");
  EXPECT_EQ(entry->GetTargetDeviceSyncCacheGuid(), "target");
}

class SendTabToSelfBrowserAgentToastEnabledTest
    : public SendTabToSelfBrowserAgentTest {
 public:
  SendTabToSelfBrowserAgentToastEnabledTest()
      : SendTabToSelfBrowserAgentTest(
            {send_tab_to_self::kSendTabToSelfPostSendToast},
            {}) {}
};

class SendTabToSelfBrowserAgentToastDisabledTest
    : public SendTabToSelfBrowserAgentTest {
 public:
  SendTabToSelfBrowserAgentToastDisabledTest()
      : SendTabToSelfBrowserAgentTest(
            {},
            {send_tab_to_self::kSendTabToSelfPostSendToast}) {}
};

// Tests that invoking HandleEntrySentForTest with kSuccess displays the
// success toast when toast feature is enabled.
TEST_F(SendTabToSelfBrowserAgentToastEnabledTest,
       HandleEntrySent_SuccessShowsToast) {
  id mock_snackbar_commands = OCMProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([mock_snackbar_commands showSnackbarMessage:[OCMArg any]]);

  agent_->HandleEntrySentForTest(
      mock_snackbar_commands, "My Phone",
      send_tab_to_self::SendTabToSelfResult::kSuccess);

  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands);
}

// Tests that invoking HandleEntrySentForTest with kSuccessThrottled displays
// the throttled toast when toast feature is enabled.
TEST_F(SendTabToSelfBrowserAgentToastEnabledTest,
       HandleEntrySent_SuccessThrottledShowsToast) {
  id mock_snackbar_commands = OCMProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([mock_snackbar_commands showSnackbarMessage:[OCMArg any]]);

  agent_->HandleEntrySentForTest(
      mock_snackbar_commands, "My Phone",
      send_tab_to_self::SendTabToSelfResult::kSuccessThrottled);

  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands);
}

// Tests that invoking HandleEntrySentForTest with a failure result displays
// the failure toast when toast feature is enabled.
TEST_F(SendTabToSelfBrowserAgentToastEnabledTest,
       HandleEntrySent_FailureShowsErrorToast) {
  id mock_snackbar_commands = OCMProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([mock_snackbar_commands showSnackbarMessage:[OCMArg any]]);

  agent_->HandleEntrySentForTest(
      mock_snackbar_commands, "",
      send_tab_to_self::SendTabToSelfResult::kFailureNoInternetConnection);

  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands);
}

// Tests that invoking HandleEntrySentForTest with kSuccess displays the
// legacy snackbar when toast feature is disabled.
TEST_F(SendTabToSelfBrowserAgentToastDisabledTest,
       HandleEntrySent_LegacySnackbarDisplayedOnlyOnSuccess) {
  id mock_snackbar_commands = OCMProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([mock_snackbar_commands showSnackbarMessage:[OCMArg any]]);

  agent_->HandleEntrySentForTest(
      mock_snackbar_commands, "My Phone",
      send_tab_to_self::SendTabToSelfResult::kSuccess);

  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands);
}

}  // anonymous namespace
