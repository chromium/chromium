// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader.h"

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "services/network/test/test_utils.h"

using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Return;
using testing::Sequence;
using testing::SetArgPointee;

namespace extensions {

namespace {

const char kTestExtensionId[] = "test_app";
const char kTestExtensionId2[] = "test_app2";

}  // namespace

class ExtensionDownloaderTest : public ExtensionsTest {
 protected:
  using URLStats = ExtensionDownloader::URLStats;

  ExtensionDownloaderTest() = default;

  std::unique_ptr<ManifestFetchData> CreateManifestFetchData(
      const GURL& update_url,
      DownloadFetchPriority fetch_priority =
          DownloadFetchPriority::kBackground) {
    return std::make_unique<ManifestFetchData>(
        update_url, 0, "", "", ManifestFetchData::PING, fetch_priority);
  }

  std::unique_ptr<ManifestFetchData> CreateTestAppFetchData() {
    GURL kUpdateUrl("http://localhost/manifest1");
    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(kUpdateUrl));
    DownloadPingData zero_days(0, 0, true, 0);
    fetch->AddExtension(kTestExtensionId, "1.0", &zero_days, "", "",
                        mojom::ManifestLocation::kInternal,
                        DownloadFetchPriority::kBackground);
    fetch->AddAssociatedTask(
        CreateDownloaderTask(kTestExtensionId, kUpdateUrl));
    return fetch;
  }

  void AddFetchDataToDownloader(ExtensionDownloaderTestHelper* helper,
                                std::unique_ptr<ManifestFetchData> fetch) {
    helper->StartUpdateCheck(std::move(fetch));
  }

  const std::vector<ExtensionDownloaderTask>& GetDownloaderPendingTasks(
      ExtensionDownloaderTestHelper* helper) {
    return helper->downloader().pending_tasks_;
  }

  // Creates an update manifest for several extensions. Provided values should
  // be tuples of (extension id, version, URL to the CRX file).
  std::string CreateUpdateManifest(
      const std::vector<std::tuple<ExtensionId, std::string, std::string>>&
          extensions) {
    std::vector<UpdateManifestItem> extensions_raw;
    for (const auto& [id, version, codebase] : extensions) {
      extensions_raw.emplace_back(UpdateManifestItem(id)
                                      .codebase(codebase)
                                      .version(version)
                                      .prodversionmin("1.1"));
    }
    return extensions::CreateUpdateManifest(extensions_raw);
  }

  // Create an update manifest with one test extension.
  std::string CreateDefaultUpdateManifest() {
    return CreateUpdateManifest({std::make_tuple(
        kTestExtensionId, "1.1", "http://example.com/extension_1.2.3.4.crx")});
  }

  void SetHelperHandlers(ExtensionDownloaderTestHelper* helper,
                         const GURL& fetch_url,
                         const std::string& manifest) {
    helper->test_url_loader_factory().AddResponse(fetch_url.spec(), manifest,
                                                  net::HTTP_OK);
    GURL kCrxUrl("http://example.com/extension_1.2.3.4.crx");
    helper->test_url_loader_factory().AddResponse(kCrxUrl.spec(), "no data",
                                                  net::HTTP_OK);
  }
};

// Several tests checking that OnExtensionDownloadStageChanged callback is
// called correctly.
TEST_F(ExtensionDownloaderTest, TestStageChanges) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  const std::string manifest = CreateDefaultUpdateManifest();
  SetHelperHandlers(&helper, fetch->full_url(), manifest);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillOnce(Return(true));
  EXPECT_CALL(delegate,
              OnExtensionDownloadCacheStatusRetrieved(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::CacheStatus::CACHE_DISABLED));
  Sequence sequence;
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::Stage::QUEUED_FOR_CRX))
      .Times(AnyNumber());
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::PARSING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::Stage::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(ExtensionDownloaderTest, TestStageChangesNoUpdates) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  const std::string manifest = CreateDefaultUpdateManifest();
  SetHelperHandlers(&helper, fetch->full_url(), manifest);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillOnce(Return(false));
  EXPECT_CALL(delegate, GetExtensionExistingVersion(kTestExtensionId, _))
      .WillOnce(DoAll(SetArgPointee<1>("1.1"), Return(true)));
  Sequence sequence;
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::PARSING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX))
      .Times(0);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::Stage::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(ExtensionDownloaderTest, TestStageChangesBadManifest) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  GURL fetch_url = fetch->full_url();

  const std::string kManifest = "invalid xml";
  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), kManifest,
                                               net::HTTP_OK);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  Sequence sequence;
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::PARSING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED))
      .Times(0);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::Stage::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(ExtensionDownloaderTest, TestStageChangesBadQuery) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  GURL fetch_url = fetch->full_url();

  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), "",
                                               net::HTTP_BAD_REQUEST);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  Sequence sequence;
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::Stage::PARSING_MANIFEST))
      .Times(0);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::Stage::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

// Test that failure callback was actually called in case of empty answer from
// the update server. Regression for problem described/fixed in
// crbug.com/938265.
TEST_F(ExtensionDownloaderTest, TestNoUpdatesManifestReports) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  GURL fetch_url = fetch->full_url();

  const std::string kManifest =
      extensions::CreateUpdateManifest({UpdateManifestItem(kTestExtensionId)
                                            .status("noupdate")
                                            .info("bandwidth limit")});
  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), kManifest,
                                               net::HTTP_OK);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillOnce(Return(true));
  // TODO(burunduk) Also check error (second argument). By now we return
  // CRX_FETCH_FAILED, but probably we may want to make another one.
  EXPECT_CALL(delegate,
              OnExtensionDownloadFailed(kTestExtensionId, _, _, _, _));

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

// Test that cache status callback is called correctly if there is no such
// extension in cache.
TEST_F(ExtensionDownloaderTest, TestCacheStatusMiss) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  const std::string manifest = CreateDefaultUpdateManifest();
  SetHelperHandlers(&helper, fetch->full_url(), manifest);

  // Create cache and provide it to downloader.
  auto test_extension_cache = std::make_unique<ExtensionCacheFake>();
  helper.downloader().StartAllPending(test_extension_cache.get());

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillOnce(Return(true));
  EXPECT_CALL(delegate,
              OnExtensionDownloadCacheStatusRetrieved(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS));

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

// Test that cache status callback is called correctly if there is an extension
// archive in cache, but only an old version.
TEST_F(ExtensionDownloaderTest, TestCacheStatusOutdated) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  const std::string manifest = CreateDefaultUpdateManifest();
  SetHelperHandlers(&helper, fetch->full_url(), manifest);

  // Create cache and provide it to downloader.
  auto test_extension_cache = std::make_unique<ExtensionCacheFake>();
  test_extension_cache->AllowCaching(kTestExtensionId);
  test_extension_cache->PutExtension(
      kTestExtensionId, "" /* expected hash, ignored by ExtensionCacheFake */,
      base::FilePath(), "1.0", base::DoNothing());
  helper.downloader().StartAllPending(test_extension_cache.get());

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillOnce(Return(true));
  EXPECT_CALL(delegate,
              OnExtensionDownloadCacheStatusRetrieved(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::CacheStatus::CACHE_OUTDATED));

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

// Test that cache status callback is called correctly if there is an extension
// archive in cache.
TEST_F(ExtensionDownloaderTest, TestCacheStatusHit) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  const std::string manifest = CreateDefaultUpdateManifest();
  SetHelperHandlers(&helper, fetch->full_url(), manifest);

  // Create cache and provide it to downloader.
  auto test_extension_cache = std::make_unique<ExtensionCacheFake>();
  test_extension_cache->AllowCaching(kTestExtensionId);
  test_extension_cache->PutExtension(
      kTestExtensionId, "" /* expected hash, ignored by ExtensionCacheFake */,
      base::FilePath(FILE_PATH_LITERAL(
          "file_not_found.crx")) /* We don't need to actually read file. */,
      "1.1", base::DoNothing());
  helper.downloader().StartAllPending(test_extension_cache.get());

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillOnce(Return(true));
  EXPECT_CALL(delegate,
              OnExtensionDownloadCacheStatusRetrieved(
                  kTestExtensionId,
                  ExtensionDownloaderDelegate::CacheStatus::CACHE_HIT));

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

// Tests edge-cases related to the update URL.
TEST_F(ExtensionDownloaderTest, TestUpdateURLHandle) {
  ExtensionDownloaderTestHelper helper;
  const std::vector<ExtensionDownloaderTask>& tasks =
      GetDownloaderPendingTasks(&helper);

  // Clear pending queue to check it.
  helper.downloader().StartAllPending(nullptr);
  // Invalid update URL, shouldn't be added at all.
  helper.downloader().AddPendingExtension(
      CreateDownloaderTask(kTestExtensionId, GURL("http://?invalid=url")));
  EXPECT_EQ(0u, tasks.size());

  // data: URL, shouldn't be added at all.
  helper.downloader().AddPendingExtension(
      CreateDownloaderTask(kTestExtensionId, GURL("data:,")));
  EXPECT_EQ(0u, tasks.size());

  // Clear pending queue to check it.
  helper.downloader().StartAllPending(nullptr);
  // HTTP Webstore URL, should be replaced with HTTPS.
  helper.downloader().AddPendingExtension(CreateDownloaderTask(
      kTestExtensionId,
      GURL("http://clients2.google.com/service/update2/crx")));
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(GURL("https://clients2.google.com/service/update2/crx"),
            tasks.rbegin()->update_url);

  // Clear pending queue to check it.
  helper.downloader().StartAllPending(nullptr);
  // Just a custom URL, should be kept as is.
  helper.downloader().AddPendingExtension(
      CreateDownloaderTask(kTestExtensionId, GURL("https://example.com")));
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(GURL("https://example.com"), tasks.rbegin()->update_url);

  // Clear pending queue to check it.
  helper.downloader().StartAllPending(nullptr);
  // Empty URL, should be replaced with Webstore one.
  helper.downloader().AddPendingExtension(
      CreateDownloaderTask(kTestExtensionId, GURL("")));
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(GURL("https://clients2.google.com/service/update2/crx"),
            tasks.rbegin()->update_url);
}

// Tests that multiple updates are combined in single requests.
TEST_F(ExtensionDownloaderTest, TestMultipleUpdates) {
  ExtensionDownloaderTestHelper helper;

  // Add two extensions with different update URLs (to have at least two items
  // in manifest requests queue).
  helper.downloader().AddPendingExtension(
      CreateDownloaderTask(kTestExtensionId, GURL("http://example1.com/")));
  helper.downloader().AddPendingExtension(
      CreateDownloaderTask(kTestExtensionId2, GURL("http://example2.com/")));

  helper.downloader().StartAllPending(nullptr);

  // Add the same two extensions again.
  helper.downloader().AddPendingExtension(
      CreateDownloaderTask(kTestExtensionId, GURL("http://example1.com/")));
  helper.downloader().AddPendingExtension(
      CreateDownloaderTask(kTestExtensionId2, GURL("http://example2.com/")));

  helper.downloader().StartAllPending(nullptr);

  int requests_count = 0;
  network::TestURLLoaderFactory::PendingRequest* request = nullptr;
  while (helper.test_url_loader_factory().NumPending() > 0) {
    request = helper.test_url_loader_factory().GetPendingRequest(0);
    ASSERT_TRUE(request);
    // Fail all requests with 404 to quickly count them.
    requests_count++;
    helper.test_url_loader_factory().AddResponse(
        request->request.url.spec(), "not found", net::HTTP_NOT_FOUND);
  }
  EXPECT_EQ(2, requests_count);
}

// Tests that extension download is retried if no network found and
// extension not found in cache.
TEST_F(ExtensionDownloaderTest, TestNoNetworkRetryAfterCacheMiss) {
  ExtensionDownloaderTestHelper helper;

  helper.downloader().SetBackoffPolicy(kZeroBackoffPolicy);

  ExtensionDownloaderTask task = CreateDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl());
  task.install_location = mojom::ManifestLocation::kExternalPolicyDownload,
  helper.downloader().AddPendingExtension(std::move(task));
  auto test_extension_cache = std::make_unique<ExtensionCacheFake>();
  helper.downloader().StartAllPending(test_extension_cache.get());

  ASSERT_EQ(1, helper.test_url_loader_factory().NumPending());
  network::mojom::URLResponseHeadPtr response_head(
      network::CreateURLResponseHead(net::HTTP_OK));
  helper.test_url_loader_factory().SimulateResponseForPendingRequest(
      helper.test_url_loader_factory().GetPendingRequest(0)->request.url,
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
      std::move(response_head), "" /* content*/);

  // ExtensionDownloader is expected to retry the request, so number of pending
  // ones should be one again.
  EXPECT_EQ(1, helper.test_url_loader_factory().NumPending());
}

// Tests that manifest fetch failure is properly reported if extension not found
// in cache.
TEST_F(ExtensionDownloaderTest, TestManifestFetchFailureAfterCacheMiss) {
  ExtensionDownloaderTestHelper helper;

  helper.downloader().SetBackoffPolicy(kZeroBackoffPolicy);

  ExtensionDownloaderTask task = CreateDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl());
  task.install_location = mojom::ManifestLocation::kExternalPolicyDownload;
  helper.downloader().AddPendingExtension(std::move(task));

  EXPECT_CALL(
      helper.delegate(),
      OnExtensionDownloadFailed(
          kTestExtensionId,
          ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED, _, _, _));

  auto test_extension_cache = std::make_unique<ExtensionCacheFake>();
  helper.downloader().StartAllPending(test_extension_cache.get());

  ASSERT_EQ(1, helper.test_url_loader_factory().NumPending());
  helper.test_url_loader_factory().SimulateResponseForPendingRequest(
      helper.test_url_loader_factory().GetPendingRequest(0)->request.url.spec(),
      "" /* content */, net::HTTP_NOT_FOUND);

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&helper.delegate());
}

// Tests that multiple requests (with different `request_id`) are handled
// correctly.
TEST_F(ExtensionDownloaderTest, TestMultipleRequests) {
  ExtensionDownloaderTestHelper helper;

  ExtensionDownloaderTask task1 = CreateDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl());
  task1.request_id = 0;
  ExtensionDownloaderTask task2 = CreateDownloaderTask(
      kTestExtensionId2, extension_urls::GetWebstoreUpdateUrl());
  task2.request_id = 1;

  helper.test_url_loader_factory().SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::vector<std::tuple<ExtensionId, std::string, std::string>>
            extensions;
        if (base::Contains(request.url.spec(),
                           std::string("%3D") + kTestExtensionId + "%26")) {
          extensions.emplace_back(kTestExtensionId, "1.0",
                                  "https://example.com/extension1.crx");
        }
        if (base::Contains(request.url.spec(),
                           std::string("%3D") + kTestExtensionId2 + "%26")) {
          extensions.emplace_back(kTestExtensionId2, "1.0",
                                  "https://example.com/extension2.crx");
        }
        helper.test_url_loader_factory().AddResponse(
            request.url.spec(), CreateUpdateManifest(extensions), net::HTTP_OK);
      }));

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillOnce(Return(true));
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId2))
      .WillOnce(Return(true));
  std::map<ExtensionId, std::set<int>> results;
  EXPECT_CALL(delegate, OnExtensionDownloadFinished_(_, _, _, _, _, _))
      .WillRepeatedly(
          [&](const CRXFileInfo& file, bool file_ownership_passed,
              const GURL& download_url,
              const ExtensionDownloaderDelegate::PingResult& ping_result,
              const std::set<int>& request_ids,
              ExtensionDownloaderDelegate::InstallCallback& callback) {
            ASSERT_EQ(results.count(file.extension_id), 0u);
            results[file.extension_id] = request_ids;
          });

  helper.downloader().AddPendingExtension(std::move(task1));
  helper.downloader().AddPendingExtension(std::move(task2));
  helper.downloader().StartAllPending(nullptr);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(results.size(), 2u);
  EXPECT_EQ(results[kTestExtensionId], std::set<int>({0}));
  EXPECT_EQ(results[kTestExtensionId2], std::set<int>({1}));

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

// Tests that multiple requests (with different `request_id`) are handled
// correctly when they are used to fetch the same extension.
TEST_F(ExtensionDownloaderTest, TestMultipleRequestsSameExtension) {
  ExtensionDownloaderTestHelper helper;

  ExtensionDownloaderTask task1 = CreateDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl());
  task1.request_id = 0;
  ExtensionDownloaderTask task2 = CreateDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl());
  task2.request_id = 1;

  helper.test_url_loader_factory().SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url.spec() == "https://example.com/extension1.crx") {
          helper.test_url_loader_factory().AddResponse(request.url.spec(), "",
                                                       net::HTTP_OK);
          return;
        }
        ASSERT_TRUE(base::Contains(
            request.url.spec(), std::string("%3D") + kTestExtensionId + "%26"));
        std::vector<std::tuple<ExtensionId, std::string, std::string>>
            extensions;
        extensions.emplace_back(kTestExtensionId, "1.0",
                                "https://example.com/extension1.crx");
        helper.test_url_loader_factory().AddResponse(
            request.url.spec(), CreateUpdateManifest(extensions), net::HTTP_OK);
      }));

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillRepeatedly(Return(true));
  std::map<ExtensionId, std::set<int>> results;
  EXPECT_CALL(delegate, OnExtensionDownloadFinished_(_, _, _, _, _, _))
      .WillRepeatedly(
          [&](const CRXFileInfo& file, bool file_ownership_passed,
              const GURL& download_url,
              const ExtensionDownloaderDelegate::PingResult& ping_result,
              const std::set<int>& request_ids,
              ExtensionDownloaderDelegate::InstallCallback& callback) {
            DCHECK(results.count(file.extension_id) == 0);
            results[file.extension_id] = request_ids;
          });

  helper.downloader().AddPendingExtension(std::move(task1));
  helper.downloader().AddPendingExtension(std::move(task2));
  helper.downloader().StartAllPending(nullptr);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(results.size(), 1u);
  EXPECT_EQ(results[kTestExtensionId], std::set<int>({0, 1}));

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

// Tests that update manifest fetches with the same URLs will be actually
// merged.
TEST_F(ExtensionDownloaderTest, TestUpdateManifestURLMerged) {
  ExtensionDownloaderTestHelper helper;

  ExtensionDownloaderTask task1 = CreateDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl());
  task1.request_id = 1;
  ExtensionDownloaderTask task2 = CreateDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl());
  task2.request_id = 2;

  int number_of_fetches = 0;
  helper.test_url_loader_factory().SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url.spec() == "https://example.com/extension1.crx") {
          helper.test_url_loader_factory().AddResponse(request.url.spec(), "",
                                                       net::HTTP_OK);
          return;
        }
        number_of_fetches++;
        helper.test_url_loader_factory().AddResponse(
            request.url.spec(),
            CreateUpdateManifest(
                {std::make_tuple(kTestExtensionId, "1.0",
                                 "https://example.com/extension1.crx")}),
            net::HTTP_OK);
      }));

  helper.downloader().AddPendingExtension(std::move(task1));
  helper.downloader().StartAllPending(nullptr);
  // We expect the downloader to merge two requests when the first one will be
  // processed. For that we ensure that the first one becomes active before we
  // add the second one.
  EXPECT_NE(helper.downloader().GetActiveManifestFetchForTesting(), nullptr);
  helper.downloader().AddPendingExtension(std::move(task2));
  helper.downloader().StartAllPending(nullptr);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(number_of_fetches, 1);
}

// Tests that extension fetches with the same URLs will be actually merged.
TEST_F(ExtensionDownloaderTest, TestExtensionURLMerged) {
  ExtensionDownloaderTestHelper helper;

  ExtensionDownloaderTask task1 = CreateDownloaderTask(
      kTestExtensionId, GURL("https://example.com/update1"));
  task1.request_id = 1;
  ExtensionDownloaderTask task2 = CreateDownloaderTask(
      kTestExtensionId, GURL("https://example.com/update2"));
  task2.request_id = 2;

  int number_of_fetches = 0;
  helper.test_url_loader_factory().SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url.spec() == "https://example.com/extension1.crx") {
          number_of_fetches++;
          // Don't reply on this request immediately, make sure that manifest
          // fetches will happen first.
          return;
        }
        helper.test_url_loader_factory().AddResponse(
            request.url.spec(),
            CreateUpdateManifest(
                {std::make_tuple(kTestExtensionId, "1.0",
                                 "https://example.com/extension1.crx")}),
            net::HTTP_OK);
      }));

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillRepeatedly(Return(true));

  helper.downloader().AddPendingExtension(std::move(task1));
  helper.downloader().AddPendingExtension(std::move(task2));
  helper.downloader().StartAllPending(nullptr);

  content::RunAllTasksUntilIdle();
  helper.test_url_loader_factory().AddResponse(
      "https://example.com/extension1.crx", "", net::HTTP_OK);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(number_of_fetches, 1);
}

// Tests how the downloader uses the cache when there is no network.
TEST_F(ExtensionDownloaderTest, TestMultipleCacheAccess) {
  ExtensionDownloaderTestHelper helper;

  // Two tasks for the same extension ID will end up in two different but
  // completely identical manifest fetches in the downloader, so when we'll ask
  // the cache about the extension after network fetch failure, they should be
  // merged into one fetch and cache should be queried only once.
  ExtensionDownloaderTask task1 = CreateDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl());
  task1.install_location = mojom::ManifestLocation::kExternalPolicyDownload;
  task1.request_id = 1;
  ExtensionDownloaderTask task2 = CreateDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl());
  task2.install_location = mojom::ManifestLocation::kExternalPolicyDownload;
  task2.request_id = 2;

  helper.test_url_loader_factory().SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        network::mojom::URLResponseHeadPtr response_head(
            network::CreateURLResponseHead(net::HTTP_OK));
        helper.test_url_loader_factory().AddResponse(
            request.url, std::move(response_head), "" /* content*/,
            network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED));
      }));

  MockExtensionCache mock_cache;

  EXPECT_CALL(mock_cache, GetExtension(kTestExtensionId, _, _, _)).Times(1);

  helper.downloader().AddPendingExtension(std::move(task1));
  helper.downloader().AddPendingExtension(std::move(task2));
  helper.downloader().StartAllPending(&mock_cache);

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_cache);
}

}  // namespace extensions
