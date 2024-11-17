// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/send_tab_to_self/test_send_tab_to_self_model.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using send_tab_to_self::SendTabToSelfEntry;

namespace {

// TODO (crbug/974040): update TestSendTabToSelfModel and delete this class
class FakeSendTabToSelfModel : public send_tab_to_self::TestSendTabToSelfModel {
 public:
  FakeSendTabToSelfModel() = default;
  ~FakeSendTabToSelfModel() override = default;

  const SendTabToSelfEntry* AddEntry(
      const GURL& url,
      const std::string& title,
      const std::string& target_device_cache_guid) override {
    last_entry_ = SendTabToSelfEntry::FromRequiredFields(
        "test-guid", url, target_device_cache_guid);
    return last_entry_.get();
  }

  bool IsReady() override { return true; }
  bool HasValidTargetDevice() override { return true; }

  SendTabToSelfEntry* GetLastEntry() { return last_entry_.get(); }

  void RemoteAddEntry(send_tab_to_self::SendTabToSelfEntry* entry) {
    std::vector<const SendTabToSelfEntry*> entries;
    entries.push_back(entry);
    for (send_tab_to_self::SendTabToSelfModelObserver& observer : observers_) {
      observer.EntriesAddedRemotely(entries);
    }
  }

 private:
  std::unique_ptr<SendTabToSelfEntry> last_entry_;
};

// TODO (crbug/974040): Move TestSendTabToSelfSyncService to components and
// reuse in both ios/chrome and chrome tests
class TestSendTabToSelfSyncService
    : public send_tab_to_self::SendTabToSelfSyncService {
 public:
  TestSendTabToSelfSyncService()
      : model_(std::make_unique<FakeSendTabToSelfModel>()) {}
  ~TestSendTabToSelfSyncService() override = default;

  static std::unique_ptr<KeyedService> Build(web::BrowserState* context) {
    return std::make_unique<TestSendTabToSelfSyncService>();
  }

  send_tab_to_self::SendTabToSelfModel* GetSendTabToSelfModel() override {
    return model_.get();
  }

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override {
    return nullptr;
  }

 private:
  std::unique_ptr<FakeSendTabToSelfModel> model_;
};

class SendTabToSelfBrowserAgentTest : public PlatformTest {
 public:
  SendTabToSelfBrowserAgentTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindRepeating(&::TestSendTabToSelfSyncService::Build));

    profile_ = std::move(test_profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
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
};

TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteAddSimple) {
  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0UL, infobar_manager->infobars().size());

  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com/test-1"), "device1");
  model_->RemoteAddEntry(entry.get());

  // An infobar for the entry should have been added.
  EXPECT_EQ(1UL, infobar_manager->infobars().size());
}

TEST_F(SendTabToSelfBrowserAgentTest, TestRemoteAddNoTab) {
  // Remote entries added when there are no web states.
  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com/test-1"), "device1");
  model_->RemoteAddEntry(entry.get());

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
  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com/test-1"), "device1");
  model_->RemoteAddEntry(entry.get());

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
  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com/test-1"), "device1");
  model_->RemoteAddEntry(entry.get());

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
  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com/test-1"), "device1");
  model_->RemoteAddEntry(entry.get());

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

}  // anonymous namespace
