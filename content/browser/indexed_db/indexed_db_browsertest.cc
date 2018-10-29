// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/thread_test_helper.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/mock_browsertest_indexed_db_class_factory.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::ASCIIToUTF16;
using storage::QuotaManager;
using storage::DatabaseUtil;
using url::Origin;

namespace content {

namespace {
const Origin kFileOrigin = Origin::Create(GURL("file:///"));
};

// This browser test is aimed towards exercising the IndexedDB bindings and
// the actual implementation that lives in the browser side.
class IndexedDBBrowserTest : public ContentBrowserTest,
                             public ::testing::WithParamInterface<const char*> {
 public:
  IndexedDBBrowserTest() = default;

  void SetUp() override {
    GetTestClassFactory()->Reset();
    IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(GetIDBClassFactory);
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(nullptr);
    ContentBrowserTest::TearDown();
  }

  void FailOperation(FailClass failure_class,
                     FailMethod failure_method,
                     int fail_on_instance_num,
                     int fail_on_call_num) {
    GetTestClassFactory()->FailOperation(
        failure_class, failure_method, fail_on_instance_num, fail_on_call_num);
  }

  void SimpleTest(const GURL& test_url, bool incognito = false) {
    // The test page will perform tests on IndexedDB, then navigate to either
    // a #pass or #fail ref.
    Shell* the_browser = incognito ? CreateOffTheRecordBrowser() : shell();

    VLOG(0) << "Navigating to URL and blocking.";
    NavigateToURLBlockUntilNavigationsComplete(the_browser, test_url, 2);
    VLOG(0) << "Navigation done.";
    std::string result =
        the_browser->web_contents()->GetLastCommittedURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ExecuteScriptAndExtractString(
          the_browser, "window.domAutomationController.send(getLog())",
          &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }

  void NavigateAndWaitForTitle(Shell* shell,
                               const char* filename,
                               const char* hash,
                               const char* expected_string) {
    GURL url = GetTestUrl("indexeddb", filename);
    if (hash)
      url = GURL(url.spec() + hash);

    base::string16 expected_title16(ASCIIToUTF16(expected_string));
    TitleWatcher title_watcher(shell->web_contents(), expected_title16);
    NavigateToURL(shell, url);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  IndexedDBContextImpl* GetContext(Shell* browser = nullptr) {
    if (!browser)
      browser = shell();
    StoragePartition* partition = BrowserContext::GetDefaultStoragePartition(
        browser->web_contents()->GetBrowserContext());
    return static_cast<IndexedDBContextImpl*>(partition->GetIndexedDBContext());
  }

  void SetQuota(int per_host_quota_kilobytes) {
    SetTempQuota(per_host_quota_kilobytes,
                 BrowserContext::GetDefaultStoragePartition(
                     shell()->web_contents()->GetBrowserContext())
                     ->GetQuotaManager());
  }

  static void SetTempQuota(int per_host_quota_kilobytes,
                           scoped_refptr<QuotaManager> qm) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&IndexedDBBrowserTest::SetTempQuota,
                         per_host_quota_kilobytes, qm));
      return;
    }
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    const int KB = 1024;
    qm->SetQuotaSettings(
        storage::GetHardCodedSettings(per_host_quota_kilobytes * KB));
  }

  void DeleteForOrigin(const Origin& origin, Shell* browser = nullptr) {
    base::RunLoop loop;
    IndexedDBContextImpl* context = GetContext();
    context->TaskRunner()->PostTask(FROM_HERE,
                                    base::BindLambdaForTesting([&]() {
                                      context->DeleteForOrigin(kFileOrigin);
                                      loop.Quit();
                                    }));
    loop.Run();
  }

  int64_t RequestUsage(const Origin& origin, Shell* browser = nullptr) {
    base::RunLoop loop;
    int64_t size;
    IndexedDBContextImpl* context = GetContext(browser);
    context->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          size = context->GetOriginDiskUsage(origin);
          loop.Quit();
        }));
    loop.Run();
    return size;
  }

  int RequestBlobFileCount(const Origin& origin) {
    base::RunLoop loop;
    int count;
    IndexedDBContextImpl* context = GetContext();
    context->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          count = context->GetOriginBlobFileCount(origin);
          loop.Quit();
        }));
    loop.Run();
    return count;
  }

  bool RequestSchemaDowngrade(const Origin& origin) {
    base::RunLoop loop;
    bool downgraded;
    IndexedDBContextImpl* context = GetContext();
    context->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          downgraded = context->ForceSchemaDowngrade(origin);
          loop.Quit();
        }));
    loop.Run();
    return downgraded;
  }

  V2SchemaCorruptionStatus RequestHasV2SchemaCorruption(Origin origin) {
    base::RunLoop loop;
    V2SchemaCorruptionStatus status;
    IndexedDBContextImpl* context = GetContext();
    context->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          status = context->HasV2SchemaCorruption(origin);
          loop.Quit();
        }));
    loop.Run();
    return status;
  }

 protected:
  static MockBrowserTestIndexedDBClassFactory* GetTestClassFactory() {
    static ::base::LazyInstance<MockBrowserTestIndexedDBClassFactory>::Leaky
        s_factory = LAZY_INSTANCE_INITIALIZER;
    return s_factory.Pointer();
  }

  static IndexedDBClassFactory* GetIDBClassFactory() {
    return GetTestClassFactory();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBBrowserTest);
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTest) {
  SimpleTest(GetTestUrl("indexeddb", "cursor_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTestIncognito) {
  SimpleTest(GetTestUrl("indexeddb", "cursor_test.html"),
             true /* incognito */);
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

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, GetAllMaxMessageSize) {
  SimpleTest(GetTestUrl("indexeddb", "getall_max_message_size.html"));
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

class IndexedDBBrowserTestWithLowQuota : public IndexedDBBrowserTest {
 public:
  IndexedDBBrowserTestWithLowQuota() {}

  void SetUpOnMainThread() override {
    const int kInitialQuotaKilobytes = 5000;
    SetQuota(kInitialQuotaKilobytes);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBBrowserTestWithLowQuota);
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithLowQuota, QuotaTest) {
  SimpleTest(GetTestUrl("indexeddb", "quota_test.html"));
}

class IndexedDBBrowserTestWithGCExposed : public IndexedDBBrowserTest {
 public:
  IndexedDBBrowserTestWithGCExposed() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBBrowserTestWithGCExposed);
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithGCExposed,
                       DatabaseCallbacksTest) {
  SimpleTest(GetTestUrl("indexeddb", "database_callbacks_first.html"));
}

static void CopyLevelDBToProfile(Shell* shell,
                                 scoped_refptr<IndexedDBContextImpl> context,
                                 const std::string& test_directory) {
  DCHECK(context->TaskRunner()->RunsTasksInCurrentSequence());
  base::FilePath leveldb_dir(FILE_PATH_LITERAL("file__0.indexeddb.leveldb"));
  base::FilePath test_data_dir =
      GetTestFilePath("indexeddb", test_directory.c_str()).Append(leveldb_dir);
  base::FilePath dest = context->data_path().Append(leveldb_dir);
  // If we don't create the destination directory first, the contents of the
  // leveldb directory are copied directly into profile/IndexedDB instead of
  // profile/IndexedDB/file__0.xxx/
  ASSERT_TRUE(base::CreateDirectory(dest));
  const bool kRecursive = true;
  ASSERT_TRUE(base::CopyDirectory(test_data_dir,
                                  context->data_path(),
                                  kRecursive));
}

class IndexedDBBrowserTestWithPreexistingLevelDB : public IndexedDBBrowserTest {
 public:
  IndexedDBBrowserTestWithPreexistingLevelDB() {}
  void SetUpOnMainThread() override {
    scoped_refptr<IndexedDBContextImpl> context = GetContext();
    context->TaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&CopyLevelDBToProfile, shell(), context,
                                  EnclosingLevelDBDir()));
    scoped_refptr<base::ThreadTestHelper> helper(
        new base::ThreadTestHelper(GetContext()->TaskRunner()));
    ASSERT_TRUE(helper->Run());
  }

  virtual std::string EnclosingLevelDBDir() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBBrowserTestWithPreexistingLevelDB);
};

class IndexedDBBrowserTestWithVersion0Schema : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "migration_from_0"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion0Schema, MigrationTest) {
  SimpleTest(GetTestUrl("indexeddb", "migration_test.html"));
}

class IndexedDBBrowserTestWithVersion123456Schema : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "schema_version_123456"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion123456Schema,
                       DestroyTest) {
  int64_t original_size = RequestUsage(kFileOrigin);
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
  int64_t new_size = RequestUsage(kFileOrigin);
  EXPECT_GT(new_size, 0);
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithVersion987654SSVData : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "ssv_version_987654"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion987654SSVData,
                       DestroyTest) {
  int64_t original_size = RequestUsage(kFileOrigin);
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
  int64_t new_size = RequestUsage(kFileOrigin);
  EXPECT_GT(new_size, 0);
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithCorruptLevelDB : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "corrupt_leveldb"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithCorruptLevelDB,
                       DestroyTest) {
  int64_t original_size = RequestUsage(kFileOrigin);
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
  int64_t new_size = RequestUsage(kFileOrigin);
  EXPECT_GT(new_size, 0);
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithMissingSSTFile : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  std::string EnclosingLevelDBDir() override { return "missing_sst"; }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithMissingSSTFile,
                       DestroyTest) {
  int64_t original_size = RequestUsage(kFileOrigin);
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_missing_table.html"));
  int64_t new_size = RequestUsage(kFileOrigin);
  EXPECT_GT(new_size, 0);
  EXPECT_NE(original_size, new_size);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, LevelDBLogFileTest) {
  // Any page that opens an IndexedDB will work here.
  SimpleTest(GetTestUrl("indexeddb", "database_test.html"));
  base::FilePath leveldb_dir(FILE_PATH_LITERAL("file__0.indexeddb.leveldb"));
  base::FilePath log_file(FILE_PATH_LITERAL("LOG"));
  base::FilePath log_file_path =
      GetContext()->data_path().Append(leveldb_dir).Append(log_file);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int64_t size;
    EXPECT_TRUE(base::GetFileSize(log_file_path, &size));
    EXPECT_GT(size, 0);
  }
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CanDeleteWhenOverQuotaTest) {
  SimpleTest(GetTestUrl("indexeddb", "fill_up_5k.html"));
  int64_t size = RequestUsage(kFileOrigin);
  const int kQuotaKilobytes = 2;
  EXPECT_GT(size, kQuotaKilobytes * 1024);
  SetQuota(kQuotaKilobytes);
  SimpleTest(GetTestUrl("indexeddb", "delete_over_quota.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, EmptyBlob) {
  // First delete all IDB's for the test origin
  DeleteForOrigin(kFileOrigin);
  EXPECT_EQ(0, RequestBlobFileCount(kFileOrigin));  // Start with no blob files.
  const GURL test_url = GetTestUrl("indexeddb", "empty_blob.html");
  // For some reason Android's futimes fails (EPERM) in this test. Do not assert
  // file times on Android, but do so on other platforms. crbug.com/467247
  // TODO(cmumford): Figure out why this is the case and fix if possible.
#if defined(OS_ANDROID)
  SimpleTest(GURL(test_url.spec() + "#ignoreTimes"));
#else
  SimpleTest(GURL(test_url.spec()));
#endif
  // Test stores one blob and one file to disk, so expect two files.
  EXPECT_EQ(2, RequestBlobFileCount(kFileOrigin));
}

// Very flaky on many bots. See crbug.com/459835
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithGCExposed, DISABLED_BlobDidAck) {
  SimpleTest(GetTestUrl("indexeddb", "blob_did_ack.html"));
  // Wait for idle so that the blob ack has time to be received/processed by
  // the browser process.
  scoped_refptr<base::ThreadTestHelper> helper =
      base::MakeRefCounted<base::ThreadTestHelper>(GetContext()->TaskRunner());
  ASSERT_TRUE(helper->Run());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(helper->Run());
  content::ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(
          shell()->web_contents()->GetBrowserContext());
  EXPECT_EQ(0UL, blob_context->context()->blob_count());
}

// Flaky. See crbug.com/459835.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithGCExposed,
                       DISABLED_BlobDidAckPrefetch) {
  SimpleTest(GetTestUrl("indexeddb", "blob_did_ack_prefetch.html"));
  // Wait for idle so that the blob ack has time to be received/processed by
  // the browser process.
  base::RunLoop().RunUntilIdle();
  content::ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(
          shell()->web_contents()->GetBrowserContext());
  EXPECT_EQ(0UL, blob_context->context()->blob_count());
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, BlobsCountAgainstQuota) {
  SimpleTest(GetTestUrl("indexeddb", "blobs_use_quota.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DeleteForOriginDeletesBlobs) {
  SimpleTest(GetTestUrl("indexeddb", "write_4mb_blob.html"));
  int64_t size = RequestUsage(kFileOrigin);
  // This assertion assumes that we do not compress blobs.
  EXPECT_GT(size, 4 << 20 /* 4 MB */);
  DeleteForOrigin(kFileOrigin);
  EXPECT_EQ(0, RequestUsage(kFileOrigin));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DeleteForOriginIncognito) {
  const GURL test_url = GetTestUrl("indexeddb", "fill_up_5k.html");
  const Origin origin = Origin::Create(test_url);

  Shell* browser = CreateOffTheRecordBrowser();
  NavigateToURLBlockUntilNavigationsComplete(browser, test_url, 2);

  EXPECT_GT(RequestUsage(origin, browser), 5 * 1024);

  IndexedDBContextImpl* context = GetContext(browser);
  base::RunLoop loop;
  context->TaskRunner()->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    context->DeleteForOrigin(origin);
                                    loop.Quit();
                                  }));
  loop.Run();

  EXPECT_EQ(0, RequestUsage(origin, browser));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DiskFullOnCommit) {
  // Ignore several preceding transactions:
  // * The test calls deleteDatabase() which opens the backing store:
  //   #1: IndexedDBBackingStore::OpenBackingStore
  //       => IndexedDBBackingStore::SetUpMetadata
  //   #2: IndexedDBBackingStore::OpenBackingStore
  //       => IndexedDBBackingStore::CleanUpBlobJournal (no-op)
  // * The test calls open(), to create a new database:
  //   #3: IndexedDBFactoryImpl::Open
  //       => IndexedDBDatabase::Create
  //       => IndexedDBBackingStore::CreateIDBDatabaseMetaData
  //   #4: IndexedDBTransaction::Commit - initial "versionchange" transaction
  // * Once the connection is opened, the test runs:
  //   #5: IndexedDBTransaction::Commit - the test's "readwrite" transaction)
  const int instance_num = 5;
  const int call_num = 1;
  FailOperation(FAIL_CLASS_LEVELDB_TRANSACTION, FAIL_METHOD_COMMIT_DISK_FULL,
                instance_num, call_num);
  SimpleTest(GetTestUrl("indexeddb", "disk_full_on_commit.html"));
}

namespace {

std::unique_ptr<net::test_server::HttpResponse> ServePath(
    std::string request_path) {
  base::FilePath resource_path =
      content::GetTestFilePath("indexeddb", request_path.c_str());
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);

  std::string file_contents;
  if (!base::ReadFileToString(resource_path, &file_contents))
    NOTREACHED() << "could not read file " << resource_path;
  http_response->set_content(file_contents);
  return std::move(http_response);
}

void CompactIndexedDBBackingStore(scoped_refptr<IndexedDBContextImpl> context,
                                  const Origin& origin) {
  IndexedDBFactory* factory = context->GetIDBFactory();

  std::pair<IndexedDBFactory::OriginDBMapIterator,
            IndexedDBFactory::OriginDBMapIterator>
      range = factory->GetOpenDatabasesForOrigin(origin);

  if (range.first == range.second)  // If no open db's for this origin
    return;

  // Compact the first db's backing store since all the db's are in the same
  // backing store.
  IndexedDBDatabase* db = range.first->second;
  IndexedDBBackingStore* backing_store = db->backing_store();
  backing_store->Compact();
}

void CorruptIndexedDBDatabase(IndexedDBContextImpl* context,
                              const Origin& origin,
                              base::WaitableEvent* signal_when_finished) {
  CompactIndexedDBBackingStore(context, origin);

  int num_files = 0;
  int num_errors = 0;
  const bool recursive = false;
  for (const base::FilePath& idb_data_path : context->GetStoragePaths(origin)) {
    base::FileEnumerator enumerator(
        idb_data_path, recursive, base::FileEnumerator::FILES);
    for (base::FilePath idb_file = enumerator.Next(); !idb_file.empty();
         idb_file = enumerator.Next()) {
      int64_t size(0);
      GetFileSize(idb_file, &size);

      if (idb_file.Extension() == FILE_PATH_LITERAL(".ldb")) {
        num_files++;
        base::File file(
            idb_file, base::File::FLAG_WRITE | base::File::FLAG_OPEN_TRUNCATED);
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

  signal_when_finished->Signal();
}

const char s_corrupt_db_test_prefix[] = "/corrupt/test/";

std::unique_ptr<net::test_server::HttpResponse> CorruptDBRequestHandler(
    IndexedDBContextImpl* context,
    const Origin& origin,
    const std::string& path,
    IndexedDBBrowserTest* test,
    const net::test_server::HttpRequest& request) {
  std::string request_path;
  if (path.find(s_corrupt_db_test_prefix) != std::string::npos)
    request_path = request.relative_url.substr(
        std::string(s_corrupt_db_test_prefix).size());
  else
    return std::unique_ptr<net::test_server::HttpResponse>();

  // Remove the query string if present.
  std::string request_query;
  size_t query_pos = request_path.find('?');
  if (query_pos != std::string::npos) {
    request_query = request_path.substr(query_pos + 1);
    request_path = request_path.substr(0, query_pos);
  }

  if (request_path == "corruptdb" && !request_query.empty()) {
    VLOG(0) << "Requested to corrupt IndexedDB: " << request_query;
    base::WaitableEvent signal_when_finished(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    context->TaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CorruptIndexedDBDatabase, base::ConstRef(context),
                       origin, &signal_when_finished));
    signal_when_finished.Wait();

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    return std::move(http_response);
  } else if (request_path == "fail" && !request_query.empty()) {
    FailClass failure_class = FAIL_CLASS_NOTHING;
    FailMethod failure_method = FAIL_METHOD_NOTHING;
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

      std::string key = net::UnescapeURLComponent(
          escaped_key,
          net::UnescapeRule::NORMAL | net::UnescapeRule::SPACES |
              net::UnescapeRule::PATH_SEPARATORS |
              net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

      std::string value = net::UnescapeURLComponent(
          escaped_value,
          net::UnescapeRule::NORMAL | net::UnescapeRule::SPACES |
              net::UnescapeRule::PATH_SEPARATORS |
              net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

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
      failure_class = FAIL_CLASS_LEVELDB_TRANSACTION;
      if (fail_method == "Get")
        failure_method = FAIL_METHOD_GET;
      else if (fail_method == "Commit")
        failure_method = FAIL_METHOD_COMMIT;
      else
        NOTREACHED() << "Unknown method: \"" << fail_method << "\"";
    } else if (fail_class == "LevelDBIterator") {
      failure_class = FAIL_CLASS_LEVELDB_ITERATOR;
      if (fail_method == "Seek")
        failure_method = FAIL_METHOD_SEEK;
      else
        NOTREACHED() << "Unknown method: \"" << fail_method << "\"";
    } else {
      NOTREACHED() << "Unknown class: \"" << fail_class << "\"";
    }

    DCHECK_GE(instance_num, 1);
    DCHECK_GE(call_num, 1);

    test->FailOperation(failure_class, failure_method, instance_num, call_num);

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    return std::move(http_response);
  }

  return ServePath(request_path);
}

const char s_indexeddb_test_prefix[] = "/indexeddb/test/";

std::unique_ptr<net::test_server::HttpResponse> StaticFileRequestHandler(
    const std::string& path,
    IndexedDBBrowserTest* test,
    const net::test_server::HttpRequest& request) {
  if (path.find(s_indexeddb_test_prefix) == std::string::npos)
    return std::unique_ptr<net::test_server::HttpResponse>();
  std::string request_path =
      request.relative_url.substr(std::string(s_indexeddb_test_prefix).size());
  return ServePath(request_path);
}

}  // namespace

IN_PROC_BROWSER_TEST_P(IndexedDBBrowserTest, OperationOnCorruptedOpenDatabase) {
  ASSERT_TRUE(embedded_test_server()->Started() ||
              embedded_test_server()->InitializeAndListen());
  const Origin origin = Origin::Create(embedded_test_server()->base_url());
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&CorruptDBRequestHandler, base::Unretained(GetContext()),
                 origin, s_corrupt_db_test_prefix, this));
  embedded_test_server()->StartAcceptingConnections();

  std::string test_file = std::string(s_corrupt_db_test_prefix) +
                          "corrupted_open_db_detection.html#" + GetParam();
  SimpleTest(embedded_test_server()->GetURL(test_file));

  test_file =
      std::string(s_corrupt_db_test_prefix) + "corrupted_open_db_recovery.html";
  SimpleTest(embedded_test_server()->GetURL(test_file));
}

INSTANTIATE_TEST_CASE_P(IndexedDBBrowserTestInstantiation,
                        IndexedDBBrowserTest,
                        ::testing::Values("failGetBlobJournal",
                                          "get",
                                          "getAll",
                                          "iterate",
                                          "failTransactionCommit",
                                          "clearObjectStore"));

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DeleteCompactsBackingStore) {
  const GURL test_url = GetTestUrl("indexeddb", "delete_compact.html");
  SimpleTest(GURL(test_url.spec() + "#fill"));
  int64_t after_filling = RequestUsage(kFileOrigin);
  EXPECT_GT(after_filling, 0);

  SimpleTest(GURL(test_url.spec() + "#purge"));
  int64_t after_deleting = RequestUsage(kFileOrigin);
  EXPECT_LT(after_deleting, after_filling);

  // The above tests verify basic assertions - that filling writes data and
  // deleting reduces the amount stored.

  // The below tests make assumptions about implementation specifics, such as
  // data compression, compaction efficiency, and the maximum amount of
  // metadata and log data remains after a deletion. It is possible that
  // changes to the implementation may require these constants to be tweaked.

  const int kTestFillBytes = 1024 * 1024 * 5;  // 5MB
  EXPECT_GT(after_filling, kTestFillBytes);

  const int kTestCompactBytes = 1024 * 10;  // 10kB
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
#if defined(OS_WIN)
#define MAYBE_VersionChangeCrashResilience DISABLED_VersionChangeCrashResilience
#else
#define MAYBE_VersionChangeCrashResilience VersionChangeCrashResilience
#endif
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest,
                       MAYBE_VersionChangeCrashResilience) {
  NavigateAndWaitForTitle(shell(), "version_change_crash.html", "#part3",
                          "pass - part3 - rolled back");
}

// crbug.com/427529
// Disable this test for ASAN on Android because it takes too long to run.
#if defined(ANDROID) && defined(ADDRESS_SANITIZER)
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
  NavigateToURL(new_shell, GURL(url::kAboutBlankURL));
  NavigateAndWaitForTitle(new_shell, "version_change_blocked.html", "#tab2",
                          "setVersion(3) blocked");

  base::string16 expected_title16(ASCIIToUTF16("setVersion(3) complete"));
  TitleWatcher title_watcher(new_shell->web_contents(), expected_title16);

  shell()->web_contents()->GetMainFrame()->GetProcess()->Shutdown(0);
  shell()->Close();

  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

// Verify that a "close" event is fired at database connections when
// the backing store is deleted.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ForceCloseEventTest) {
  NavigateAndWaitForTitle(shell(), "force_close_event.html", nullptr,
                          "connection ready");
  DeleteForOrigin(kFileOrigin);
  base::string16 expected_title16(ASCIIToUTF16("connection closed"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
  title_watcher.AlsoWaitForTitle(ASCIIToUTF16("connection closed with error"));
  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

// The V2 schema corruption test runs in a separate class to avoid corrupting
// an IDB store that other tests use.
class IndexedDBBrowserTestV2SchemaCorruption : public IndexedDBBrowserTest {
 public:
  void SetUp() override {
    GetTestClassFactory()->Reset();
    IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(GetIDBClassFactory);
    ContentBrowserTest::SetUp();
  }
};

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
  const Origin origin = Origin::Create(embedded_test_server()->base_url());
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &StaticFileRequestHandler, s_indexeddb_test_prefix, this));
  embedded_test_server()->StartAcceptingConnections();

  // Set up the IndexedDB instance so it contains our reference data.
  std::string test_file =
      std::string(s_indexeddb_test_prefix) + "v2schemacorrupt_setup.html";
  SimpleTest(embedded_test_server()->GetURL(test_file));

  // Verify the backing store does not have corruption.
  V2SchemaCorruptionStatus has_corruption =
      RequestHasV2SchemaCorruption(origin);
  ASSERT_EQ(has_corruption, V2SchemaCorruptionStatus::kNo);

  // Revert schema to v2.  This closes the targeted backing store.
  bool schema_downgrade = RequestSchemaDowngrade(origin);
  ASSERT_EQ(schema_downgrade, true);

  // Re-open the backing store and verify it has corruption.
  test_file =
      std::string(s_indexeddb_test_prefix) + "v2schemacorrupt_reopen.html";
  SimpleTest(embedded_test_server()->GetURL(test_file));
  has_corruption = RequestHasV2SchemaCorruption(origin);
  ASSERT_EQ(has_corruption, V2SchemaCorruptionStatus::kYes);

  // Verify that the saved blob is get-able with a v2 backing store.
  test_file =
      std::string(s_indexeddb_test_prefix) + "v2schemacorrupt_verify.html";
  SimpleTest(embedded_test_server()->GetURL(test_file));
}

class IndexedDBBrowserTestSingleProcess : public IndexedDBBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }
};

// https://crbug.com/788788
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_RenderThreadShutdownTest DISABLED_RenderThreadShutdownTest
#else
#define MAYBE_RenderThreadShutdownTest RenderThreadShutdownTest
#endif  // defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestSingleProcess,
                       MAYBE_RenderThreadShutdownTest) {
  SimpleTest(GetTestUrl("indexeddb", "shutdown_with_requests.html"));
}

}  // namespace content
