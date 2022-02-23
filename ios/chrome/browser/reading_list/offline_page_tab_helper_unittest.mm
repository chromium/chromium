// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/offline_page_tab_helper.h"

#include <memory>

#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "base/time/default_clock.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/reading_list/fake_reading_list_model.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kTestURL[] = "http://foo.test";
const char kTestSecondURL[] = "http://bar.test";
const char kTestTitle[] = "title";
const char kTestDistilledPath[] = "distilled.html";
const char kTestDistilledURL[] = "http://foo.bar/distilled";
}

// Test fixture to test loading of Reading list offline pages.
class OfflinePageTabHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              auto model = std::make_unique<ReadingListModelImpl>(
                  /* storage_layer */ nullptr, /* pref_service */ nullptr,
                  base::DefaultClock::GetInstance());

              model->AddEntry(GURL(kTestURL), kTestTitle,
                              reading_list::ADDED_VIA_CURRENT_APP);

              return model;
            }));
    browser_state_ = builder.Build();

    fake_web_state_.SetBrowserState(browser_state_.get());
    fake_web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());

    OfflinePageTabHelper::CreateForWebState(&fake_web_state_,
                                            reading_list_model());
  }

  ReadingListModel* reading_list_model() {
    return ReadingListModelFactory::GetForBrowserState(browser_state_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  web::FakeWebState fake_web_state_;
};

// Test fixture to test loading of Reading list offline pages with a delayed
// ReadingListModel.
class OfflinePageTabHelperDelayedModelTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              auto model = std::make_unique<FakeReadingListModel>();

              ReadingListEntry entry(GURL(kTestURL), kTestTitle, base::Time());
              entry.SetDistilledInfo(base::FilePath(kTestDistilledPath),
                                     GURL(kTestDistilledURL), 50,
                                     base::Time::FromTimeT(100));
              model->SetEntry(std::move(entry));

              return model;
            }));
    browser_state_ = builder.Build();

    fake_web_state_.SetBrowserState(browser_state_.get());
    fake_web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());

    OfflinePageTabHelper::CreateForWebState(&fake_web_state_,
                                            reading_list_model());
  }

  FakeReadingListModel* reading_list_model() {
    return static_cast<FakeReadingListModel*>(
        ReadingListModelFactory::GetForBrowserState(browser_state_.get()));
  }

  const ReadingListEntry* entry() { return reading_list_model()->entry(); }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  web::FakeWebState fake_web_state_;
};

// Tests that loading an online version does mark it read.
TEST_F(OfflinePageTabHelperTest, TestLoadReadingListSuccess) {
  GURL url(kTestURL);
  const ReadingListEntry* entry = reading_list_model()->GetEntryByURL(url);
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
  const ReadingListEntry* entry = reading_list_model()->GetEntryByURL(url);
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
  reading_list_model()->SetEntryDistilledInfo(
      url, base::FilePath(distilled_path), GURL(kTestDistilledURL), 50,
      base::Time::FromTimeT(100));
  const ReadingListEntry* entry = reading_list_model()->GetEntryByURL(url);
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
  const ReadingListEntry* entry = reading_list_model()->GetEntryByURL(url);
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
  reading_list_model()->SetEntryDistilledInfo(
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
  EXPECT_FALSE(entry()->IsRead());
  EXPECT_FALSE(offline_page_tab_helper->presenting_offline_page());
  reading_list_model()->SetLoaded();
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return fake_web_state_.GetLastLoadedData();
      }));
  EXPECT_TRUE(entry()->IsRead());
  EXPECT_TRUE(offline_page_tab_helper->presenting_offline_page());
}
