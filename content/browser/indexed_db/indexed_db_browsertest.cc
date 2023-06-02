// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/thread_test_helper.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom-test-utils.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/shell/browser/shell.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "url/gurl.h"
#include "url/origin.h"

using storage::DatabaseUtil;
using storage::QuotaManager;
using storage::mojom::FailClass;
using storage::mojom::FailMethod;

namespace content {

// This browser test is aimed towards exercising the IndexedDB bindings and
// the actual implementation that lives in the browser side.
class IndexedDBBrowserTest : public ContentBrowserTest {
 public:
  IndexedDBBrowserTest() = default;

  IndexedDBBrowserTest(const IndexedDBBrowserTest&) = delete;
  IndexedDBBrowserTest& operator=(const IndexedDBBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    // Some tests need more space than the default used for browser tests.
    static storage::QuotaSettings quota_settings =
        storage::GetHardCodedSettings(100 * 1024 * 1024);
    StoragePartition::SetDefaultQuotaSettingsForTesting(&quota_settings);

    GetControlTest()->BindMockFailureSingletonForTesting(
        failure_injector_.BindNewPipeAndPassReceiver());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable experimental web platform features to enable write access.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void TearDownOnMainThread() override { failure_injector_.reset(); }

  bool UseProductionQuotaSettings() override {
    // So that the browser test harness doesn't call
    // SetDefaultQuotaSettingsForTesting and overwrite the settings above.
    return true;
  }

  void FailOperation(FailClass failure_class,
                     FailMethod failure_method,
                     int fail_on_instance_num,
                     int fail_on_call_num) {
    base::RunLoop loop;
    FailOperationWithCallback(failure_class, failure_method,
                              fail_on_instance_num, fail_on_call_num,
                              loop.QuitClosure());
    loop.Run();
  }

  void FailOperationWithCallback(FailClass failure_class,
                                 FailMethod failure_method,
                                 int fail_on_instance_num,
                                 int fail_on_call_num,
                                 base::OnceClosure callback) {
    failure_injector_->FailOperation(failure_class, failure_method,
                                     fail_on_instance_num, fail_on_call_num,
                                     std::move(callback));
  }

  void SimpleTest(const GURL& test_url,
                  bool incognito = false,
                  Shell** shell_out = nullptr) {
    // The test page will perform tests on IndexedDB, then navigate to either
    // a #pass or #fail ref.
    Shell* the_browser = incognito ? CreateOffTheRecordBrowser() : shell();

    VLOG(0) << "Navigating to URL and blocking.";
    NavigateToURLBlockUntilNavigationsComplete(the_browser, test_url, 2);
    VLOG(0) << "Navigation done.";
    std::string result =
        the_browser->web_contents()->GetLastCommittedURL().ref();
    if (result != "pass") {
      std::string js_result = EvalJs(the_browser, "getLog()").ExtractString();
      FAIL() << "Failed: " << js_result;
    }
    if (shell_out)
      *shell_out = the_browser;
  }

  void NavigateAndWaitForTitle(Shell* shell,
                               const char* filename,
                               const char* hash,
                               const char* expected_string) {
    GURL url = GetTestUrl("indexeddb", filename);
    if (hash)
      url = GURL(url.spec() + hash);

    std::u16string expected_title16(base::ASCIIToUTF16(expected_string));
    TitleWatcher title_watcher(shell->web_contents(), expected_title16);
    EXPECT_TRUE(NavigateToURL(shell, url));
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  storage::mojom::IndexedDBControl& GetControl(Shell* browser = nullptr) {
    if (!browser)
      browser = shell();
    StoragePartition* partition = browser->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    return partition->GetIndexedDBControl();
  }

  mojo::Remote<storage::mojom::IndexedDBControlTest> GetControlTest() {
    mojo::Remote<storage::mojom::IndexedDBControlTest> idb_control_test;
    BindControlTest(idb_control_test.BindNewPipeAndPassReceiver());
    return idb_control_test;
  }

  void BindControlTest(
      mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver) {
    auto* browser = shell();
    StoragePartition* partition = browser->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    auto& control = partition->GetIndexedDBControl();
    control.BindTestInterface(std::move(receiver));
  }

  void SetQuota(int per_host_quota_kilobytes) {
    SetTempQuota(per_host_quota_kilobytes, shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition()
                                               ->GetQuotaManager());
  }

  static void SetTempQuota(int per_host_quota_kilobytes,
                           scoped_refptr<QuotaManager> qm) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&IndexedDBBrowserTest::SetTempQuota,
                                    per_host_quota_kilobytes, qm));
      return;
    }
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    const int KB = 1024;
    qm->SetQuotaSettings(
        storage::GetHardCodedSettings(per_host_quota_kilobytes * KB));
  }

  bool DeleteForStorageKey(const blink::StorageKey& storage_key,
                           Shell* browser = nullptr) {
    base::RunLoop loop;
    auto& control = GetControl(browser);
    bool result = false;
    control.DeleteForStorageKey(storage_key,
                                base::BindLambdaForTesting([&](bool success) {
                                  result = success;
                                  loop.Quit();
                                }));
    loop.Run();
    return result;
  }

  int64_t RequestUsage(const blink::StorageKey& storage_key,
                       Shell* browser = nullptr) {
    base::RunLoop loop;
    int64_t size = 0;
    auto& control = GetControl(browser);
    control.GetUsage(base::BindLambdaForTesting(
        [&](std::vector<storage::mojom::StorageUsageInfoPtr> usages) {
          for (auto& usage : usages)
            size += usage->total_size_bytes;
          loop.Quit();
        }));
    loop.Run();
    return size;
  }

  int64_t RequestBlobFileCount(const storage::BucketLocator& bucket_locator) {
    base::RunLoop loop;
    int64_t count = 0;
    auto control_test = GetControlTest();
    control_test->GetBlobCountForTesting(
        bucket_locator, base::BindLambdaForTesting([&](int64_t returned_count) {
          count = returned_count;
          loop.Quit();
        }));
    loop.Run();
    return count;
  }

  bool RequestSchemaDowngrade(const storage::BucketLocator& bucket_locator) {
    base::RunLoop loop;
    bool downgraded;
    auto control_test = GetControlTest();
    control_test->ForceSchemaDowngradeForTesting(
        bucket_locator, base::BindLambdaForTesting([&](bool was_downgraded) {
          downgraded = was_downgraded;
          loop.Quit();
        }));
    loop.Run();
    return downgraded;
  }

  storage::mojom::V2SchemaCorruptionStatus RequestHasV2SchemaCorruption(
      const storage::BucketLocator& bucket_locator) {
    base::RunLoop loop;
    storage::mojom::V2SchemaCorruptionStatus ret;
    auto control_test = GetControlTest();
    control_test->HasV2SchemaCorruptionForTesting(
        bucket_locator,
        base::BindLambdaForTesting(
            [&](storage::mojom::V2SchemaCorruptionStatus status) {
              ret = status;
              loop.Quit();
            }));
    loop.Run();
    return ret;
  }

  // Synchronously writes to the IndexedDB database at the given storage_key.
  void WriteToIndexedDB(const storage::BucketLocator& bucket_locator,
                        std::string key,
                        std::string value) {
    auto control_test = GetControlTest();
    base::RunLoop loop;
    control_test->WriteToIndexedDBForTesting(
        bucket_locator, std::move(key), std::move(value), loop.QuitClosure());
    loop.Run();
  }

  storage::QuotaErrorOr<storage::BucketInfo> GetOrCreateBucket(
      const storage::BucketInitParams& params) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetQuotaManager()
        ->proxy()
        ->UpdateOrCreateBucket(
            params, base::SingleThreadTaskRunner::GetCurrentDefault(),
            future.GetCallback());
    return future.Take();
  }

 private:
  mojo::Remote<storage::mojom::MockFailureInjector> failure_injector_;
};

class IndexedDBIncognitoTest : public IndexedDBBrowserTest,
                               public ::testing::WithParamInterface<bool> {
 public:
  IndexedDBIncognitoTest() = default;

  bool IsIncognito() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(IndexedDBIncognitoTest, CursorTest) {
  SimpleTest(GetTestUrl("indexeddb", "cursor_test.html"), IsIncognito());
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorPrefetch) {
  SimpleTest(GetTestUrl("indexeddb", "cursor_prefetch.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, IndexTest) {
  SimpleTest(GetTestUrl("indexeddb", "index_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, KeyPathTest) {
  SimpleTest(GetTestUrl("indexeddb", "key_path_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionGetTest) {
  SimpleTest(GetTestUrl("indexeddb", "transaction_get_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, KeyTypesTest) {
  SimpleTest(GetTestUrl("indexeddb", "key_types_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ObjectStoreTest) {
  SimpleTest(GetTestUrl("indexeddb", "object_store_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DatabaseTest) {
  SimpleTest(GetTestUrl("indexeddb", "database_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionTest) {
  SimpleTest(GetTestUrl("indexeddb", "transaction_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CallbackAccounting) {
  SimpleTest(GetTestUrl("indexeddb", "callback_accounting.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DoesntHangTest) {
  SimpleTest(GetTestUrl("indexeddb", "transaction_run_forever.html"));
  CrashTab(shell()->web_contents());
  SimpleTest(GetTestUrl("indexeddb", "transaction_not_blocked.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug84933Test) {
  const GURL url = GetTestUrl("indexeddb", "bug_84933.html");

  // Just navigate to the URL. Test will crash if it fails.
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug106883Test) {
  const GURL url = GetTestUrl("indexeddb", "bug_106883.html");

  // Just navigate to the URL. Test will crash if it fails.
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug109187Test) {
  const GURL url = GetTestUrl("indexeddb", "bug_109187.html");

  // Just navigate to the URL. Test will crash if it fails.
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug941965Test) {
  // Double-open an incognito window to test that saving & reading a blob from
  // indexeddb works.
  Shell* incognito_browser = nullptr;
  SimpleTest(GetTestUrl("indexeddb", "simple_blob_read.html"), true,
             &incognito_browser);
  ASSERT_TRUE(incognito_browser);
  incognito_browser->Close();
  incognito_browser = nullptr;
  SimpleTest(GetTestUrl("indexeddb", "simple_blob_read.html"), true,
             &incognito_browser);
  ASSERT_TRUE(incognito_browser);
  incognito_browser->Close();
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, NegativeDBSchemaVersion) {
  const GURL database_open_url = GetTestUrl("indexeddb", "database_test.html");

  // Create the database.
  SimpleTest(database_open_url);
  // -10, little endian.
  std::string value = "\xF6\xFF\xFF\xFF\xFF\xFF\xFF\xFF";

  // Find the bucket that was created.
  const auto maybe_bucket_info =
      GetOrCreateBucket(storage::BucketInitParams::ForDefaultBucket(
          blink::StorageKey::CreateFirstParty(
              url::Origin::Create(database_open_url))));
  ASSERT_TRUE(maybe_bucket_info.has_value());
  const auto bucket_locator = maybe_bucket_info->ToBucketLocator();

  auto control_test = GetControlTest();
  base::RunLoop loop;
  std::string key;
  control_test->GetDatabaseKeysForTesting(
      base::BindLambdaForTesting([&](const std::string& schema_version_key,
                                     const std::string& data_version_key) {
        key = schema_version_key;
        loop.Quit();
      }));
  loop.Run();

  WriteToIndexedDB(bucket_locator, key, value);
  // Crash the tab to ensure no old navigations are picked up.
  CrashTab(shell()->web_contents());
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, NegativeDBDataVersion) {
  const GURL database_open_url = GetTestUrl("indexeddb", "database_test.html");

  // Create the database.
  SimpleTest(database_open_url);
  // -10, little endian.
  std::string value = "\xF6\xFF\xFF\xFF\xFF\xFF\xFF\xFF";

  // Find the bucket that was created.
  const auto maybe_bucket_info =
      GetOrCreateBucket(storage::BucketInitParams::ForDefaultBucket(
          blink::StorageKey::CreateFirstParty(
              url::Origin::Create(database_open_url))));
  ASSERT_TRUE(maybe_bucket_info.has_value());
  const auto bucket_locator = maybe_bucket_info->ToBucketLocator();

  auto control_test = GetControlTest();
  base::RunLoop loop;
  std::string key;
  control_test->GetDatabaseKeysForTesting(
      base::BindLambdaForTesting([&](const std::string& schema_version_key,
                                     const std::string& data_version_key) {
        key = data_version_key;
        loop.Quit();
      }));
  loop.Run();

  WriteToIndexedDB(bucket_locator, key, value);
  // Crash the tab to ensure no old navigations are picked up.
  CrashTab(shell()->web_contents());
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
}

class IndexedDBBrowserTestWithLowQuota : public IndexedDBBrowserTest {
 public:
  IndexedDBBrowserTestWithLowQuota() = default;

  IndexedDBBrowserTestWithLowQuota(const IndexedDBBrowserTestWithLowQuota&) =
      delete;
  IndexedDBBrowserTestWithLowQuota& operator=(
      const IndexedDBBrowserTestWithLowQuota&) = delete;

  void SetUpOnMainThread() override {
    const int kInitialQuotaKilobytes = 5000;
    SetQuota(kInitialQuotaKilobytes);
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithLowQuota, QuotaTest) {
  SimpleTest(GetTestUrl("indexeddb", "quota_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithLowQuota, QuotaTestWithCommit) {
  SimpleTest(GetTestUrl("indexeddb", "bug_1203335.html"));
}

class IndexedDBBrowserTestWithGCExposed : public IndexedDBBrowserTest {
 public:
  IndexedDBBrowserTestWithGCExposed() = default;

  IndexedDBBrowserTestWithGCExposed(const IndexedDBBrowserTestWithGCExposed&) =
      delete;
  IndexedDBBrowserTestWithGCExposed& operator=(
      const IndexedDBBrowserTestWithGCExposed&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--expose-gc");
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithGCExposed,
                       DatabaseCallbacksTest) {
  SimpleTest(GetTestUrl("indexeddb", "database_callbacks_first.html"));
}

struct BlobModificationTime {
  base::FilePath relative_blob_path;
  base::Time time;
};

static void CopyLevelDBToProfile(
    Shell* shell,
    const base::FilePath& data_path,
    const std::string& test_directory,
    std::vector<BlobModificationTime> modification_times) {
  base::FilePath leveldb_dir(FILE_PATH_LITERAL("file__0.indexeddb.leveldb"));
  base::FilePath blob_dir(FILE_PATH_LITERAL("file__0.indexeddb.blob"));
  base::FilePath test_leveldb_data_dir =
      GetTestFilePath("indexeddb", test_directory.c_str()).Append(leveldb_dir);
  base::FilePath test_blob_data_dir =
      GetTestFilePath("indexeddb", test_directory.c_str()).Append(blob_dir);
  base::FilePath leveldb_dest = data_path.Append(leveldb_dir);
  base::FilePath blob_dest = data_path.Append(blob_dir);
  // If we don't create the destination directories first, the contents of the
  // leveldb directory are copied directly into profile/IndexedDB instead of
  // profile/IndexedDB/file__0.xxx/
  ASSERT_TRUE(base::CreateDirectory(leveldb_dest));
  const bool kRecursive = true;
  ASSERT_TRUE(
      base::CopyDirectory(test_leveldb_data_dir, data_path, kRecursive));

  if (!base::PathExists(test_blob_data_dir))
    return;
  ASSERT_TRUE(base::CreateDirectory(blob_dest));
  ASSERT_TRUE(base::CopyDirectory(test_blob_data_dir, data_path, kRecursive));
  // For some reason touching files on Android fails with EPERM.
  // https://crbug.com/1045488
#if !BUILDFLAG(IS_ANDROID)
  // The modification time of the saved blobs is used for File objects, so these
  // need to manually be set (they are clobbered both by the above copy
  // operation and by git).
  for (const BlobModificationTime& time : modification_times) {
    base::FilePath total_path = blob_dest.Append(time.relative_blob_path);
    ASSERT_TRUE(base::TouchFile(total_path, time.time, time.time));
  }
#endif
}

class IndexedDBBrowserTestWithPreexistingLevelDB : public IndexedDBBrowserTest {
 public:
  IndexedDBBrowserTestWithPreexistingLevelDB() = default;

  IndexedDBBrowserTestWithPreexistingLevelDB(
      const IndexedDBBrowserTestWithPreexistingLevelDB&) = delete;
  IndexedDBBrowserTestWithPreexistingLevelDB& operator=(
      const IndexedDBBrowserTestWithPreexistingLevelDB&) = delete;

  void SetUpOnMainThread() override {
    base::RunLoop loop_move;
    auto control_test = GetControlTest();
    control_test->GetBaseDataPathForTesting(
        base::BindLambdaForTesting([&](const base::FilePath& data_path) {
          CopyLevelDBToProfile(shell(), data_path, EnclosingLevelDBDir(),
                               CustomModificationTimes());
          loop_move.Quit();
        }));
    loop_move.Run();
    base::RunLoop loop_init;
    control_test->ForceInitializeFromFilesForTesting(
        base::BindLambdaForTesting([&]() { loop_init.Quit(); }));
    loop_init.Run();
  }

  virtual std::string EnclosingLevelDBDir() = 0;

  virtual std::vector<BlobModificationTime> CustomModificationTimes() {
    return std::vector<BlobModificationTime>();
  }
};

class IndexedDBBrowserTestWithVersion0Schema : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "migration_from_0"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion0Schema, MigrationTest) {
  SimpleTest(GetTestUrl("indexeddb", "migration_test.html"));
}

class IndexedDBBrowserTestWithVersion3Schema
    : public IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "v3_migration_test"; }

  std::vector<BlobModificationTime> CustomModificationTimes() override {
    return {
        {base::FilePath(FILE_PATH_LITERAL("1/00/3")),
         base::Time::FromJsTime(1579809038000)},
        {base::FilePath(FILE_PATH_LITERAL("1/00/4")),
         base::Time::FromJsTime(1579808985000)},
        {base::FilePath(FILE_PATH_LITERAL("1/00/5")),
         base::Time::FromJsTime(1579199256000)},
    };
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion3Schema, MigrationTest) {
  const GURL kTestUrl = GetTestUrl("indexeddb", "v3_migration_test.html");
  // For some reason setting empty file modification time on Android fails with
  // EPERM. https://crbug.com/1045488
#if BUILDFLAG(IS_ANDROID)
  SimpleTest(GURL(kTestUrl.spec() + "#ignoreTimes"));
#else
  SimpleTest(kTestUrl);
#endif
}

class IndexedDBBrowserTestWithVersion123456Schema : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "schema_version_123456"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion123456Schema,
                       DestroyTest) {
  const GURL kTestUrl = GetTestUrl("indexeddb", "open_bad_db.html");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl));
  int64_t original_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(original_size, 0);
  SimpleTest(kTestUrl);
  int64_t new_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(new_size, 0);
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithVersion987654SSVData : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "ssv_version_987654"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion987654SSVData,
                       DestroyTest) {
  const GURL kTestUrl = GetTestUrl("indexeddb", "open_bad_db.html");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl));
  int64_t original_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(original_size, 0);
  SimpleTest(kTestUrl);
  int64_t new_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(new_size, 0);
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithCorruptLevelDB : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "corrupt_leveldb"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithCorruptLevelDB,
                       DestroyTest) {
  const GURL kTestUrl = GetTestUrl("indexeddb", "open_bad_db.html");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl));
  int64_t original_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(original_size, 0);
  SimpleTest(kTestUrl);
  int64_t new_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(new_size, 0);
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithMissingSSTFile : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "missing_sst"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithMissingSSTFile,
                       DestroyTest) {
  const GURL kTestUrl = GetTestUrl("indexeddb", "open_missing_table.html");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl));
  int64_t original_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(original_size, 0);
  SimpleTest(kTestUrl);
  int64_t new_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(new_size, 0);
  EXPECT_NE(original_size, new_size);
}

// IndexedDBBrowserTestWithCrbug899446* capture IDB instances from Chrome stable
// to verify that the current code can read those instances.  For more info on
// a case when Chrome canary couldn't read stable's IDB instances, see
// https://crbug.com/899446.

class IndexedDBBrowserTestWithCrbug899446
    : public IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "crbug899446"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithCrbug899446, StableTest) {
  const GURL kTestUrl = GetTestUrl("indexeddb", "crbug899446.html");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl));
  int64_t original_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(original_size, 0);
  SimpleTest(kTestUrl);
  int64_t new_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(new_size, 0);
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithCrbug899446Noai
    : public IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "crbug899446_noai"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithCrbug899446Noai, StableTest) {
  const GURL kTestUrl = GetTestUrl("indexeddb", "crbug899446_noai.html");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl));
  int64_t original_size = RequestUsage(kTestStorageKey);
  SimpleTest(kTestUrl);
  int64_t new_size = RequestUsage(kTestStorageKey);
  EXPECT_GT(new_size, 0);
  EXPECT_NE(original_size, new_size);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, LevelDBLogFileTest) {
  // Any page that opens an IndexedDB will work here.
  SimpleTest(GetTestUrl("indexeddb", "database_test.html"));
  base::FilePath leveldb_dir(FILE_PATH_LITERAL("file__0.indexeddb.leveldb"));
  base::FilePath log_file(FILE_PATH_LITERAL("LOG"));

  base::FilePath log_file_path;
  base::RunLoop loop;
  auto control_test = GetControlTest();
  control_test->GetBaseDataPathForTesting(
      base::BindLambdaForTesting([&](const base::FilePath& path) {
        log_file_path = path.Append(leveldb_dir).Append(log_file);
        loop.Quit();
      }));
  loop.Run();

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int64_t size;
    EXPECT_TRUE(base::GetFileSize(log_file_path, &size));
    EXPECT_GT(size, 0);
  }
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CanDeleteWhenOverQuotaTest) {
  const GURL kTestUrl = GetTestUrl("indexeddb", "fill_up_5k.html");
  SimpleTest(kTestUrl);
  int64_t size = RequestUsage(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl)));
  const int kQuotaKilobytes = 2;
  EXPECT_GT(size, kQuotaKilobytes * 1024);
  SetQuota(kQuotaKilobytes);
  SimpleTest(GetTestUrl("indexeddb", "delete_over_quota.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, EmptyBlob) {
  // First delete all IDB's for the test storage_key
  const GURL kTestUrl = GetTestUrl("indexeddb", "empty_blob.html");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl));
  DeleteForStorageKey(kTestStorageKey);
  const auto maybe_bucket_info = GetOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(kTestStorageKey));
  ASSERT_TRUE(maybe_bucket_info.has_value());
  const auto bucket_locator = maybe_bucket_info->ToBucketLocator();
  EXPECT_EQ(0,
            RequestBlobFileCount(bucket_locator));  // Start with no blob files.
  // For some reason Android's futimes fails (EPERM) in this test. Do not assert
  // file times on Android, but do so on other platforms. crbug.com/467247
  // TODO(cmumford): Figure out why this is the case and fix if possible.
#if BUILDFLAG(IS_ANDROID)
  SimpleTest(GURL(kTestUrl.spec() + "#ignoreTimes"));
#else
  SimpleTest(kTestUrl);
#endif
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, BlobsCountAgainstQuota) {
  SimpleTest(GetTestUrl("indexeddb", "blobs_use_quota.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DeleteForStorageKeyDeletesBlobs) {
  const GURL kTestUrl = GetTestUrl("indexeddb", "write_4mb_blob.html");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl));
  SimpleTest(kTestUrl);
  int64_t size = RequestUsage(kTestStorageKey);
  // This assertion assumes that we do not compress blobs.
  EXPECT_GT(size, 4 << 20 /* 4 MB */);
  DeleteForStorageKey(kTestStorageKey);
  EXPECT_EQ(0, RequestUsage(kTestStorageKey));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DeleteForStorageKeyIncognito) {
  const GURL test_url = GetTestUrl("indexeddb", "fill_up_5k.html");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(test_url));

  Shell* browser = CreateOffTheRecordBrowser();
  NavigateToURLBlockUntilNavigationsComplete(browser, test_url, 2);

  EXPECT_GT(RequestUsage(kTestStorageKey, browser), 5 * 1024);

  DeleteForStorageKey(kTestStorageKey, browser);

  EXPECT_EQ(0, RequestUsage(kTestStorageKey, browser));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DiskFullOnCommit) {
  // Ignore several preceding transactions:
  // * The test calls deleteDatabase() which opens the backing store:
  //   #1: IndexedDBTransaction::Commit - initial "versionchange" transaction
  // * Once the connection is opened, the test runs:
  //   #2: IndexedDBTransaction::Commit - the test's "readwrite" transaction)
  const int instance_num = 2;
  const int call_num = 1;
  FailOperation(FailClass::LEVELDB_TRANSACTION, FailMethod::COMMIT_DISK_FULL,
                instance_num, call_num);
  SimpleTest(GetTestUrl("indexeddb", "disk_full_on_commit.html"));
}

namespace {

std::unique_ptr<net::test_server::HttpResponse> ServePath(
    std::string request_path) {
  base::FilePath resource_path =
      content::GetTestFilePath("indexeddb", request_path.c_str());
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  std::string file_contents;
  if (!base::ReadFileToString(resource_path, &file_contents))
    NOTREACHED() << "could not read file " << resource_path;
  http_response->set_content(file_contents);
  return std::move(http_response);
}

#if !BUILDFLAG(IS_WIN)
void CorruptIndexedDBDatabase(const base::FilePath& idb_data_path) {
  int num_files = 0;
  int num_errors = 0;
  const bool recursive = false;

  base::FileEnumerator enumerator(idb_data_path, recursive,
                                  base::FileEnumerator::FILES);
  for (base::FilePath idb_file = enumerator.Next(); !idb_file.empty();
       idb_file = enumerator.Next()) {
    int64_t size(0);
    GetFileSize(idb_file, &size);

    if (idb_file.Extension() == FILE_PATH_LITERAL(".ldb")) {
      num_files++;
      base::File file(idb_file,
                      base::File::FLAG_WRITE | base::File::FLAG_OPEN_TRUNCATED);
      if (file.IsValid()) {
        // Was opened truncated, expand back to the original
        // file size and fill with zeros (corrupting the file).
        file.SetLength(size);
      } else {
        num_errors++;
      }
    }
  }
  VLOG(0) << "There were " << num_files << " in " << idb_data_path.value()
          << " with " << num_errors << " errors";
}

const char s_corrupt_db_test_prefix[] = "/corrupt/test/";

std::unique_ptr<net::test_server::HttpResponse> CorruptDBRequestHandler(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const storage::BucketLocator& bucket_locator,
    const std::string& path,
    IndexedDBBrowserTest* test,
    const net::test_server::HttpRequest& request) {
  std::string request_path;
  if (path.find(s_corrupt_db_test_prefix) == std::string::npos)
    return nullptr;

  request_path =
      request.relative_url.substr(std::string(s_corrupt_db_test_prefix).size());

  // Remove the query string if present.
  std::string request_query;
  size_t query_pos = request_path.find('?');
  if (query_pos != std::string::npos) {
    request_query = request_path.substr(query_pos + 1);
    request_path = request_path.substr(0, query_pos);
  }

  if (request_path == "corruptdb" && !request_query.empty()) {
    VLOG(0) << "Requested to corrupt IndexedDB: " << request_query;

    // BindControlTest must be called on the same sequence that
    // IndexedDBBrowserTest lives on.
    mojo::Remote<storage::mojom::IndexedDBControlTest> control_test;
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&IndexedDBBrowserTest::BindControlTest,
                                  base::Unretained(test),
                                  control_test.BindNewPipeAndPassReceiver()));

    // TODO(enne): this is a nested message loop on the embedded test server's
    // IO thread.  Windows does not support such nested message loops.
    // However, alternatives like WaitableEvent can't be used here because
    // these potentially cross-process mojo calls have callbacks that will
    // bounce through the IO thread, causing a deadlock if we wait here.
    // The ideal solution here is to refactor the embedded test server
    // to support asynchronous request handlers (if possible??).
    // The less ideal temporary solution is to only run these tests on Windows.
    base::RunLoop loop;
    control_test->CompactBackingStoreForTesting(
        bucket_locator, base::BindLambdaForTesting([&]() {
          control_test->GetFilePathForTesting(
              bucket_locator,
              base::BindLambdaForTesting([&](const base::FilePath& path) {
                CorruptIndexedDBDatabase(path);
                loop.Quit();
              }));
        }));
    loop.Run();

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    return std::move(http_response);
  } else if (request_path == "fail" && !request_query.empty()) {
    FailClass failure_class = FailClass::NOTHING;
    FailMethod failure_method = FailMethod::NOTHING;
    int instance_num = 1;
    int call_num = 1;
    std::string fail_class;
    std::string fail_method;

    url::Component query(0, request_query.length()), key_pos, value_pos;
    while (url::ExtractQueryKeyValue(
        request_query.c_str(), &query, &key_pos, &value_pos)) {
      std::string escaped_key(request_query.substr(key_pos.begin, key_pos.len));
      std::string escaped_value(
          request_query.substr(value_pos.begin, value_pos.len));

      std::string key = base::UnescapeBinaryURLComponent(escaped_key);

      std::string value = base::UnescapeBinaryURLComponent(escaped_value);

      if (key == "method")
        fail_method = value;
      else if (key == "class")
        fail_class = value;
      else if (key == "instNum")
        instance_num = atoi(value.c_str());
      else if (key == "callNum")
        call_num = atoi(value.c_str());
      else
        NOTREACHED() << "Unknown param: \"" << key << "\"";
    }

    if (fail_class == "LevelDBTransaction") {
      failure_class = FailClass::LEVELDB_TRANSACTION;
      if (fail_method == "Get")
        failure_method = FailMethod::GET;
      else if (fail_method == "Commit")
        failure_method = FailMethod::COMMIT;
      else
        NOTREACHED() << "Unknown method: \"" << fail_method << "\"";
    } else if (fail_class == "LevelDBIterator") {
      failure_class = FailClass::LEVELDB_ITERATOR;
      if (fail_method == "Seek")
        failure_method = FailMethod::SEEK;
      else
        NOTREACHED() << "Unknown method: \"" << fail_method << "\"";
    } else if (fail_class == "LevelDBDatabase") {
      failure_class = FailClass::LEVELDB_DATABASE;
      if (fail_method == "Write")
        failure_method = FailMethod::WRITE;
      else
        NOTREACHED() << "Unknown method: \"" << fail_method << "\"";
    } else if (fail_class == "LevelDBDirectTransaction") {
      failure_class = FailClass::LEVELDB_DIRECT_TRANSACTION;
      if (fail_method == "Get")
        failure_method = FailMethod::GET;
      else
        NOTREACHED() << "Unknown method: \"" << fail_method << "\"";
    } else {
      NOTREACHED() << "Unknown class: \"" << fail_class << "\"";
    }

    DCHECK_GE(instance_num, 1);
    DCHECK_GE(call_num, 1);

    base::RunLoop loop;
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&IndexedDBBrowserTest::FailOperationWithCallback,
                       base::Unretained(test), failure_class, failure_method,
                       instance_num, call_num, loop.QuitClosure()));
    loop.Run();

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    return std::move(http_response);
  }

  return ServePath(request_path);
}
#endif

const char s_indexeddb_test_prefix[] = "/indexeddb/test/";

std::unique_ptr<net::test_server::HttpResponse> StaticFileRequestHandler(
    const std::string& path,
    IndexedDBBrowserTest* test,
    const net::test_server::HttpRequest& request) {
  if (path.find(s_indexeddb_test_prefix) == std::string::npos)
    return nullptr;
  std::string request_path =
      request.relative_url.substr(std::string(s_indexeddb_test_prefix).size());
  return ServePath(request_path);
}

}  // namespace

// See TODO in CorruptDBRequestHandler.  Windows does not support nested
// message loops on the IO thread, so run this test on other platforms.
#if !BUILDFLAG(IS_WIN)
class IndexedDBBrowserTestWithCorruption
    : public IndexedDBBrowserTest,
      public ::testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         IndexedDBBrowserTestWithCorruption,
                         ::testing::Values("failGetBlobJournal",
                                           "get",
                                           "getAll",
                                           "iterate",
                                           "failTransactionCommit",
                                           "clearObjectStore"));

IN_PROC_BROWSER_TEST_P(IndexedDBBrowserTestWithCorruption,
                       OperationOnCorruptedOpenDatabase) {
  ASSERT_TRUE(embedded_test_server()->Started() ||
              embedded_test_server()->InitializeAndListen());
  const blink::StorageKey storage_key = blink::StorageKey::CreateFirstParty(
      url::Origin::Create(embedded_test_server()->base_url()));
  base::RunLoop loop;
  storage::BucketLocator bucket_locator;
  shell()
      ->web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetQuotaManager()
      ->proxy()
      ->UpdateOrCreateBucket(
          storage::BucketInitParams::ForDefaultBucket(storage_key),
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindLambdaForTesting(
              [&](storage::QuotaErrorOr<storage::BucketInfo> result) {
                ASSERT_TRUE(result.has_value());
                bucket_locator = result->ToBucketLocator();
                loop.Quit();
              }));
  loop.Run();
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &CorruptDBRequestHandler, base::SequencedTaskRunner::GetCurrentDefault(),
      bucket_locator, s_corrupt_db_test_prefix, this));
  embedded_test_server()->StartAcceptingConnections();

  std::string test_file = std::string(s_corrupt_db_test_prefix) +
                          "corrupted_open_db_detection.html#" + GetParam();
  SimpleTest(embedded_test_server()->GetURL(test_file));

  test_file =
      std::string(s_corrupt_db_test_prefix) + "corrupted_open_db_recovery.html";
  SimpleTest(embedded_test_server()->GetURL(test_file));
}
#endif  // !BUILDFLAG(IS_WIN)

// TODO: http://crbug.com/510520, flaky on all platforms
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest,
                       DISABLED_DeleteCompactsBackingStore) {
  const GURL kTestUrl = GetTestUrl("indexeddb", "delete_compact.html");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl));
  SimpleTest(GURL(kTestUrl.spec() + "#fill"));

  int64_t after_filling = RequestUsage(kTestStorageKey);
  EXPECT_GT(after_filling, 0);

  SimpleTest(GURL(kTestUrl.spec() + "#purge"));
  int64_t after_deleting = RequestUsage(kTestStorageKey);
  EXPECT_LT(after_deleting, after_filling);

  // The above tests verify basic assertions - that filling writes data and
  // deleting reduces the amount stored.

  // The below tests make assumptions about implementation specifics, such as
  // data compression, compaction efficiency, and the maximum amount of
  // metadata and log data remains after a deletion. It is possible that
  // changes to the implementation may require these constants to be tweaked.

  // 1MB, as sometimes the leveldb log is compacted to .ldb files, which are
  // compressed.
  const int kTestFillBytes = 1 * 1024 * 1024;
  EXPECT_GT(after_filling, kTestFillBytes);

  const int kTestCompactBytes = 300 * 1024;  // 300kB
  EXPECT_LT(after_deleting, kTestCompactBytes);
}

// Complex multi-step (converted from pyauto) tests begin here.

// Verify null key path persists after restarting browser.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, PRE_NullKeyPathPersistence) {
  NavigateAndWaitForTitle(shell(), "bug_90635.html", "#part1",
                          "pass - first run");
}

// Verify null key path persists after restarting browser.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, NullKeyPathPersistence) {
  NavigateAndWaitForTitle(shell(), "bug_90635.html", "#part2",
                          "pass - second run");
}

// Verify that a VERSION_CHANGE transaction is rolled back after a
// renderer/browser crash
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest,
                       PRE_PRE_VersionChangeCrashResilience) {
  NavigateAndWaitForTitle(shell(), "version_change_crash.html", "#part1",
                          "pass - part1 - complete");
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, PRE_VersionChangeCrashResilience) {
  NavigateAndWaitForTitle(shell(), "version_change_crash.html", "#part2",
                          "pass - part2 - crash me");
  // Previously this test would abruptly terminate the browser process
  // to ensure that the version update was not partially committed,
  // which was possible in the very early implementation (circa 2011).
  // This test no longer abruptly terminates the process, but the
  // commit scheme has changed so it's not plausible any more anyway.
  // TODO(jsbell): Delete or rename the test.
}

// Fails to cleanup GPU processes on swarming.
// http://crbug.com/552543
// Flaky on TSAN: crbug.com/1061251
// Flaky on mac, linux, cast, chromeos, lacros bots: crbug.com/1061251
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest,
                       DISABLED_VersionChangeCrashResilience) {
  NavigateAndWaitForTitle(shell(), "version_change_crash.html", "#part3",
                          "pass - part3 - rolled back");
}

// Disable this test on Android due to failures. See crbug.com/427529 and
// crbug.com/1116464 for details.
#if defined(ANDROID)
#define MAYBE_ConnectionsClosedOnTabClose DISABLED_ConnectionsClosedOnTabClose
#else
#define MAYBE_ConnectionsClosedOnTabClose ConnectionsClosedOnTabClose
#endif
// Verify that open DB connections are closed when a tab is destroyed.
IN_PROC_BROWSER_TEST_F(
    IndexedDBBrowserTest, MAYBE_ConnectionsClosedOnTabClose) {
  NavigateAndWaitForTitle(shell(), "version_change_blocked.html", "#tab1",
                          "setVersion(2) complete");

  // Start on a different URL to force a new renderer process.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, GURL(url::kAboutBlankURL)));
  NavigateAndWaitForTitle(new_shell, "version_change_blocked.html", "#tab2",
                          "setVersion(3) blocked");

  std::u16string expected_title16(u"setVersion(3) complete");
  TitleWatcher title_watcher(new_shell->web_contents(), expected_title16);

  shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->Shutdown(0);
  shell()->Close();

  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

// Verify that a "close" event is fired at database connections when
// the backing store is deleted.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ForceCloseEventTest) {
  constexpr char kFilename[] = "force_close_event.html";
  NavigateAndWaitForTitle(shell(), kFilename, nullptr, "connection ready");
  DeleteForStorageKey(blink::StorageKey::CreateFirstParty(
      url::Origin::Create(GetTestUrl("indexeddb", kFilename))));
  std::u16string expected_title16(u"connection closed");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
  title_watcher.AlsoWaitForTitle(u"connection closed with error");
  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

// The V2 schema corruption test runs in a separate class to avoid corrupting
// an IDB store that other tests use.
class IndexedDBBrowserTestV2SchemaCorruption : public IndexedDBBrowserTest {};

// Verify the V2 schema corruption lifecycle:
// - create a current version backing store (v3 or later)
// - add an object store, some data, and an object that contains a blob
// - verify the object+blob are stored in the object store
// - verify the backing store doesn't have v2 schema corruption
// - force the schema to downgrade to v2
// - verify the backing store has v2 schema corruption
// - verify the object+blob can be fetched
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestV2SchemaCorruption, LifecycleTest) {
  ASSERT_TRUE(embedded_test_server()->Started() ||
              embedded_test_server()->InitializeAndListen());
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &StaticFileRequestHandler, s_indexeddb_test_prefix, this));
  embedded_test_server()->StartAcceptingConnections();

  // Set up the IndexedDB instance so it contains our reference data.
  std::string test_file =
      std::string(s_indexeddb_test_prefix) + "v2schemacorrupt_setup.html";
  SimpleTest(embedded_test_server()->GetURL(test_file));

  // Find the bucket that was created.
  const auto maybe_bucket_info =
      GetOrCreateBucket(storage::BucketInitParams::ForDefaultBucket(
          blink::StorageKey::CreateFirstParty(
              url::Origin::Create(embedded_test_server()->base_url()))));
  ASSERT_TRUE(maybe_bucket_info.has_value());
  const auto bucket_locator = maybe_bucket_info->ToBucketLocator();

  // Verify the backing store does not have corruption.
  storage::mojom::V2SchemaCorruptionStatus has_corruption =
      RequestHasV2SchemaCorruption(bucket_locator);
  ASSERT_EQ(has_corruption,
            storage::mojom::V2SchemaCorruptionStatus::CORRUPTION_NO);

  // Revert schema to v2.  This closes the targeted backing store.
  bool schema_downgrade = RequestSchemaDowngrade(bucket_locator);
  ASSERT_EQ(schema_downgrade, true);

  // Re-open the backing store and verify it has corruption.
  test_file =
      std::string(s_indexeddb_test_prefix) + "v2schemacorrupt_reopen.html";
  SimpleTest(embedded_test_server()->GetURL(test_file));
  has_corruption = RequestHasV2SchemaCorruption(bucket_locator);
  ASSERT_EQ(has_corruption,
            storage::mojom::V2SchemaCorruptionStatus::CORRUPTION_YES);

  // Verify that the saved blob is get-able with a v2 backing store.
  test_file =
      std::string(s_indexeddb_test_prefix) + "v2schemacorrupt_verify.html";
  SimpleTest(embedded_test_server()->GetURL(test_file));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ShutdownWithRequests) {
  SimpleTest(GetTestUrl("indexeddb", "shutdown_with_requests.html"));
}

// The blob key corruption test runs in a separate class to avoid corrupting
// an IDB store that other tests use.
// This test is for https://crbug.com/1039446.
class IndexedDBBrowserTestBlobKeyCorruption : public IndexedDBBrowserTest {
 public:
  int64_t GetNextBlobNumber(const storage::BucketLocator& bucket_locator,
                            int64_t database_id) {
    int64_t number;

    base::RunLoop loop;
    auto control_test = GetControlTest();
    control_test->GetNextBlobNumberForTesting(
        bucket_locator, database_id,
        base::BindLambdaForTesting([&](int64_t next_blob_number) {
          number = next_blob_number;
          loop.Quit();
        }));
    loop.Run();
    return number;
  }

  base::FilePath PathForBlob(const storage::BucketLocator& bucket_locator,
                             int64_t database_id,
                             int64_t blob_number) {
    base::FilePath path;
    base::RunLoop loop;
    auto control_test = GetControlTest();
    control_test->GetPathForBlobForTesting(
        bucket_locator, database_id, blob_number,
        base::BindLambdaForTesting([&](const base::FilePath& blob_path) {
          path = blob_path;
          loop.Quit();
        }));
    loop.Run();
    return path;
  }
};

// Verify the blob key corruption state recovery:
// - Create a file that should be the 'first' blob file.
// - open a database that tries to write a blob.
// - verify the new blob key is correct.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestBlobKeyCorruption, LifecycleTest) {
  ASSERT_TRUE(embedded_test_server()->Started() ||
              embedded_test_server()->InitializeAndListen());
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &StaticFileRequestHandler, s_indexeddb_test_prefix, this));
  embedded_test_server()->StartAcceptingConnections();

  // Set up the IndexedDB instance so it contains our reference data.
  std::string test_file =
      std::string(s_indexeddb_test_prefix) + "write_and_read_blob.html";
  SimpleTest(embedded_test_server()->GetURL(test_file));

  // Find the bucket that was created.
  const auto maybe_bucket_info =
      GetOrCreateBucket(storage::BucketInitParams::ForDefaultBucket(
          blink::StorageKey::CreateFirstParty(
              url::Origin::Create(embedded_test_server()->base_url()))));
  ASSERT_TRUE(maybe_bucket_info.has_value());
  const auto bucket_locator = maybe_bucket_info->ToBucketLocator();
  int64_t next_blob_number = GetNextBlobNumber(bucket_locator, 1);

  base::FilePath first_blob =
      PathForBlob(bucket_locator, 1, next_blob_number - 1);
  base::FilePath corrupt_blob =
      PathForBlob(bucket_locator, 1, next_blob_number);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(first_blob));
    EXPECT_FALSE(base::PathExists(corrupt_blob));
    const char kCorruptData[] = "corrupt";
    base::WriteFile(corrupt_blob, kCorruptData);
  }

  SimpleTest(embedded_test_server()->GetURL(test_file));
}

IN_PROC_BROWSER_TEST_P(IndexedDBIncognitoTest, BucketDurabilityStrict) {
  FailOperation(FailClass::LEVELDB_TRANSACTION, FailMethod::COMMIT_SYNC, 2, 1);
  SimpleTest(GetTestUrl("indexeddb", "bucket_durability_strict.html"),
             IsIncognito());
}

IN_PROC_BROWSER_TEST_P(IndexedDBIncognitoTest, BucketDurabilityRelaxed) {
  FailOperation(FailClass::LEVELDB_TRANSACTION, FailMethod::COMMIT_SYNC, 2, 1);
  SimpleTest(GetTestUrl("indexeddb", "bucket_durability_relaxed.html"),
             IsIncognito());
}

IN_PROC_BROWSER_TEST_P(IndexedDBIncognitoTest, BucketDurabilityOverride) {
  FailOperation(FailClass::LEVELDB_TRANSACTION, FailMethod::COMMIT_SYNC, 2, 1);
  SimpleTest(GetTestUrl("indexeddb", "bucket_durability_override.html"),
             IsIncognito());
}

INSTANTIATE_TEST_SUITE_P(All, IndexedDBIncognitoTest, testing::Bool());

}  // namespace content
