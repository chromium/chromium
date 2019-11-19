// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/offline_page_tab_helper.h"

#include <memory>

#include "base/path_service.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "base/time/default_clock.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kTestURL[] = "http://foo.test";
const char kTestSecondURL[] = "http://bar.test";
const char kTestTitle[] = "title";
const char kTestDistilledPath[] = "distilled.html";
const char kTestDistilledURL[] = "http://foo.bar/distilled";
const char kTestDirectory[] = "ios/testing/data/";

// A simple implementation of ReadingListModel that only support functions
// needed to load an offline page.
class FakeReadingListModel : public ReadingListModel {
 public:
  ~FakeReadingListModel() override {}
  bool loaded() const override { return loaded_; }

  syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() override {
    NOTREACHED();
    return nullptr;
  }

  const std::vector<GURL> Keys() const override {
    NOTREACHED();
    return std::vector<GURL>();
  }

  size_t size() const override {
    DCHECK(loaded_);
    return 0;
  }

  size_t unread_size() const override {
    NOTREACHED();
    return 0;
  }

  size_t unseen_size() const override {
    NOTREACHED();
    return 0;
  }

  void MarkAllSeen() override { NOTREACHED(); }

  bool DeleteAllEntries() override {
    NOTREACHED();
    return false;
  }

  bool GetLocalUnseenFlag() const override {
    NOTREACHED();
    return false;
  }

  void ResetLocalUnseenFlag() override { NOTREACHED(); }

  const ReadingListEntry* GetEntryByURL(const GURL& gurl) const override {
    DCHECK(loaded_);
    if (entry_->URL() == gurl) {
      return entry_;
    }
    return nullptr;
  }

  const ReadingListEntry* GetFirstUnreadEntry(bool distilled) const override {
    NOTREACHED();
    return nullptr;
  }

  const ReadingListEntry& AddEntry(const GURL& url,
                                   const std::string& title,
                                   reading_list::EntrySource source) override {
    NOTREACHED();
    return *entry_;
  }

  void RemoveEntryByURL(const GURL& url) override { NOTREACHED(); }

  void SetReadStatus(const GURL& url, bool read) override {
    if (entry_->URL() == url) {
      entry_->SetRead(true, base::Time());
    }
  }

  void SetEntryTitle(const GURL& url, const std::string& title) override {
    NOTREACHED();
  }

  void SetEntryDistilledState(
      const GURL& url,
      ReadingListEntry::DistillationState state) override {
    NOTREACHED();
  }

  void SetEntryDistilledInfo(const GURL& url,
                             const base::FilePath& distilled_path,
                             const GURL& distilled_url,
                             int64_t distilation_size,
                             const base::Time& distilation_time) override {
    NOTREACHED();
  }

  void SetContentSuggestionsExtra(
      const GURL& url,
      const reading_list::ContentSuggestionsExtra& extra) override {
    NOTREACHED();
  }

  void SetEntry(ReadingListEntry* entry) { entry_ = entry; }
  void SetLoaded() {
    loaded_ = true;
    for (auto& observer : observers_) {
      observer.ReadingListModelLoaded(this);
    }
  }

 private:
  ReadingListEntry* entry_ = nullptr;
  bool loaded_ = false;
};
}

// Test fixture to test loading of Reading list offline pages.
class OfflinePageTabHelperTest : public web::WebTest {
 public:
  void SetUp() override {
    web::WebTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    test_data_dir = test_data_dir.AppendASCII(kTestDirectory);
    test_cbs_builder.SetPath(test_data_dir);
    chrome_browser_state_ = test_cbs_builder.Build();
    test_web_state_.SetBrowserState(chrome_browser_state_.get());
    test_web_state_.SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        /*storage_layer*/ nullptr, /*pref_service*/ nullptr,
        base::DefaultClock::GetInstance());
    reading_list_model_->AddEntry(GURL(kTestURL), kTestTitle,
                                  reading_list::ADDED_VIA_CURRENT_APP);
    OfflinePageTabHelper::CreateForWebState(&test_web_state_,
                                            reading_list_model_.get());
  }

 protected:
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  web::TestWebState test_web_state_;
};

// Test fixture to test loading of Reading list offline pages with a delayed
// ReadingListModel.
class OfflinePageTabHelperDelayedModelTest : public web::WebTest {
  void SetUp() override {
    web::WebTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    test_data_dir = test_data_dir.AppendASCII(kTestDirectory);
    test_cbs_builder.SetPath(test_data_dir);
    chrome_browser_state_ = test_cbs_builder.Build();
    test_web_state_.SetBrowserState(chrome_browser_state_.get());
    test_web_state_.SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    fake_reading_list_model_ = std::make_unique<FakeReadingListModel>();
    GURL url(kTestURL);
    entry_ = std::make_unique<ReadingListEntry>(url, kTestTitle, base::Time());
    std::string distilled_path = kTestDistilledPath;
    entry_->SetDistilledInfo(base::FilePath(distilled_path),
                             GURL(kTestDistilledURL), 50,
                             base::Time::FromTimeT(100));
    fake_reading_list_model_->SetEntry(entry_.get());
    OfflinePageTabHelper::CreateForWebState(&test_web_state_,
                                            fake_reading_list_model_.get());
  }

 protected:
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<FakeReadingListModel> fake_reading_list_model_;
  web::TestWebState test_web_state_;
  std::unique_ptr<ReadingListEntry> entry_;
};

// Tests that loading an online version does mark it read.
TEST_F(OfflinePageTabHelperTest, TestLoadReadingListSuccess) {
  GURL url(kTestURL);
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  test_web_state_.SetCurrentURL(url);
  web::FakeNavigationContext context;
  context.SetUrl(url);
  context.SetHasCommitted(true);
  test_web_state_.OnNavigationStarted(&context);
  test_web_state_.OnNavigationFinished(&context);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return test_web_state_.GetLastLoadedData();
      }));
  EXPECT_FALSE(test_web_state_.GetLastLoadedData());
  EXPECT_TRUE(entry->IsRead());
  EXPECT_FALSE(OfflinePageTabHelper::FromWebState(&test_web_state_)
                   ->presenting_offline_page());
}

// Tests that failing loading an online version does not mark it read.
TEST_F(OfflinePageTabHelperTest, TestLoadReadingListFailure) {
  GURL url(kTestURL);
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  web::FakeNavigationContext context;
  context.SetUrl(url);
  context.SetHasCommitted(true);
  test_web_state_.OnNavigationStarted(&context);
  test_web_state_.OnNavigationFinished(&context);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return test_web_state_.GetLastLoadedData();
      }));
  EXPECT_FALSE(test_web_state_.GetLastLoadedData());
  EXPECT_FALSE(entry->IsRead());
  EXPECT_FALSE(OfflinePageTabHelper::FromWebState(&test_web_state_)
                   ->presenting_offline_page());
}

// Tests that failing loading an online version will load the distilled version
// and mark it read.
TEST_F(OfflinePageTabHelperTest, TestLoadReadingListDistilled) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model_->SetEntryDistilledInfo(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  test_web_state_.SetCurrentURL(url);
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  static_cast<web::TestNavigationManager*>(
      test_web_state_.GetNavigationManager())
      ->SetLastCommittedItem(item.get());
  context.SetUrl(url);
  test_web_state_.OnNavigationStarted(&context);
  test_web_state_.OnNavigationFinished(&context);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(test_web_state_.GetLastLoadedData());
  EXPECT_FALSE(entry->IsRead());
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return test_web_state_.GetLastLoadedData();
      }));
  EXPECT_TRUE(entry->IsRead());
  EXPECT_TRUE(OfflinePageTabHelper::FromWebState(&test_web_state_)
                  ->presenting_offline_page());
}

// Tests that failing loading an online version does not load distilled
// version if another navigation started.
TEST_F(OfflinePageTabHelperTest, TestLoadReadingListFailureThenNavigate) {
  GURL url(kTestURL);
  GURL second_url(kTestSecondURL);
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetUrl(url);
  test_web_state_.OnNavigationStarted(&context);
  test_web_state_.OnNavigationFinished(&context);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);

  web::FakeNavigationContext second_context;
  second_context.SetUrl(second_url);
  second_context.SetHasCommitted(true);
  test_web_state_.OnNavigationStarted(&second_context);
  EXPECT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return test_web_state_.GetLastLoadedData();
      }));
  EXPECT_FALSE(test_web_state_.GetLastLoadedData());
  EXPECT_FALSE(entry->IsRead());
  EXPECT_FALSE(OfflinePageTabHelper::FromWebState(&test_web_state_)
                   ->presenting_offline_page());
}

// Tests that OfflinePageTabHelper correctly reports existence of a distilled
// version.
TEST_F(OfflinePageTabHelperTest, TestHasDistilledVersionForOnlineUrl) {
  OfflinePageTabHelper* offline_page_tab_helper =
      OfflinePageTabHelper::FromWebState(&test_web_state_);
  GURL url(kTestURL);
  EXPECT_FALSE(offline_page_tab_helper->HasDistilledVersionForOnlineUrl(url));
  GURL second_url(kTestSecondURL);
  EXPECT_FALSE(
      offline_page_tab_helper->HasDistilledVersionForOnlineUrl(second_url));

  std::string distilled_path = kTestDistilledPath;
  reading_list_model_->SetEntryDistilledInfo(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  EXPECT_TRUE(offline_page_tab_helper->HasDistilledVersionForOnlineUrl(url));
}

// Tests that OfflinePageTabHelper correctly shows Offline page if model takes
// a long time to load.
TEST_F(OfflinePageTabHelperDelayedModelTest, TestLateReadingListModelLoading) {
  OfflinePageTabHelper* offline_page_tab_helper =
      OfflinePageTabHelper::FromWebState(&test_web_state_);
  GURL url(kTestURL);
  EXPECT_FALSE(offline_page_tab_helper->HasDistilledVersionForOnlineUrl(url));
  web::FakeNavigationContext context;

  context.SetHasCommitted(true);
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  static_cast<web::TestNavigationManager*>(
      test_web_state_.GetNavigationManager())
      ->SetLastCommittedItem(item.get());
  context.SetUrl(url);
  test_web_state_.OnNavigationStarted(&context);
  test_web_state_.OnNavigationFinished(&context);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return test_web_state_.GetLastLoadedData();
      }));
  EXPECT_FALSE(entry_->IsRead());
  EXPECT_FALSE(offline_page_tab_helper->presenting_offline_page());
  fake_reading_list_model_->SetLoaded();
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return test_web_state_.GetLastLoadedData();
      }));
  EXPECT_TRUE(entry_->IsRead());
  EXPECT_TRUE(offline_page_tab_helper->presenting_offline_page());
}
