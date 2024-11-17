// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_web_state_observer.h"

#import <memory>

#import "base/memory/scoped_refptr.h"
#import "base/time/default_clock.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/reading_list/model/offline_url_utils.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/network_change_notifier.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
const char kTestURL[] = "http://foo.bar";
const char kTestTitle[] = "title";
const char kTestDistilledPath[] = "distilled/page.html";
const char kTestDistilledURL[] = "http://foo.bar/distilled";

// A Test navigation manager that checks if Reload was called.
class FakeNavigationManager : public web::FakeNavigationManager {
 public:
  void Reload(web::ReloadType reload_type, bool check_for_repost) override {
    reload_called_ = true;
  }

  bool ReloadCalled() { return reload_called_; }

  int GetLastCommittedItemIndex() const override { return 1; }

  void GoToIndex(int index) override { go_to_index_called_index_ = index; }

  int GoToIndexCalled() { return go_to_index_called_index_; }

 private:
  bool reload_called_ = false;
  int go_to_index_called_index_ = -1;
};

// WebState that remembers the last opened parameters.
class FakeWebState : public web::FakeWebState {
 public:
  void OpenURL(const web::WebState::OpenURLParams& params) override {
    last_opened_url_ = params.url;
  }
  const GURL& LastOpenedUrl() { return last_opened_url_; }

 private:
  GURL last_opened_url_;
};

}  // namespace

// Test fixture to test loading of Reading list entries.
class ReadingListWebStateObserverTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    std::vector<scoped_refptr<ReadingListEntry>> initial_entries;
    initial_entries.push_back(base::MakeRefCounted<ReadingListEntry>(
        GURL(kTestURL), kTestTitle, base::Time::Now()));

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::move(initial_entries)));
    profile_ = std::move(builder).Build();

    test_web_state_.SetBrowserState(profile_.get());

    auto fake_navigation_manager = std::make_unique<FakeNavigationManager>();
    GURL url(kTestURL);
    std::unique_ptr<web::NavigationItem> pending_item =
        web::NavigationItem::Create();
    pending_item->SetURL(url);
    std::unique_ptr<web::NavigationItem> last_committed_item =
        web::NavigationItem::Create();

    fake_navigation_manager->SetPendingItem(pending_item.release());
    fake_navigation_manager->SetLastCommittedItem(
        last_committed_item.release());
    test_web_state_.SetNavigationManager(std::move(fake_navigation_manager));

    ReadingListWebStateObserver::CreateForWebState(&test_web_state_,
                                                   reading_list_model());
  }

  ReadingListModel* reading_list_model() {
    return ReadingListModelFactory::GetForProfile(profile_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeWebState test_web_state_;
};

// Tests that failing loading an online version does not mark it read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListFailure) {
  GURL url(kTestURL);
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);
  FakeNavigationManager* fake_navigation_manager =
      static_cast<FakeNavigationManager*>(
          test_web_state_.GetNavigationManager());

  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(fake_navigation_manager->ReloadCalled());
  EXPECT_EQ(fake_navigation_manager->GoToIndexCalled(), -1);
  // Check that `GetLastCommittedItem()` has not been altered.
  EXPECT_EQ(fake_navigation_manager->GetLastCommittedItem()->GetVirtualURL(),
            GURL());
  EXPECT_EQ(fake_navigation_manager->GetLastCommittedItem()->GetURL(), GURL());
  EXPECT_FALSE(entry->IsRead());
}

// Tests that loading an online version of an entry does not alter navigation
// stack and mark entry read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListOnline) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model()->SetEntryDistilledInfoIfExists(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);

  FakeNavigationManager* fake_navigation_manager =
      static_cast<FakeNavigationManager*>(
          test_web_state_.GetNavigationManager());
  fake_navigation_manager->GetPendingItem()->SetURL(url);

  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(fake_navigation_manager->ReloadCalled());
  EXPECT_EQ(fake_navigation_manager->GoToIndexCalled(), -1);
  // Check that `GetLastCommittedItem()` has not been altered.
  EXPECT_EQ(fake_navigation_manager->GetLastCommittedItem()->GetVirtualURL(),
            GURL());
  EXPECT_EQ(fake_navigation_manager->GetLastCommittedItem()->GetURL(), GURL());
  EXPECT_TRUE(entry->IsRead());
}

// Tests that loading an online version of an entry does update navigation
// stack and mark entry read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListDistilledCommitted) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model()->SetEntryDistilledInfoIfExists(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);
  GURL distilled_url = reading_list::OfflineURLForURL(entry->URL());

  FakeNavigationManager* fake_navigation_manager =
      static_cast<FakeNavigationManager*>(
          test_web_state_.GetNavigationManager());

  // Test on committed entry, there must be no pending item.
  fake_navigation_manager->SetPendingItem(nullptr);
  fake_navigation_manager->GetLastCommittedItem()->SetURL(url);

  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(fake_navigation_manager->ReloadCalled());
  EXPECT_EQ(fake_navigation_manager->GoToIndexCalled(),
            fake_navigation_manager->GetLastCommittedItemIndex());
  EXPECT_EQ(fake_navigation_manager->GetLastCommittedItem()->GetVirtualURL(),
            url);
  EXPECT_EQ(fake_navigation_manager->GetLastCommittedItem()->GetURL(),
            distilled_url);
  EXPECT_TRUE(entry->IsRead());
}

// Tests that loading an online version of a pending entry on reload does update
// committed entry, reload, and mark entry read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListDistilledPending) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model()->SetEntryDistilledInfoIfExists(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);
  GURL distilled_url = reading_list::OfflineURLForURL(entry->URL());

  FakeNavigationManager* fake_navigation_manager =
      static_cast<FakeNavigationManager*>(
          test_web_state_.GetNavigationManager());

  fake_navigation_manager->SetPendingItem(nil);
  fake_navigation_manager->GetLastCommittedItem()->SetURL(url);

  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(fake_navigation_manager->ReloadCalled());
  EXPECT_EQ(fake_navigation_manager->GoToIndexCalled(),
            fake_navigation_manager->GetLastCommittedItemIndex());
  EXPECT_EQ(fake_navigation_manager->GetLastCommittedItem()->GetVirtualURL(),
            url);
  EXPECT_EQ(fake_navigation_manager->GetLastCommittedItem()->GetURL(),
            distilled_url);

  EXPECT_TRUE(entry->IsRead());
}
