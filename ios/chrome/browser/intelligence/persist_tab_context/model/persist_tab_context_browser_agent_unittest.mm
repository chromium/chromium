// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"

#import <algorithm>

#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/metrics/field_trial_params.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/page_content_annotations/core/page_content_annotations_features.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/metrics/persist_tab_context_metrics.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_service.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/paths/paths_internal.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
// TODO(crbug.com/447646545): Extract these constants to a shared header file.
constexpr std::string kPageContextPrefix = "page_context_";
constexpr std::string kProtoSuffix = ".proto";
constexpr std::string kPersistedTabContextsDir = "persisted_tab_contexts";
constexpr base::TimeDelta kPurgeTaskDelay = base::Seconds(3);
// The delay applied before cleanup tasks are executed
// after the PageContentCache is initialized.
constexpr base::TimeDelta kCleanupTaskDelay = base::Seconds(25);
}  // namespace

class PersistTabContextBrowserAgentTest
    : public PlatformTest,
      public testing::WithParamInterface<PersistTabStorageType> {
 protected:
  PersistTabContextBrowserAgentTest()
      : task_environment_(web::WebTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    PlatformTest::SetUp();

    InitFeatures(/*extract_on_page_load=*/false);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build(temp_dir_.GetPath());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_list_ = browser_->GetWebStateList();
    PersistTabContextBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = PersistTabContextBrowserAgent::FromBrowser(browser_.get());
    if (GetParam() == PersistTabStorageType::kSQLite) {
      PageContentCacheService* service =
          PageContentCacheServiceFactory::GetForProfile(profile_.get());
      ASSERT_TRUE(base::test::RunUntil(
          [&]() { return service->IsCacheInitialized(); }));
    }
  }

  // Initializes the feature list for the current storage type. When
  // `extract_on_page_load` is true, also enables page-load extraction timing.
  void InitFeatures(bool extract_on_page_load) {
    base::FieldTrialParams params = {
        {kPersistTabContextStorageParam,
         GetParam() == PersistTabStorageType::kSQLite ? "1" : "0"},
        {kPersistTabContextExtractionTimingParam,
         extract_on_page_load ? "1" : "0"}};
    base::test::FeatureRefAndParams storage_feature =
        GetParam() == PersistTabStorageType::kSQLite
            ? base::test::FeatureRefAndParams(
                  page_content_annotations::features::kPageContentCache, {})
            : base::test::FeatureRefAndParams(kCleanupPersistedTabContexts, {});
    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters(
        {{kPersistTabContext, params}, storage_feature}, {});
  }

  // Recreates the agent with page-load extraction enabled. The extraction
  // timing param is only read when the agent is constructed.
  void RecreateAgentWithPageLoadExtraction() {
    InitFeatures(/*extract_on_page_load=*/true);
    agent_ = nullptr;
    PersistTabContextBrowserAgent::RemoveFromBrowser(browser_.get());
    PersistTabContextBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = PersistTabContextBrowserAgent::FromBrowser(browser_.get());
  }

  // Inserts a realized, eligible tab with `id` into the WebStateList.
  web::FakeWebState* InsertEligibleTab(web::WebStateID id) {
    auto tab = std::make_unique<web::FakeWebState>(id);
    tab->SetCurrentURL(
        GURL("http://example.com/" + base::NumberToString(id.identifier())));
    tab->SetIsRealized(true);
    tab->SetContentsMimeType("text/html");
    web::FakeWebState* tab_ptr = tab.get();
    web_state_list_->InsertWebState(std::move(tab));
    return tab_ptr;
  }

  base::FilePath GetStorageDir() {
    // The agent's storage directory is based on the profile path.
    return profile_->GetStatePath().Append(kPersistedTabContextsDir);
  }

  base::FilePath GetPathForWebStateIdForTest(web::WebStateID web_state_id) {
    return GetStorageDir().Append(FILE_PATH_LITERAL(
        kPageContextPrefix + base::NumberToString(web_state_id.identifier()) +
        kProtoSuffix));
  }

  void CreateDummyContextFile(
      web::WebStateID web_state_id,
      base::Time last_modified_time = base::Time::Now()) {
    optimization_guide::proto::PageContext context;
    std::string title =
        "test_title_" + base::NumberToString(web_state_id.identifier());
    std::string url_spec =
        "http://example.com/" + base::NumberToString(web_state_id.identifier());

    context.set_title(title);
    context.set_url(url_spec);

    if (GetParam() == PersistTabStorageType::kSQLite) {
      PageContentCacheService* service =
          PageContentCacheServiceFactory::GetForProfile(profile_.get());

      service->CachePageContent(web_state_id.identifier(), GURL(url_spec),
                                last_modified_time, last_modified_time,
                                context);

      base::test::TestFuture<std::vector<int64_t>> future;
      service->GetAllTabIds(future.GetCallback());
      ASSERT_TRUE(future.Wait());

    } else {
      std::string serialized_context;
      ASSERT_TRUE(context.SerializeToString(&serialized_context));

      base::FilePath path = GetPathForWebStateIdForTest(web_state_id);
      ASSERT_TRUE(base::CreateDirectory(path.DirName()));
      ASSERT_TRUE(base::WriteFile(path, serialized_context));
      ASSERT_TRUE(
          base::TouchFile(path, last_modified_time, last_modified_time));
    }
  }

  // Helper to wait for the service's background sequence to process pending
  // tasks.
  void WaitForServiceSequence(PageContentCacheService* bridge) {
    base::RunLoop run_loop;
    bridge->GetAllTabIds(base::BindOnce(
        [](base::RunLoop* loop, std::vector<int64_t> ignored) { loop->Quit(); },
        &run_loop));
    run_loop.Run();
  }

  std::unique_ptr<web::FakeWebState> CreateFakeWebState(
      const GURL& url,
      const std::string& mime_type = "text/html") {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetCurrentURL(url);
    web_state->SetContentsMimeType(mime_type);
    return web_state;
  }

  void CallOnPageContextExtracted(
      base::WeakPtr<web::WebState> weak_web_state,
      PageContextWrapperCallbackResponse response,
      std::optional<GURL> expected_url = std::nullopt) {
    GURL url = expected_url.value_or(
        weak_web_state ? weak_web_state->GetLastCommittedURL() : GURL());
    agent_->OnPageContextExtracted(weak_web_state, url, std::move(response));
  }

  void RunCacheCleanup() { agent_->RunCacheCleanup(); }

  // Returns the agent's in-flight extraction wrapper, or nil if none.
  PageContextWrapper* GetPageContextWrapper() {
    return agent_->page_context_wrapper_;
  }

  web::WebTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<PersistTabContextBrowserAgent> agent_;
  web::WebStateID test_web_state_id_ = web::WebStateID::FromSerializedValue(1);
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

// Instantiate the test suite for both FileSystem and SQLite configurations.
INSTANTIATE_TEST_SUITE_P(PersistTabContextStorageTests,
                         PersistTabContextBrowserAgentTest,
                         testing::Values(PersistTabStorageType::kFileSystem,
                                         PersistTabStorageType::kSQLite));

TEST_P(PersistTabContextBrowserAgentTest, TestStorageDifferenceHistogram) {
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester_
               .GetAllSamples(kPersistTabContextStorageDifferenceHistogram)
               .size() > 0;
  }));
}

TEST_P(PersistTabContextBrowserAgentTest, TestGetSingleContextAsync_NotFound) {
  base::RunLoop run_loop;
  agent_->GetSingleContextAsync(
      base::NumberToString(test_web_state_id_.identifier()),
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::optional<std::unique_ptr<
                 optimization_guide::proto::PageContext>> context) {
            EXPECT_FALSE(context.has_value());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  // Verify result metric: FileNotFound.
  histogram_tester_.ExpectBucketCount(
      kReadTabContextResultHistogram,
      IOSPersistTabContextReadResult::kFileNotFound, 1);
}

TEST_P(PersistTabContextBrowserAgentTest, TestGetSingleContextAsync_Found) {
  auto tab = std::make_unique<web::FakeWebState>(test_web_state_id_);
  tab->SetCurrentURL(GURL("http://example.com/1"));
  tab->SetIsRealized(true);
  web_state_list_->InsertWebState(std::move(tab));

  CreateDummyContextFile(test_web_state_id_);
  base::RunLoop run_loop;
  agent_->GetSingleContextAsync(
      base::NumberToString(test_web_state_id_.identifier()),
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::optional<std::unique_ptr<
                 optimization_guide::proto::PageContext>> context) {
            ASSERT_TRUE(context.has_value());
            EXPECT_EQ((*context)->title(), "test_title_1");
            EXPECT_EQ((*context)->url(), "http://example.com/1");
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  // Verify result metric: Success.
  histogram_tester_.ExpectBucketCount(kReadTabContextResultHistogram,
                                      IOSPersistTabContextReadResult::kSuccess,
                                      1);

  // Verify time metric.
  histogram_tester_.ExpectTotalCount(kPersistTabContextReadTimeHistogram, 1);
}

TEST_P(PersistTabContextBrowserAgentTest, TestStartupMetricEmitted) {
  if (GetParam() != PersistTabStorageType::kSQLite) {
    return;
  }
  CreateDummyContextFile(test_web_state_id_);
  // Fast forward to trigger the cleanup task.
  task_environment_.FastForwardBy(kPurgeTaskDelay + kCleanupTaskDelay +
                                  base::Seconds(1));
  // Verify that the histogram contains at least one sample.
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.PageContentCache.TotalCacheSize", 1);
}

TEST_P(PersistTabContextBrowserAgentTest, TestGetMultipleContextsAsync) {
  web::WebStateID id1 = web::WebStateID::FromSerializedValue(1);
  web::WebStateID id2 = web::WebStateID::FromSerializedValue(2);
  web::WebStateID id3 = web::WebStateID::FromSerializedValue(3);
  for (web::WebStateID id : {id1, id2, id3}) {
    auto tab = std::make_unique<web::FakeWebState>(id);
    tab->SetCurrentURL(
        GURL("http://example.com/" + base::NumberToString(id.identifier())));
    tab->SetIsRealized(true);
    web_state_list_->InsertWebState(std::move(tab));
  }
  CreateDummyContextFile(id1);
  CreateDummyContextFile(id3);

  base::RunLoop run_loop;
  agent_->GetMultipleContextsAsync(
      {base::NumberToString(id1.identifier()),
       base::NumberToString(id2.identifier()),
       base::NumberToString(id3.identifier())},
      base::BindOnce(
          [](base::RunLoop* run_loop,
             PersistTabContextBrowserAgent::PageContextMap context_map) {
            EXPECT_TRUE(context_map.at("1").has_value());
            EXPECT_FALSE(context_map.at("2").has_value());
            EXPECT_TRUE(context_map.at("3").has_value());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  // Total reads = 3.
  histogram_tester_.ExpectTotalCount(kReadTabContextResultHistogram, 3);
  // 2 Success.
  histogram_tester_.ExpectBucketCount(kReadTabContextResultHistogram,
                                      IOSPersistTabContextReadResult::kSuccess,
                                      2);
  // 1 FileNotFound.
  histogram_tester_.ExpectBucketCount(
      kReadTabContextResultHistogram,
      IOSPersistTabContextReadResult::kFileNotFound, 1);
}

TEST_P(PersistTabContextBrowserAgentTest, TestPurgeExpiredContexts) {
  if (GetParam() == PersistTabStorageType::kSQLite) {
    // Expiration handling is internal to PageContentCache.
    return;
  }

  base::TimeDelta test_ttl = base::Days(7);

  web::WebStateID id_expired = web::WebStateID::FromSerializedValue(100);
  base::Time expired_time = base::Time::Now() - test_ttl - base::Days(1);
  CreateDummyContextFile(id_expired, expired_time);
  base::FilePath path_expired = GetPathForWebStateIdForTest(id_expired);
  ASSERT_TRUE(base::PathExists(path_expired));

  web::WebStateID id_valid = web::WebStateID::FromSerializedValue(101);
  base::Time valid_time = base::Time::Now() - test_ttl + base::Days(1);
  CreateDummyContextFile(id_valid, valid_time);
  base::FilePath path_valid = GetPathForWebStateIdForTest(id_valid);
  ASSERT_TRUE(base::PathExists(path_valid));

  task_environment_.FastForwardBy(kPurgeTaskDelay + base::Milliseconds(100));
  // Wait until the expired file is confirmed to be deleted.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !base::PathExists(path_expired); }));

  EXPECT_FALSE(base::PathExists(path_expired))
      << "Expired context file was not purged.";
  EXPECT_TRUE(base::PathExists(path_valid))
      << "Valid context file was incorrectly purged.";
}
TEST_P(PersistTabContextBrowserAgentTest, WasHiddenWithNullWebState) {
  agent_->WasHidden(nullptr);
  if (GetParam() == PersistTabStorageType::kSQLite) {
    PageContentCacheService* bridge =
        PageContentCacheServiceFactory::GetForProfile(profile_.get());

    base::RunLoop run_loop;
    bridge->GetAllTabIds(base::BindOnce(
        [](base::RunLoop* run_loop, std::vector<int64_t> tab_ids) {
          EXPECT_TRUE(tab_ids.empty())
              << "Database contains entries but should be empty!";
          run_loop->Quit();
        },
        &run_loop));
    run_loop.Run();
  } else {
    task_environment_.FastForwardBy(base::Milliseconds(100));

    base::FilePath storage_dir = GetStorageDir();
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return base::DirectoryExists(storage_dir); }));

    base::FileEnumerator enumerator(storage_dir, /*recursive=*/false,
                                    base::FileEnumerator::FILES);
    EXPECT_TRUE(enumerator.Next().empty())
        << "File was created for null web state";
  }
}

TEST_P(PersistTabContextBrowserAgentTest,
       WasHiddenWithNullWebStateDoesNotCrash) {
  EXPECT_NO_FATAL_FAILURE(agent_->WasHidden(nullptr));
}

TEST_P(PersistTabContextBrowserAgentTest,
       TestStorageTypeMigrationDeletesLegacyFilesystemFiles) {
  if (GetParam() != PersistTabStorageType::kSQLite) {
    return;
  }

  // We explicitly create a file on disk here (bypassing CreateDummyContextFile)
  // because we want to test that the Agent deletes files it sees on disk
  // when it is configured for SQLite.
  web::WebStateID id1 = web::WebStateID::FromSerializedValue(1);
  base::FilePath legacy_file = GetPathForWebStateIdForTest(id1);

  ASSERT_TRUE(base::CreateDirectory(legacy_file.DirName()));
  ASSERT_TRUE(base::WriteFile(legacy_file, "legacy_proto_data"));
  ASSERT_TRUE(base::PathExists(legacy_file));

  task_environment_.FastForwardBy(kPurgeTaskDelay + base::Milliseconds(100));

  EXPECT_FALSE(base::PathExists(legacy_file));
}

TEST_P(PersistTabContextBrowserAgentTest,
       TestStorageTypeMigrationDeletesLegacySqliteDBFiles) {
  if (GetParam() != PersistTabStorageType::kFileSystem) {
    return;
  }

  // We explicitly create an old sqlite DB directory here to test
  // that the Agent deletes it when it is configured for FileSystem storage.
  base::FilePath legacy_db_dir =
      PageContentCacheServiceFactory::GetStoragePathForProfile(profile_.get());
  ASSERT_TRUE(base::CreateDirectory(legacy_db_dir));

  // Ensure the directory is created effectively mimicking a legacy state.
  ASSERT_TRUE(base::DirectoryExists(legacy_db_dir));

  // Trigger the task runner to execute the cleanup task.
  task_environment_.FastForwardBy(kPurgeTaskDelay + base::Milliseconds(100));

  EXPECT_FALSE(base::DirectoryExists(legacy_db_dir));
}

// Tests that when a tab is hidden and its URL is ineligible for persistence,
// any existing persisted context for that tab is deleted.
TEST_P(PersistTabContextBrowserAgentTest,
       WasHiddenWithIneligibleURLDeletesContext) {
  std::unique_ptr<web::FakeWebState> web_state =
      CreateFakeWebState(GURL("chrome://newtab"));
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  CreateDummyContextFile(web_state_id);

  agent_->WasHidden(web_state.get());

  if (GetParam() == PersistTabStorageType::kSQLite) {
    PageContentCacheService* service =
        PageContentCacheServiceFactory::GetForProfile(profile_.get());
    base::RunLoop run_loop;
    service->GetAllTabIds(base::BindOnce(
        [](base::RunLoop* run_loop, web::WebStateID id,
           std::vector<int64_t> tab_ids) {
          EXPECT_TRUE(std::find(tab_ids.begin(), tab_ids.end(),
                                id.identifier()) == tab_ids.end());
          run_loop->Quit();
        },
        &run_loop, web_state_id));
    run_loop.Run();
  } else {
    task_environment_.FastForwardBy(base::Milliseconds(100));
    EXPECT_FALSE(base::PathExists(GetPathForWebStateIdForTest(web_state_id)));
  }
}

// Tests that when page context extraction fails, any existing persisted
// context for that tab is deleted.
TEST_P(PersistTabContextBrowserAgentTest,
       OnPageContextExtractedFailureDeletesContext) {
  std::unique_ptr<web::FakeWebState> web_state =
      CreateFakeWebState(GURL("http://example.com"));
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  CreateDummyContextFile(web_state_id);

  CallOnPageContextExtracted(
      web_state->GetWeakPtr(),
      base::unexpected(PageContextWrapperError::kGenericError));

  if (GetParam() == PersistTabStorageType::kSQLite) {
    PageContentCacheService* service =
        PageContentCacheServiceFactory::GetForProfile(profile_.get());
    base::RunLoop run_loop;
    service->GetAllTabIds(base::BindOnce(
        [](base::RunLoop* run_loop, web::WebStateID id,
           std::vector<int64_t> tab_ids) {
          EXPECT_TRUE(std::find(tab_ids.begin(), tab_ids.end(),
                                id.identifier()) == tab_ids.end());
          run_loop->Quit();
        },
        &run_loop, web_state_id));
    run_loop.Run();
  } else {
    task_environment_.FastForwardBy(base::Milliseconds(100));
    EXPECT_FALSE(base::PathExists(GetPathForWebStateIdForTest(web_state_id)));
  }
}

// Test that a committed navigation in a hidden realized tab deletes its
// persisted context from storage.
TEST_P(PersistTabContextBrowserAgentTest,
       DidFinishNavigationDeletesContextForHiddenTab) {
  web::WebStateID hidden_id = web::WebStateID::FromSerializedValue(201);
  auto hidden_tab = std::make_unique<web::FakeWebState>(hidden_id);
  hidden_tab->SetCurrentURL(GURL("http://example.com/201"));
  hidden_tab->SetIsRealized(true);
  hidden_tab->SetContentsMimeType("text/html");
  web::FakeWebState* hidden_tab_ptr = hidden_tab.get();
  web_state_list_->InsertWebState(
      std::move(hidden_tab),
      WebStateList::InsertionParams::Automatic().Activate(true));

  CreateDummyContextFile(hidden_id);

  // Insert and activate a second tab so the first tab is no longer active.
  web::WebStateID active_id = web::WebStateID::FromSerializedValue(202);
  auto active_tab = std::make_unique<web::FakeWebState>(active_id);
  active_tab->SetCurrentURL(GURL("http://example.com/202"));
  active_tab->SetIsRealized(true);
  web_state_list_->InsertWebState(
      std::move(active_tab),
      WebStateList::InsertionParams::Automatic().Activate(true));

  // Commit a navigation in the hidden tab.
  hidden_tab_ptr->SetCurrentURL(GURL("http://victim.example/account"));
  web::FakeNavigationContext nav_context;
  nav_context.SetHasCommitted(true);
  nav_context.SetIsSameDocument(false);
  hidden_tab_ptr->OnNavigationFinished(&nav_context);

  // Restore the original URL before reading to prove DidFinishNavigation
  // deleted the stored entry rather than ValidateReadContext rejecting a URL
  // mismatch.
  hidden_tab_ptr->SetCurrentURL(GURL("http://example.com/201"));

  base::test::TestFuture<
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>
      future;
  agent_->GetSingleContextAsync(base::NumberToString(hidden_id.identifier()),
                                future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

// Test that a committed navigation in one tab does not cancel an in-flight
// extraction for another tab. The extraction wrapper is shared across all
// observed tabs, so resetting it on navigation would drop unrelated work.
TEST_P(PersistTabContextBrowserAgentTest,
       NavigationInOtherTabDoesNotCancelInFlightExtraction) {
  web::WebStateID extracting_id = web::WebStateID::FromSerializedValue(211);
  auto extracting_tab = std::make_unique<web::FakeWebState>(extracting_id);
  extracting_tab->SetCurrentURL(GURL("http://example.com/211"));
  extracting_tab->SetIsRealized(true);
  extracting_tab->SetContentsMimeType("text/html");
  web::FakeWebState* extracting_tab_ptr = extracting_tab.get();
  web_state_list_->InsertWebState(std::move(extracting_tab));

  web::WebStateID navigating_id = web::WebStateID::FromSerializedValue(212);
  auto navigating_tab = std::make_unique<web::FakeWebState>(navigating_id);
  navigating_tab->SetCurrentURL(GURL("http://example.com/212"));
  navigating_tab->SetIsRealized(true);
  navigating_tab->SetContentsMimeType("text/html");
  web::FakeWebState* navigating_tab_ptr = navigating_tab.get();
  web_state_list_->InsertWebState(std::move(navigating_tab));

  // Start an extraction for the first tab. Low-priority extractions post their
  // work, so the wrapper stays in flight until the run loop spins.
  agent_->WasHidden(extracting_tab_ptr);
  PageContextWrapper* in_flight_wrapper = GetPageContextWrapper();
  ASSERT_NE(nil, in_flight_wrapper);

  // Commit a navigation in the other tab.
  navigating_tab_ptr->SetCurrentURL(GURL("http://example.com/212/next"));
  web::FakeNavigationContext nav_context;
  nav_context.SetHasCommitted(true);
  navigating_tab_ptr->OnNavigationFinished(&nav_context);

  // The first tab's extraction must still be in flight.
  EXPECT_EQ(in_flight_wrapper, GetPageContextWrapper());
}

// Test that a page load does not start an extraction when page-load extraction
// timing is disabled (the default).
TEST_P(PersistTabContextBrowserAgentTest,
       PageLoadedDoesNotExtractWhenTimingDisabled) {
  web::FakeWebState* tab =
      InsertEligibleTab(web::WebStateID::FromSerializedValue(221));
  tab->WasShown();

  tab->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_EQ(nil, GetPageContextWrapper());
}

// Test that a successful page load in the visible tab starts an extraction when
// page-load extraction timing is enabled.
TEST_P(PersistTabContextBrowserAgentTest,
       PageLoadedExtractsVisibleTabWhenTimingEnabled) {
  RecreateAgentWithPageLoadExtraction();
  web::FakeWebState* tab =
      InsertEligibleTab(web::WebStateID::FromSerializedValue(222));
  tab->WasShown();

  // A failed load must not trigger an extraction.
  tab->OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_EQ(nil, GetPageContextWrapper());

  tab->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_NE(nil, GetPageContextWrapper());
}

// Test that a page load in a background tab neither starts an extraction nor
// cancels the in-flight extraction of a just-hidden tab, since all realized
// tabs are observed and they share a single extraction wrapper.
TEST_P(PersistTabContextBrowserAgentTest,
       PageLoadedInBackgroundTabDoesNotCancelInFlightExtraction) {
  RecreateAgentWithPageLoadExtraction();
  web::FakeWebState* background_tab =
      InsertEligibleTab(web::WebStateID::FromSerializedValue(223));

  // A background load with no extraction in flight must not start one.
  background_tab->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  ASSERT_EQ(nil, GetPageContextWrapper());

  // Start an extraction for a tab that was just hidden.
  web::FakeWebState* hidden_tab =
      InsertEligibleTab(web::WebStateID::FromSerializedValue(224));
  hidden_tab->WasHidden();
  PageContextWrapper* in_flight_wrapper = GetPageContextWrapper();
  ASSERT_NE(nil, in_flight_wrapper);

  background_tab->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_EQ(in_flight_wrapper, GetPageContextWrapper());
}

// Test that a committed same-document navigation (e.g. SPA pushState) also
// deletes the persisted context for that tab.
TEST_P(PersistTabContextBrowserAgentTest,
       DidFinishNavigationSameDocumentDeletesContext) {
  web::WebStateID tab_id = web::WebStateID::FromSerializedValue(203);
  auto tab = std::make_unique<web::FakeWebState>(tab_id);
  tab->SetCurrentURL(GURL("http://example.com/203"));
  tab->SetIsRealized(true);
  web::FakeWebState* tab_ptr = tab.get();
  web_state_list_->InsertWebState(std::move(tab));

  CreateDummyContextFile(tab_id);

  web::FakeNavigationContext nav_context;
  nav_context.SetHasCommitted(true);
  nav_context.SetIsSameDocument(true);
  tab_ptr->OnNavigationFinished(&nav_context);

  base::test::TestFuture<
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>
      future;
  agent_->GetSingleContextAsync(base::NumberToString(tab_id.identifier()),
                                future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

// Test that OnPageContextExtracted discards extracted context and deletes any
// existing cached entry when the WebState's committed URL changed during
// extraction.
TEST_P(PersistTabContextBrowserAgentTest,
       OnPageContextExtractedDiscardsContextOnURLMismatch) {
  web::WebStateID tab_id = web::WebStateID::FromSerializedValue(204);
  auto tab = std::make_unique<web::FakeWebState>(tab_id);
  tab->SetCurrentURL(GURL("http://victim.example/account"));
  tab->SetIsRealized(true);
  web::FakeWebState* tab_ptr = tab.get();
  web_state_list_->InsertWebState(std::move(tab));

  CreateDummyContextFile(tab_id);

  auto extracted_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  extracted_context->set_url("http://attacker.example/page");
  extracted_context->set_title("Attacker Title");

  CallOnPageContextExtracted(tab_ptr->GetWeakPtr(),
                             base::ok(std::move(extracted_context)),
                             GURL("http://attacker.example/page"));

  // Verify the newly extracted context was not written to storage even when the
  // tab's URL matches the extracted context's URL.
  tab_ptr->SetCurrentURL(GURL("http://attacker.example/page"));
  base::test::TestFuture<
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>
      extracted_future;
  agent_->GetSingleContextAsync(base::NumberToString(tab_id.identifier()),
                                extracted_future.GetCallback());
  EXPECT_FALSE(extracted_future.Get().has_value());

  // Verify the pre-existing cached context was also deleted from storage.
  tab_ptr->SetCurrentURL(GURL("http://example.com/204"));
  base::test::TestFuture<
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>
      previous_future;
  agent_->GetSingleContextAsync(base::NumberToString(tab_id.identifier()),
                                previous_future.GetCallback());
  EXPECT_FALSE(previous_future.Get().has_value());
}

// Test that GetSingleContextAsync validates the cached PageContext URL against
// the live WebState's committed URL, returning nullopt and evicting the entry
// on mismatch.
TEST_P(PersistTabContextBrowserAgentTest,
       GetSingleContextAsyncRejectsAndEvictsMismatchedURL) {
  web::WebStateID tab_id = web::WebStateID::FromSerializedValue(205);
  auto tab = std::make_unique<web::FakeWebState>(tab_id);
  tab->SetCurrentURL(GURL("http://example.com/205"));
  tab->SetIsRealized(true);
  web::FakeWebState* tab_ptr = tab.get();
  web_state_list_->InsertWebState(std::move(tab));

  CreateDummyContextFile(tab_id);

  // Update the live WebState's committed URL without firing navigation events.
  tab_ptr->SetCurrentURL(GURL("http://victim.example/account"));

  base::test::TestFuture<
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>
      mismatch_future;
  agent_->GetSingleContextAsync(base::NumberToString(tab_id.identifier()),
                                mismatch_future.GetCallback());
  EXPECT_FALSE(mismatch_future.Get().has_value());

  // Restore the original URL and verify the stale entry was evicted from
  // storage.
  tab_ptr->SetCurrentURL(GURL("http://example.com/205"));
  base::test::TestFuture<
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>
      evicted_future;
  agent_->GetSingleContextAsync(base::NumberToString(tab_id.identifier()),
                                evicted_future.GetCallback());
  EXPECT_FALSE(evicted_future.Get().has_value());
}

// Test that GetMultipleContextsAsync validates all fetched contexts in a batch,
// preserving valid entries while rejecting and evicting mismatched or closed
// tabs.
TEST_P(PersistTabContextBrowserAgentTest,
       GetMultipleContextsAsyncRejectsAndEvictsMismatchedURL) {
  web::WebStateID valid_id = web::WebStateID::FromSerializedValue(206);
  auto valid_tab = std::make_unique<web::FakeWebState>(valid_id);
  valid_tab->SetCurrentURL(GURL("http://example.com/206"));
  valid_tab->SetIsRealized(true);
  web_state_list_->InsertWebState(std::move(valid_tab));
  CreateDummyContextFile(valid_id);

  web::WebStateID mismatch_id = web::WebStateID::FromSerializedValue(207);
  auto mismatch_tab = std::make_unique<web::FakeWebState>(mismatch_id);
  mismatch_tab->SetCurrentURL(GURL("http://example.com/207"));
  mismatch_tab->SetIsRealized(true);
  web::FakeWebState* mismatch_tab_ptr = mismatch_tab.get();
  web_state_list_->InsertWebState(std::move(mismatch_tab));
  CreateDummyContextFile(mismatch_id);

  // Cached context for a closed tab not present in any WebStateList.
  web::WebStateID orphaned_id = web::WebStateID::FromSerializedValue(208);
  CreateDummyContextFile(orphaned_id);

  // Change mismatch_tab's committed URL without firing navigation events.
  mismatch_tab_ptr->SetCurrentURL(GURL("http://victim.example/account"));

  base::test::TestFuture<PersistTabContextBrowserAgent::PageContextMap>
      batch_future;
  agent_->GetMultipleContextsAsync(
      {base::NumberToString(valid_id.identifier()),
       base::NumberToString(mismatch_id.identifier()),
       base::NumberToString(orphaned_id.identifier())},
      batch_future.GetCallback());
  const PersistTabContextBrowserAgent::PageContextMap& results =
      batch_future.Get();
  ASSERT_TRUE(results.at("206").has_value());
  EXPECT_EQ((*results.at("206"))->url(), "http://example.com/206");
  EXPECT_FALSE(results.at("207").has_value());
  EXPECT_FALSE(results.at("208").has_value());

  // Restore mismatch_tab's original URL and verify its stale entry was evicted.
  mismatch_tab_ptr->SetCurrentURL(GURL("http://example.com/207"));
  base::test::TestFuture<
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>
      evicted_future;
  agent_->GetSingleContextAsync(base::NumberToString(mismatch_id.identifier()),
                                evicted_future.GetCallback());
  EXPECT_FALSE(evicted_future.Get().has_value());
}

// Test that read validation checks WebStates across all browsers in the
// profile's BrowserList.
TEST_P(PersistTabContextBrowserAgentTest,
       GetSingleContextAsyncValidatesAcrossMultipleBrowsers) {
  auto secondary_browser = std::make_unique<TestBrowser>(profile_.get());
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  browser_list->AddBrowser(browser_.get());
  browser_list->AddBrowser(secondary_browser.get());

  web::WebStateID other_browser_tab_id =
      web::WebStateID::FromSerializedValue(209);
  auto other_tab = std::make_unique<web::FakeWebState>(other_browser_tab_id);
  other_tab->SetCurrentURL(GURL("http://example.com/209"));
  other_tab->SetIsRealized(true);
  secondary_browser->GetWebStateList()->InsertWebState(std::move(other_tab));

  CreateDummyContextFile(other_browser_tab_id);

  base::test::TestFuture<
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>
      future;
  agent_->GetSingleContextAsync(
      base::NumberToString(other_browser_tab_id.identifier()),
      future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ((*future.Get())->url(), "http://example.com/209");

  browser_list->RemoveBrowser(secondary_browser.get());
  browser_list->RemoveBrowser(browser_.get());
}

// Verifies cleanup removes cache entries whose WebState no longer exists,
// while preserving contexts for both realized and unrealized WebStates.
TEST_P(PersistTabContextBrowserAgentTest, TestRunCacheCleanup) {
  if (GetParam() != PersistTabStorageType::kSQLite) {
    return;
  }

  web::WebStateID realized_id = web::WebStateID::FromSerializedValue(101);
  auto realized = std::make_unique<web::FakeWebState>(realized_id);
  realized->SetCurrentURL(GURL("https://example.com/realized"));
  realized->SetIsRealized(true);
  realized->SetContentsMimeType("text/html");
  web_state_list_->InsertWebState(std::move(realized));
  CreateDummyContextFile(realized_id);

  web::WebStateID unrealized_id = web::WebStateID::FromSerializedValue(102);
  auto unrealized = std::make_unique<web::FakeWebState>(unrealized_id);
  unrealized->SetCurrentURL(GURL("https://example.com/unrealized"));
  unrealized->SetIsRealized(false);
  web_state_list_->InsertWebState(std::move(unrealized));
  CreateDummyContextFile(unrealized_id);

  // Cache entry with no matching WebState in the list.
  web::WebStateID stale_id = web::WebStateID::FromSerializedValue(103);
  CreateDummyContextFile(stale_id);

  PageContentCacheService* service =
      PageContentCacheServiceFactory::GetForProfile(profile_.get());
  BrowserListFactory::GetForProfile(profile_.get())->AddBrowser(browser_.get());

  {
    base::test::TestFuture<std::vector<int64_t>> future;
    service->GetAllTabIds(future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_THAT(future.Get(),
                testing::UnorderedElementsAre(realized_id.identifier(),
                                              unrealized_id.identifier(),
                                              stale_id.identifier()));
  }

  RunCacheCleanup();
  task_environment_.FastForwardBy(kPurgeTaskDelay + kCleanupTaskDelay +
                                  base::Seconds(1));
  WaitForServiceSequence(service);

  base::test::TestFuture<std::vector<int64_t>> future;
  service->GetAllTabIds(future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(),
              testing::UnorderedElementsAre(realized_id.identifier(),
                                            unrealized_id.identifier()));
}

class PersistTabContextBrowserAgentDisabledTest : public PlatformTest {
 protected:
  PersistTabContextBrowserAgentDisabledTest()
      : task_environment_(web::WebTaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures({kCleanupPersistedTabContexts},
                                   {kPersistTabContext});
  }

  // Helper to get the storage directory path consistent with the agent's logic.
  base::FilePath GetExpectedStorageDir(ProfileIOS* profile) {
    base::FilePath cache_directory_path;
    // Use the same function as the agent to get the cache directory.
    ios::GetUserCacheDirectory(profile->GetStatePath(), &cache_directory_path);
    return cache_directory_path.Append(kPersistedTabContextsDir);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Initialize profile with the base temp directory path.
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build(temp_dir_.GetPath());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_list_ = browser_->GetWebStateList();
  }

  web::WebTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<PersistTabContextBrowserAgent> agent_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PersistTabContextBrowserAgentDisabledTest, TestDirectoriesDeleted) {
  base::FilePath storage_dir_fs = GetExpectedStorageDir(profile_.get());
  base::FilePath storage_dir_sql =
      PageContentCacheServiceFactory::GetStoragePathForProfile(profile_.get());

  // Create dummy directories and files for both.
  ASSERT_TRUE(base::CreateDirectory(storage_dir_fs));
  base::FilePath dummy_file_fs =
      storage_dir_fs.Append(FILE_PATH_LITERAL("dummy_fs.proto"));
  ASSERT_TRUE(base::WriteFile(dummy_file_fs, "fs"));
  ASSERT_TRUE(base::DirectoryExists(storage_dir_fs));

  ASSERT_TRUE(base::CreateDirectory(storage_dir_sql));
  base::FilePath dummy_file_sql =
      storage_dir_sql.Append(FILE_PATH_LITERAL("dummy_sql.proto"));
  ASSERT_TRUE(base::WriteFile(dummy_file_sql, "sql"));
  ASSERT_TRUE(base::DirectoryExists(storage_dir_sql));

  // Creating the agent should trigger directory deletion for both storage
  // systems when the feature is disabled.
  PersistTabContextBrowserAgent::CreateForBrowser(browser_.get());
  agent_ = PersistTabContextBrowserAgent::FromBrowser(browser_.get());

  // Wait for the deletion to complete.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !base::DirectoryExists(storage_dir_fs) &&
           !base::DirectoryExists(storage_dir_sql);
  }));

  // Verify both directories are deleted.
  EXPECT_FALSE(base::DirectoryExists(storage_dir_fs))
      << "Filesystem storage was not deleted when feature is disabled.";
  EXPECT_FALSE(base::DirectoryExists(storage_dir_sql))
      << "SQLite storage was not deleted when feature is disabled.";
}
