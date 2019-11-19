// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_web_state_observer.h"

#include <memory>

#include "base/time/default_clock.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#import "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_test.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kTestURL[] = "http://foo.bar";
const char kTestTitle[] = "title";
const char kTestDistilledPath[] = "distilled/page.html";
const char kTestDistilledURL[] = "http://foo.bar/distilled";
}

// A Test navigation manager that checks if Reload was called.
class TestNavigationManager : public web::TestNavigationManager {
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

// A Test navigation manager that remembers the last opened parameters.
class TestWebState : public web::TestWebState {
 public:
  void OpenURL(const web::WebState::OpenURLParams& params) override {
    last_opened_url_ = params.url;
  }
  const GURL& LastOpenedUrl() { return last_opened_url_; }

 private:
  GURL last_opened_url_;
};

// Test fixture to test loading of Reading list entries.
class ReadingListWebStateObserverTest : public web::WebTest {
 public:
  ReadingListWebStateObserverTest() {
    auto test_navigation_manager = std::make_unique<TestNavigationManager>();
    test_navigation_manager_ = test_navigation_manager.get();
    pending_item_ = web::NavigationItem::Create();
    last_committed_item_ = web::NavigationItem::Create();
    test_navigation_manager->SetPendingItem(pending_item_.get());
    test_navigation_manager->SetLastCommittedItem(last_committed_item_.get());
    test_web_state_.SetNavigationManager(std::move(test_navigation_manager));
    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        nullptr, nullptr, base::DefaultClock::GetInstance());
    reading_list_model_->AddEntry(GURL(kTestURL), kTestTitle,
                                  reading_list::ADDED_VIA_CURRENT_APP);
    ReadingListWebStateObserver::CreateForWebState(&test_web_state_,
                                                   reading_list_model_.get());
  }

 protected:
  std::unique_ptr<web::NavigationItem> pending_item_;
  std::unique_ptr<web::NavigationItem> last_committed_item_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  TestNavigationManager* test_navigation_manager_;
  TestWebState test_web_state_;
};

// Tests that failing loading an online version does not mark it read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListFailure) {
  GURL url(kTestURL);
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  test_navigation_manager_->GetPendingItem()->SetURL(url);
  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(test_navigation_manager_->ReloadCalled());
  EXPECT_EQ(test_navigation_manager_->GoToIndexCalled(), -1);
  // Check that |GetLastCommittedItem()| has not been altered.
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetVirtualURL(),
            GURL());
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetURL(), GURL());
  EXPECT_FALSE(entry->IsRead());
}

// Tests that loading an online version of an entry does not alter navigation
// stack and mark entry read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListOnline) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model_->SetEntryDistilledInfo(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);

  test_navigation_manager_->GetPendingItem()->SetURL(url);
  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(test_navigation_manager_->ReloadCalled());
  EXPECT_EQ(test_navigation_manager_->GoToIndexCalled(), -1);
  // Check that |GetLastCommittedItem()| has not been altered.
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetVirtualURL(),
            GURL());
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetURL(), GURL());
  EXPECT_TRUE(entry->IsRead());
}

// Tests that loading an online version of an entry does update navigation
// stack and mark entry read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListDistilledCommitted) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model_->SetEntryDistilledInfo(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  GURL distilled_url = reading_list::OfflineURLForPath(
      entry->DistilledPath(), entry->URL(), entry->DistilledURL());

  // Test on committed entry, there must be no pending item.
  test_navigation_manager_->SetPendingItem(nullptr);
  test_navigation_manager_->GetLastCommittedItem()->SetURL(url);
  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(test_navigation_manager_->ReloadCalled());
  EXPECT_EQ(test_navigation_manager_->GoToIndexCalled(),
            test_navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetVirtualURL(),
            url);
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetURL(),
            distilled_url);
  EXPECT_TRUE(entry->IsRead());
}

// Tests that loading an online version of a pending entry on reload does update
// committed entry, reload, and mark entry read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListDistilledPending) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model_->SetEntryDistilledInfo(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  GURL distilled_url = reading_list::OfflineURLForPath(
      entry->DistilledPath(), entry->URL(), entry->DistilledURL());

  test_navigation_manager_->SetPendingItem(nil);
  test_navigation_manager_->GetLastCommittedItem()->SetURL(url);
  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(test_navigation_manager_->ReloadCalled());
  EXPECT_EQ(test_navigation_manager_->GoToIndexCalled(),
            test_navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetVirtualURL(),
            url);
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetURL(),
            distilled_url);

  EXPECT_TRUE(entry->IsRead());
}
