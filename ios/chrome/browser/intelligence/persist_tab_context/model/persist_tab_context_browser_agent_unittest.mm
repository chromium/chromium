// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"

#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/page_content_annotations/core/page_content_annotations_features.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/metrics/persist_tab_context_metrics.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_bridge_service.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_bridge_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/paths/paths_internal.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
// TODO(crbug.com/447646545): Extract these constants to a shared header file.
constexpr std::string kPageContextPrefix = "page_context_";
constexpr std::string kProtoSuffix = ".proto";
constexpr std::string kPersistedTabContextsDir = "persisted_tab_contexts";
constexpr base::TimeDelta kPurgeTaskDelay = base::Seconds(3);
}  // namespace

class PersistTabContextBrowserAgentTest
    : public PlatformTest,
      public testing::WithParamInterface<PersistTabStorageType> {
 protected:
  PersistTabContextBrowserAgentTest()
      : task_environment_(web::WebTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    PlatformTest::SetUp();

    if (GetParam() == PersistTabStorageType::kSQLite) {
      feature_list_.InitWithFeaturesAndParameters(
          {{kPersistTabContext, {{kPersistTabContextStorageParam, "1"}}},
           {page_content_annotations::features::kPageContentCache, {}}},
          {});
    } else {
      feature_list_.InitWithFeaturesAndParameters(
          {{kPersistTabContext, {{kPersistTabContextStorageParam, "0"}}},
           {kCleanupPersistedTabContexts, {}}},
          {});
    }

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build(temp_dir_.GetPath());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_list_ = browser_->GetWebStateList();
    PersistTabContextBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = PersistTabContextBrowserAgent::FromBrowser(browser_.get());
    if (GetParam() == PersistTabStorageType::kSQLite) {
      PageContentCacheBridgeService* service =
          PageContentCacheBridgeServiceFactory::GetForProfile(profile_.get());
      ASSERT_TRUE(base::test::RunUntil(
          [&]() { return service->IsCacheInitialized(); }));
    }
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
      PageContentCacheBridgeService* service =
          PageContentCacheBridgeServiceFactory::GetForProfile(profile_.get());

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
  void WaitForServiceSequence(PageContentCacheBridgeService* bridge) {
    base::RunLoop run_loop;
    bridge->GetAllTabIds(base::BindOnce(
        [](base::RunLoop* loop, std::vector<int64_t> ignored) { loop->Quit(); },
        &run_loop));
    run_loop.Run();
  }

  std::unique_ptr<web::WebState> CreateAndLoadWebState(const std::string& html,
                                                       const GURL& url) {
    web::WebState::CreateParams params(profile_.get());
    std::unique_ptr<web::WebState> web_state = web::WebState::Create(params);
    web::test::LoadHtml(base::SysUTF8ToNSString(html), url, web_state.get());
    return web_state;
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

TEST_P(PersistTabContextBrowserAgentTest, TestGetMultipleContextsAsync) {
  web::WebStateID id1 = web::WebStateID::FromSerializedValue(1);
  web::WebStateID id2 = web::WebStateID::FromSerializedValue(2);
  web::WebStateID id3 = web::WebStateID::FromSerializedValue(3);
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

  base::TimeDelta test_ttl = base::Days(21);

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
    PageContentCacheBridgeService* bridge =
        PageContentCacheBridgeServiceFactory::GetForProfile(profile_.get());

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

TEST_P(PersistTabContextBrowserAgentTest, TestMigrationDeletesLegacyFiles) {
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

TEST_F(PersistTabContextBrowserAgentDisabledTest, TestDirectoryDeleted) {
  base::FilePath storage_dir = GetExpectedStorageDir(profile_.get());

  // Create a dummy directory and file.
  ASSERT_TRUE(base::CreateDirectory(storage_dir));
  base::FilePath dummy_file =
      storage_dir.Append(FILE_PATH_LITERAL("dummy.proto"));
  ASSERT_TRUE(base::WriteFile(dummy_file, "dummy content"));

  // Verify the directory exists before the agent is created.
  ASSERT_TRUE(base::DirectoryExists(storage_dir));

  // Creating the agent should trigger directory deletion when the feature is
  // disabled.
  PersistTabContextBrowserAgent::CreateForBrowser(browser_.get());
  agent_ = PersistTabContextBrowserAgent::FromBrowser(browser_.get());

  // Wait for the deletion to complete.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !base::DirectoryExists(storage_dir); }));

  // Verify the directory is deleted.
  EXPECT_FALSE(base::DirectoryExists(storage_dir))
      << "Persisted tab contexts directory was not deleted when feature is "
         "disabled.";
}
