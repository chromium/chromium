// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader.h"

#include "base/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
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
const char kTestExtensionId3[] = "test_app3";

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
    fetch->AddAssociatedTask(ExtensionDownloaderTask(
        kTestExtensionId, kUpdateUrl, mojom::ManifestLocation::kInternal,
        false /* is_corrupt_reinstall */, 0 /* request_id */,
        DownloadFetchPriority::kBackground));
    return fetch;
  }

  void AddFetchDataToDownloader(ExtensionDownloaderTestHelper* helper,
                                std::unique_ptr<ManifestFetchData> fetch) {
    helper->StartUpdateCheck(std::move(fetch));
  }

  const URLStats& GetDownloaderURLStats(ExtensionDownloaderTestHelper* helper) {
    return helper->downloader().url_stats_;
  }

  const std::vector<ExtensionDownloaderTask>& GetDownloaderPendingTasks(
      ExtensionDownloaderTestHelper* helper) {
    return helper->downloader().pending_tasks_;
  }

  std::string CreateUpdateManifest(const std::string& extension_id,
                                   const std::string& extension_version) {
    return "<?xml version='1.0' encoding='UTF-8'?>"
           "<gupdate xmlns='http://www.google.com/update2/response'"
           "                protocol='2.0'>"
           " <app appid='" +
           extension_id +
           "'>"
           "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
           "               version='" +
           extension_version +
           "' prodversionmin='1.1' />"
           " </app>"
           "</gupdate>";
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
  const std::string manifest = CreateUpdateManifest(kTestExtensionId, "1.1");
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
  const std::string manifest = CreateUpdateManifest(kTestExtensionId, "1.1");
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
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<gupdate xmlns='http://www.google.com/update2/response'"
      "                protocol='2.0'>"
      " <app appid='" +
      std::string(kTestExtensionId) +
      "'>"
      "  <updatecheck info='bandwidth limit' status='noupdate' />"
      " </app>"
      "</gupdate>";
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
  const std::string manifest = CreateUpdateManifest(kTestExtensionId, "1.1");
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
  const std::string manifest = CreateUpdateManifest(kTestExtensionId, "1.1");
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
  const std::string manifest = CreateUpdateManifest(kTestExtensionId, "1.1");
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

// Tests that stats for UMA is collected correctly.
TEST_F(ExtensionDownloaderTest, TestURLStats) {
  ExtensionDownloaderTestHelper helper;
  GURL kUpdateUrl("http://localhost/manifest1");
  const URLStats& stats = GetDownloaderURLStats(&helper);

  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl(),
      mojom::ManifestLocation::kInternal, false /* is_corrupt_reinstall */,
      0 /* request_id */, DownloadFetchPriority::kBackground));
  EXPECT_EQ(1, stats.google_url_count);

  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId2, GURL() /* update_url */,
      mojom::ManifestLocation::kInternal, false /* is_corrupt_reinstall */,
      0 /* request_id */, DownloadFetchPriority::kBackground));
  EXPECT_EQ(1, stats.no_url_count);

  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId3, kUpdateUrl, mojom::ManifestLocation::kInternal,
      false /* is_corrupt_reinstall */, 0 /* request_id */,
      DownloadFetchPriority::kBackground));
  EXPECT_EQ(1, stats.other_url_count);
}

// Tests edge-cases related to the update URL.
TEST_F(ExtensionDownloaderTest, TestUpdateURLHandle) {
  ExtensionDownloaderTestHelper helper;
  const std::vector<ExtensionDownloaderTask>& tasks =
      GetDownloaderPendingTasks(&helper);

  // Clear pending queue to check it.
  helper.downloader().StartAllPending(nullptr);
  // Invalid update URL, shouldn't be added at all.
  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId, GURL("http://?invalid=url"),
      mojom::ManifestLocation::kInternal, false /* is_corrupt_reinstall */,
      0 /* request_id */, DownloadFetchPriority::kBackground));
  EXPECT_EQ(0u, tasks.size());

  // Clear pending queue to check it.
  helper.downloader().StartAllPending(nullptr);
  // HTTP Webstore URL, should be replaced with HTTPS.
  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId, GURL("http://clients2.google.com/service/update2/crx"),
      mojom::ManifestLocation::kInternal, false /* is_corrupt_reinstall */,
      0 /* request_id */, DownloadFetchPriority::kBackground));
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(GURL("https://clients2.google.com/service/update2/crx"),
            tasks.rbegin()->update_url);

  // Clear pending queue to check it.
  helper.downloader().StartAllPending(nullptr);
  // Just a custom URL, should be kept as is.
  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId, GURL("https://example.com"),
      mojom::ManifestLocation::kInternal, false /* is_corrupt_reinstall */,
      0 /* request_id */, DownloadFetchPriority::kBackground));
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(GURL("https://example.com"), tasks.rbegin()->update_url);

  // Clear pending queue to check it.
  helper.downloader().StartAllPending(nullptr);
  // Empty URL, should be replaced with Webstore one.
  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId, GURL(""), mojom::ManifestLocation::kInternal,
      false /* is_corrupt_reinstall */, 0 /* request_id */,
      DownloadFetchPriority::kBackground));
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(GURL("https://clients2.google.com/service/update2/crx"),
            tasks.rbegin()->update_url);
}

// Tests that multiple updates are combined in single requests.
TEST_F(ExtensionDownloaderTest, TestMultipleUpdates) {
  ExtensionDownloaderTestHelper helper;

  // Add two extensions with different update URLs (to have at least two items
  // in manifest requests queue).
  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId, GURL("http://example1.com/"),
      mojom::ManifestLocation::kInternal, false /* is_corrupt_reinstall */,
      0 /* request_id */, DownloadFetchPriority::kBackground));
  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId2, GURL("http://example2.com/"),
      mojom::ManifestLocation::kInternal, false /* is_corrupt_reinstall */,
      0 /* request_id */, DownloadFetchPriority::kBackground));

  helper.downloader().StartAllPending(nullptr);

  // Add the same two extensions again.
  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId, GURL("http://example1.com/"),
      mojom::ManifestLocation::kInternal, false /* is_corrupt_reinstall */,
      0 /* request_id */, DownloadFetchPriority::kBackground));
  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId2, GURL("http://example2.com/"),
      mojom::ManifestLocation::kInternal, false /* is_corrupt_reinstall */,
      0 /* request_id */, DownloadFetchPriority::kBackground));

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

  helper.downloader().SetBackoffPolicyForTesting(&kZeroBackoffPolicy);

  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl(),
      mojom::ManifestLocation::kExternalPolicyDownload,
      false /* is_corrupt_reinstall */, 0 /* request_id */,
      DownloadFetchPriority::kBackground));
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

  helper.downloader().SetBackoffPolicyForTesting(&kZeroBackoffPolicy);

  helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
      kTestExtensionId, extension_urls::GetWebstoreUpdateUrl(),
      mojom::ManifestLocation::kExternalPolicyDownload,
      false /* is_corrupt_reinstall */, 0 /* request_id */,
      DownloadFetchPriority::kBackground));

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

}  // namespace extensions
