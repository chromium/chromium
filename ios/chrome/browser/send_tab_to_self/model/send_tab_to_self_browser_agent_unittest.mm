// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/page_context.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_load_navigation_user_data.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_card_label_data.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
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

const char kBlankURL[] = "about:blank";
const char kExampleURL[] = "https://www.example.com/";
const char kDeviceID[] = "device_id";

class SendTabToSelfBrowserAgentTest : public PlatformTest {
 public:
  explicit SendTabToSelfBrowserAgentTest(
      const std::vector<base::test::FeatureRef>& enabled_features = {}) {
    feature_list_.InitWithFeatures(enabled_features, {});
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

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
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
                           kDeviceID, send_tab_to_self::PageContext(),
                           send_tab_to_self::NavigationHistory());

  // An infobar for the entry should have been added.
  EXPECT_EQ(1UL, infobar_manager->infobars().size());
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
            {send_tab_to_self::kSendTabToSelfAutoOpen}) {
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

  OCMExpect([mock_scene_commands_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        return command.inBackground == YES;
      }]]);

  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL(kExampleURL), "title", kDeviceID, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory());

  [mock_scene_commands_ verify];
  EXPECT_TRUE(model_->GetEntryByGUID(entry->GetGUID())->IsOpened());
  EXPECT_EQ(1UL, infobar_manager->infobars().size());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      send_tab_to_self::AutoOpenOutcome::kTabsOpenedImmediatelyInBackground, 1);
}

TEST_F(SendTabToSelfBrowserAgentAutoOpenTest,
       ShouldNotAutoOpenNewEntriesIfNotActive) {
  base::HistogramTester histogram_tester;
  AppendNewWebState(GURL(kBlankURL),
                    /*activate=*/true, /*is_visible=*/false);

  [[mock_scene_commands_ reject] openURLInNewTab:[OCMArg any]];

  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL(kExampleURL), "title", kDeviceID, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory());

  [mock_scene_commands_ verify];
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

  OCMExpect([mock_scene_commands_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        return command.inBackground == YES;
      }]]);
  OCMExpect([mock_scene_commands_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        return command.inBackground == YES;
      }]]);
  web::WebState* web_state = AppendNewWebState(GURL(kBlankURL));

  [mock_scene_commands_ verify];
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
  OCMStub([mock_scene_commands_ openURLInNewTab:[OCMArg any]]);
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));

  // Create an entry in the model.
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", kDeviceID,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
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
  OCMStub([mock_scene_commands_ openURLInNewTab:[OCMArg any]]);
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));

  // Create an entry and attach the label.
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", kDeviceID,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
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
  OCMStub([mock_scene_commands_ openURLInNewTab:[OCMArg any]]);
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));

  // Create an entry and attach the label.
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", kDeviceID,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
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
  OCMStub([mock_scene_commands_ openURLInNewTab:[OCMArg any]]);
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));

  // Create an entry and attach the label.
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL("http://www.test.com"), "title", kDeviceID,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
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

}  // anonymous namespace
