// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/offline_page_tab_helper.h"

#import <memory>
#import <vector>

#import "base/memory/scoped_refptr.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/time/default_clock.h"
#import "components/reading_list/core/fake_reading_list_model_storage.h"
#import "components/reading_list/core/reading_list_entry.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "components/sync/base/storage_type.h"
#import "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
const char kTestURL[] = "http://foo.test";
const char kTestSecondURL[] = "http://bar.test";
const char kTestTitle[] = "title";
const char kTestDistilledPath[] = "distilled.html";
const char kTestDistilledURL[] = "http://foo.bar/distilled";
}  // namespace

// Test fixture to test loading of Reading list offline pages.
class OfflinePageTabHelperTest : public PlatformTest {
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

    fake_web_state_.SetBrowserState(profile_.get());
    fake_web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());

    OfflinePageTabHelper::CreateForWebState(&fake_web_state_,
                                            reading_list_model());
  }

  ReadingListModel* reading_list_model() {
    return ReadingListModelFactory::GetForProfile(profile_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState fake_web_state_;
};

// Test fixture to test loading of Reading list offline pages with a delayed
// ReadingListModel.
class OfflinePageTabHelperDelayedModelTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    auto storage = std::make_unique<FakeReadingListModelStorage>();
    fake_reading_list_model_storage_ = storage->AsWeakPtr();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(
            [](std::unique_ptr<FakeReadingListModelStorage>& storage,
               web::BrowserState*) -> std::unique_ptr<KeyedService> {
              DCHECK(storage.get());
              return std::make_unique<ReadingListModelImpl>(
                  std::move(storage), syncer::StorageType::kUnspecified,
                  syncer::WipeModelUponSyncDisabledBehavior::kNever,
                  base::DefaultClock::GetInstance());
            },
            base::OwnedRef(std::move(storage))));
    profile_ = std::move(builder).Build();

    fake_web_state_.SetBrowserState(profile_.get());
    fake_web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());

    OfflinePageTabHelper::CreateForWebState(&fake_web_state_,
                                            reading_list_model());
  }

  ReadingListModel* reading_list_model() {
    return ReadingListModelFactory::GetForProfile(profile_.get());
  }

  FakeReadingListModelStorage* fake_reading_list_model_storage() {
    return fake_reading_list_model_storage_.get();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState fake_web_state_;
  base::WeakPtr<FakeReadingListModelStorage> fake_reading_list_model_storage_;
};

// Tests that loading an online version does mark it read.
TEST_F(OfflinePageTabHelperTest, TestLoadReadingListSuccess) {
  GURL url(kTestURL);
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);
  fake_web_state_.SetCurrentURL(url);
  web::FakeNavigationContext context;
  context.SetUrl(url);
  context.SetHasCommitted(true);
  fake_web_state_.OnNavigationStarted(&context);
  fake_web_state_.OnNavigationFinished(&context);
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return fake_web_state_.GetLastLoadedData();
      }));
  EXPECT_FALSE(fake_web_state_.GetLastLoadedData());
  EXPECT_TRUE(entry->IsRead());
  EXPECT_FALSE(OfflinePageTabHelper::FromWebState(&fake_web_state_)
                   ->presenting_offline_page());
}

// Tests that failing loading an online version does not mark it read.
TEST_F(OfflinePageTabHelperTest, TestLoadReadingListFailure) {
  GURL url(kTestURL);
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);
  web::FakeNavigationContext context;
  context.SetUrl(url);
  context.SetHasCommitted(true);
  fake_web_state_.OnNavigationStarted(&context);
  fake_web_state_.OnNavigationFinished(&context);
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return fake_web_state_.GetLastLoadedData();
      }));
  EXPECT_FALSE(fake_web_state_.GetLastLoadedData());
  EXPECT_FALSE(entry->IsRead());
  EXPECT_FALSE(OfflinePageTabHelper::FromWebState(&fake_web_state_)
                   ->presenting_offline_page());
}

// Tests that failing loading an online version will load the distilled version
// and mark it read.
TEST_F(OfflinePageTabHelperTest, TestLoadReadingListDistilled) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model()->SetEntryDistilledInfoIfExists(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);
  fake_web_state_.SetCurrentURL(url);
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  static_cast<web::FakeNavigationManager*>(
      fake_web_state_.GetNavigationManager())
      ->SetLastCommittedItem(item.get());
  context.SetUrl(url);
  fake_web_state_.OnNavigationStarted(&context);
  fake_web_state_.OnNavigationFinished(&context);
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(fake_web_state_.GetLastLoadedData());
  EXPECT_FALSE(entry->IsRead());
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return fake_web_state_.GetLastLoadedData();
      }));
  EXPECT_TRUE(entry->IsRead());
  EXPECT_TRUE(OfflinePageTabHelper::FromWebState(&fake_web_state_)
                  ->presenting_offline_page());
}

// Tests that failing loading an online version does not load distilled
// version if another navigation started.
TEST_F(OfflinePageTabHelperTest, TestLoadReadingListFailureThenNavigate) {
  GURL url(kTestURL);
  GURL second_url(kTestSecondURL);
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetUrl(url);
  fake_web_state_.OnNavigationStarted(&context);
  fake_web_state_.OnNavigationFinished(&context);
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);

  web::FakeNavigationContext second_context;
  second_context.SetUrl(second_url);
  second_context.SetHasCommitted(true);
  fake_web_state_.OnNavigationStarted(&second_context);
  EXPECT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return fake_web_state_.GetLastLoadedData();
      }));
  EXPECT_FALSE(fake_web_state_.GetLastLoadedData());
  EXPECT_FALSE(entry->IsRead());
  EXPECT_FALSE(OfflinePageTabHelper::FromWebState(&fake_web_state_)
                   ->presenting_offline_page());
}

// Tests that OfflinePageTabHelper correctly reports existence of a distilled
// version.
TEST_F(OfflinePageTabHelperTest, TestHasDistilledVersionForOnlineUrl) {
  OfflinePageTabHelper* offline_page_tab_helper =
      OfflinePageTabHelper::FromWebState(&fake_web_state_);
  GURL url(kTestURL);
  EXPECT_FALSE(offline_page_tab_helper->HasDistilledVersionForOnlineUrl(url));
  GURL second_url(kTestSecondURL);
  EXPECT_FALSE(
      offline_page_tab_helper->HasDistilledVersionForOnlineUrl(second_url));

  std::string distilled_path = kTestDistilledPath;
  reading_list_model()->SetEntryDistilledInfoIfExists(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  EXPECT_TRUE(offline_page_tab_helper->HasDistilledVersionForOnlineUrl(url));
}

// Tests that OfflinePageTabHelper correctly shows Offline page if model takes
// a long time to load.
TEST_F(OfflinePageTabHelperDelayedModelTest, TestLateReadingListModelLoading) {
  OfflinePageTabHelper* offline_page_tab_helper =
      OfflinePageTabHelper::FromWebState(&fake_web_state_);
  GURL url(kTestURL);
  EXPECT_FALSE(offline_page_tab_helper->HasDistilledVersionForOnlineUrl(url));
  web::FakeNavigationContext context;

  context.SetHasCommitted(true);
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  static_cast<web::FakeNavigationManager*>(
      fake_web_state_.GetNavigationManager())
      ->SetLastCommittedItem(item.get());
  context.SetUrl(url);
  fake_web_state_.OnNavigationStarted(&context);
  fake_web_state_.OnNavigationFinished(&context);
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return fake_web_state_.GetLastLoadedData();
      }));
  EXPECT_FALSE(offline_page_tab_helper->presenting_offline_page());
  // Complete the reading list model load from storage.
  std::vector<scoped_refptr<ReadingListEntry>> initial_entries;
  initial_entries.push_back(base::MakeRefCounted<ReadingListEntry>(
      GURL(kTestURL), kTestTitle, base::Time()));
  initial_entries.back()->SetDistilledInfo(base::FilePath(kTestDistilledPath),
                                           GURL(kTestDistilledURL), 50,
                                           base::Time::FromTimeT(100));
  fake_reading_list_model_storage()->TriggerLoadCompletion(
      std::move(initial_entries));
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return fake_web_state_.GetLastLoadedData();
      }));
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);
  EXPECT_TRUE(entry->IsRead());
  EXPECT_TRUE(offline_page_tab_helper->presenting_offline_page());
}
