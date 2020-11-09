// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/local_storage_database.pb.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/generated_code_cache_settings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/cookie_manager.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/test/mock_quota_client.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/leveldatabase/env_chromium.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "ppapi/shared_impl/ppapi_constants.h"  // nogncheck
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/origin.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

using net::CanonicalCookie;
using CookieDeletionFilter = network::mojom::CookieDeletionFilter;
using CookieDeletionFilterPtr = network::mojom::CookieDeletionFilterPtr;

namespace content {
namespace {

const int kDefaultClientId = 42;
const char kCacheKey[] = "key";
const char kCacheValue[] = "cached value";

#if BUILDFLAG(ENABLE_PLUGINS)
const char kWidevineCdmPluginId[] = "application_x-ppapi-widevine-cdm";
const char kClearKeyCdmPluginId[] = "application_x-ppapi-clearkey-cdm";
#endif  // BUILDFLAG(ENABLE_PLUGINS)

const blink::mojom::StorageType kTemporary =
    blink::mojom::StorageType::kTemporary;
const blink::mojom::StorageType kPersistent =
    blink::mojom::StorageType::kPersistent;

const storage::QuotaClientType kClientFile =
    storage::QuotaClientType::kFileSystem;

const uint32_t kAllQuotaRemoveMask =
    StoragePartition::REMOVE_DATA_MASK_APPCACHE |
    StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
    StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
    StoragePartition::REMOVE_DATA_MASK_WEBSQL;

class AwaitCompletionHelper {
 public:
  AwaitCompletionHelper() : start_(false), already_quit_(false) {}
  virtual ~AwaitCompletionHelper() = default;

  void BlockUntilNotified() {
    if (!already_quit_) {
      DCHECK(!start_);
      start_ = true;
      base::RunLoop().Run();
    } else {
      DCHECK(!start_);
      already_quit_ = false;
    }
  }

  void Notify() {
    if (start_) {
      DCHECK(!already_quit_);
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
      start_ = false;
    } else {
      DCHECK(!already_quit_);
      already_quit_ = true;
    }
  }

 private:
  // Helps prevent from running message_loop, if the callback invoked
  // immediately.
  bool start_;
  bool already_quit_;

  DISALLOW_COPY_AND_ASSIGN(AwaitCompletionHelper);
};

class RemoveCookieTester {
 public:
  explicit RemoveCookieTester(StoragePartition* storage_partition)
      : get_cookie_success_(false), storage_partition_(storage_partition) {}

  // Returns true, if the given cookie exists in the cookie store.
  bool ContainsCookie(const url::Origin& origin) {
    get_cookie_success_ = false;
    storage_partition_->GetCookieManagerForBrowserProcess()->GetCookieList(
        origin.GetURL(), net::CookieOptions::MakeAllInclusive(),
        base::BindOnce(&RemoveCookieTester::GetCookieListCallback,
                       base::Unretained(this)));
    await_completion_.BlockUntilNotified();
    return get_cookie_success_;
  }

  void AddCookie(const url::Origin& origin) {
    net::CookieInclusionStatus status;
    std::unique_ptr<net::CanonicalCookie> cc(
        net::CanonicalCookie::Create(origin.GetURL(), "A=1", base::Time::Now(),
                                     base::nullopt /* server_time */, &status));
    storage_partition_->GetCookieManagerForBrowserProcess()->SetCanonicalCookie(
        *cc, origin.GetURL(), net::CookieOptions::MakeAllInclusive(),
        base::BindOnce(&RemoveCookieTester::SetCookieCallback,
                       base::Unretained(this)));
    await_completion_.BlockUntilNotified();
  }

 private:
  void GetCookieListCallback(
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies) {
    std::string cookie_line =
        net::CanonicalCookie::BuildCookieLine(cookie_list);
    if (cookie_line == "A=1") {
      get_cookie_success_ = true;
    } else {
      EXPECT_EQ("", cookie_line);
      get_cookie_success_ = false;
    }
    await_completion_.Notify();
  }

  void SetCookieCallback(net::CookieAccessResult result) {
    ASSERT_TRUE(result.status.IsInclude());
    await_completion_.Notify();
  }

  bool get_cookie_success_;
  AwaitCompletionHelper await_completion_;
  StoragePartition* storage_partition_;

  DISALLOW_COPY_AND_ASSIGN(RemoveCookieTester);
};

class RemoveLocalStorageTester {
 public:
  RemoveLocalStorageTester(content::BrowserTaskEnvironment* task_environment,
                           TestBrowserContext* browser_context)
      : task_environment_(task_environment),
        storage_partition_(
            BrowserContext::GetDefaultStoragePartition(browser_context)),
        dom_storage_context_(storage_partition_->GetDOMStorageContext()) {}

  ~RemoveLocalStorageTester() {
    // Tests which bring up a real Local Storage context need to shut it down
    // and wait for the database to be closed before terminating; otherwise the
    // TestBrowserContext may fail to delete its temp dir, and it will not be
    // happy about that.
    static_cast<DOMStorageContextWrapper*>(dom_storage_context_)->Shutdown();
    task_environment_->RunUntilIdle();
  }

  // Returns true, if the given origin URL exists.
  bool DOMStorageExistsForOrigin(const url::Origin& origin) {
    GetLocalStorageUsage();
    await_completion_.BlockUntilNotified();
    for (size_t i = 0; i < infos_.size(); ++i) {
      if (origin == infos_[i].origin)
        return true;
    }
    return false;
  }

  void AddDOMStorageTestData(const url::Origin& origin1,
                             const url::Origin& origin2,
                             const url::Origin& origin3) {
    // NOTE: Tests which call this method depend on implementation details of
    // how exactly the Local Storage subsystem stores persistent data.

    base::RunLoop open_loop;
    leveldb_env::Options options;
    options.create_if_missing = true;
    auto database = storage::AsyncDomStorageDatabase::OpenDirectory(
        std::move(options),
        storage_partition_->GetPath().Append(storage::kLocalStoragePath),
        storage::kLocalStorageLeveldbName, base::nullopt,
        base::ThreadTaskRunnerHandle::Get(),
        base::BindLambdaForTesting([&](leveldb::Status status) {
          ASSERT_TRUE(status.ok());
          open_loop.Quit();
        }));
    open_loop.Run();

    base::RunLoop populate_loop;
    database->database().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const storage::DomStorageDatabase& db) {
          PopulateDatabase(db, origin1, origin2, origin3);
          populate_loop.Quit();
        }));
    populate_loop.Run();

    // Ensure that this database is fully closed before returning.
    database.reset();
    task_environment_->RunUntilIdle();

    EXPECT_TRUE(DOMStorageExistsForOrigin(origin1));
    EXPECT_TRUE(DOMStorageExistsForOrigin(origin2));
    EXPECT_TRUE(DOMStorageExistsForOrigin(origin3));
  }

  static void PopulateDatabase(const storage::DomStorageDatabase& db,
                               const url::Origin& origin1,
                               const url::Origin& origin2,
                               const url::Origin& origin3) {
    storage::LocalStorageOriginMetaData data;
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> entries;

    base::Time now = base::Time::Now();
    data.set_last_modified(now.ToInternalValue());
    data.set_size_bytes(16);
    ASSERT_TRUE(
        db.Put(CreateMetaDataKey(origin1),
               base::as_bytes(base::make_span(data.SerializeAsString())))
            .ok());
    ASSERT_TRUE(db.Put(CreateDataKey(origin1), {}).ok());

    base::Time one_day_ago = now - base::TimeDelta::FromDays(1);
    data.set_last_modified(one_day_ago.ToInternalValue());
    ASSERT_TRUE(
        db.Put(CreateMetaDataKey(origin2),
               base::as_bytes(base::make_span((data.SerializeAsString()))))
            .ok());
    ASSERT_TRUE(db.Put(CreateDataKey(origin2), {}).ok());

    base::Time sixty_days_ago = now - base::TimeDelta::FromDays(60);
    data.set_last_modified(sixty_days_ago.ToInternalValue());
    ASSERT_TRUE(
        db.Put(CreateMetaDataKey(origin3),
               base::as_bytes(base::make_span(data.SerializeAsString())))
            .ok());
    ASSERT_TRUE(db.Put(CreateDataKey(origin3), {}).ok());
  }

 private:
  static std::vector<uint8_t> CreateDataKey(const url::Origin& origin) {
    auto origin_str = origin.Serialize();
    std::vector<uint8_t> serialized_origin(origin_str.begin(),
                                           origin_str.end());
    std::vector<uint8_t> key = {'_'};
    key.insert(key.end(), serialized_origin.begin(), serialized_origin.end());
    key.push_back(0);
    key.push_back('X');
    return key;
  }

  static std::vector<uint8_t> CreateMetaDataKey(const url::Origin& origin) {
    const uint8_t kMetaPrefix[] = {'M', 'E', 'T', 'A', ':'};
    auto origin_str = origin.Serialize();
    std::vector<uint8_t> serialized_origin(origin_str.begin(),
                                           origin_str.end());
    std::vector<uint8_t> key;
    key.reserve(base::size(kMetaPrefix) + serialized_origin.size());
    key.insert(key.end(), kMetaPrefix, kMetaPrefix + base::size(kMetaPrefix));
    key.insert(key.end(), serialized_origin.begin(), serialized_origin.end());
    return key;
  }

  void GetLocalStorageUsage() {
    dom_storage_context_->GetLocalStorageUsage(
        base::BindOnce(&RemoveLocalStorageTester::OnGotLocalStorageUsage,
                       base::Unretained(this)));
  }

  void OnGotLocalStorageUsage(
      const std::vector<content::StorageUsageInfo>& infos) {
    infos_ = infos;
    await_completion_.Notify();
  }

  // We don't own these pointers.
  BrowserTaskEnvironment* const task_environment_;
  StoragePartition* const storage_partition_;
  DOMStorageContext* dom_storage_context_;

  std::vector<content::StorageUsageInfo> infos_;

  AwaitCompletionHelper await_completion_;

  DISALLOW_COPY_AND_ASSIGN(RemoveLocalStorageTester);
};

class RemoveCodeCacheTester {
 public:
  explicit RemoveCodeCacheTester(GeneratedCodeCacheContext* code_cache_context)
      : code_cache_context_(code_cache_context) {}

  enum Cache { kJs, kWebAssembly };

  bool ContainsEntry(Cache cache, GURL url, GURL origin_lock) {
    entry_exists_ = false;
    GeneratedCodeCache::ReadDataCallback callback = base::BindRepeating(
        &RemoveCodeCacheTester::FetchEntryCallback, base::Unretained(this));
    GetCache(cache)->FetchEntry(url, origin_lock, callback);
    await_completion_.BlockUntilNotified();
    return entry_exists_;
  }

  void AddEntry(Cache cache,
                GURL url,
                GURL origin_lock,
                const std::string& data) {
    std::vector<uint8_t> data_vector(data.begin(), data.end());
    GetCache(cache)->WriteEntry(url, origin_lock, base::Time::Now(),
                                data_vector);
    base::RunLoop().RunUntilIdle();
  }

  void SetLastUseTime(Cache cache,
                      GURL url,
                      GURL origin_lock,
                      base::Time time) {
    GetCache(cache)->SetLastUsedTimeForTest(
        url, origin_lock, time,
        base::BindRepeating(&RemoveCodeCacheTester::SetTimeCallback,
                            base::Unretained(this)));
    await_completion_.BlockUntilNotified();
  }

  std::string received_data() { return received_data_; }

 private:
  GeneratedCodeCache* GetCache(Cache cache) {
    if (cache == kJs)
      return code_cache_context_->generated_js_code_cache();
    else
      return code_cache_context_->generated_wasm_code_cache();
  }

  void FetchEntryCallback(const base::Time& response_time,
                          mojo_base::BigBuffer data) {
    if (!response_time.is_null()) {
      entry_exists_ = true;
      received_data_ = std::string(data.data(), data.data() + data.size());
    } else {
      entry_exists_ = false;
    }
    await_completion_.Notify();
  }

  void SetTimeCallback() { await_completion_.Notify(); }

  bool entry_exists_;
  AwaitCompletionHelper await_completion_;
  GeneratedCodeCacheContext* code_cache_context_;
  std::string received_data_;
  DISALLOW_COPY_AND_ASSIGN(RemoveCodeCacheTester);
};

#if BUILDFLAG(ENABLE_PLUGINS)
class RemovePluginPrivateDataTester {
 public:
  explicit RemovePluginPrivateDataTester(
      storage::FileSystemContext* filesystem_context)
      : filesystem_context_(filesystem_context) {}

  // Add some files to the PluginPrivateFileSystem. They are created as follows:
  //   url1 - ClearKey - 1 file - timestamp 10 days ago
  //   url2 - Widevine - 2 files - timestamps now and 60 days ago
  void AddPluginPrivateTestData(const GURL& url1, const GURL& url2) {
    base::Time now = base::Time::Now();
    base::Time ten_days_ago = now - base::TimeDelta::FromDays(10);
    base::Time sixty_days_ago = now - base::TimeDelta::FromDays(60);

    // Create a PluginPrivateFileSystem for ClearKey and add a single file
    // with a timestamp of 1 day ago.
    std::string clearkey_fsid = CreateFileSystem(kClearKeyCdmPluginId, url1);
    clearkey_file_ = CreateFile(url1, clearkey_fsid, "foo");
    SetFileTimestamp(clearkey_file_, ten_days_ago);

    // Create a second PluginPrivateFileSystem for Widevine and add two files
    // with different times.
    std::string widevine_fsid = CreateFileSystem(kWidevineCdmPluginId, url2);
    storage::FileSystemURL widevine_file1 =
        CreateFile(url2, widevine_fsid, "bar1");
    storage::FileSystemURL widevine_file2 =
        CreateFile(url2, widevine_fsid, "bar2");
    SetFileTimestamp(widevine_file1, now);
    SetFileTimestamp(widevine_file2, sixty_days_ago);
  }

  void DeleteClearKeyTestData() { DeleteFile(clearkey_file_); }

  // Returns true, if the given origin exists in a PluginPrivateFileSystem.
  bool DataExistsForOrigin(const url::Origin& origin) {
    AwaitCompletionHelper await_completion;
    bool data_exists_for_origin = false;
    filesystem_context_->default_file_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RemovePluginPrivateDataTester::
                           CheckIfDataExistsForOriginOnFileTaskRunner,
                       base::Unretained(this), origin, &data_exists_for_origin,
                       &await_completion));
    await_completion.BlockUntilNotified();
    return data_exists_for_origin;
  }

 private:
  // Creates a PluginPrivateFileSystem for the |plugin_name| and |origin|
  // provided. Returns the file system ID for the created
  // PluginPrivateFileSystem.
  std::string CreateFileSystem(const std::string& plugin_name,
                               const GURL& origin) {
    AwaitCompletionHelper await_completion;
    std::string fsid = storage::IsolatedContext::GetInstance()
                           ->RegisterFileSystemForVirtualPath(
                               storage::kFileSystemTypePluginPrivate,
                               ppapi::kPluginPrivateRootName, base::FilePath());
    EXPECT_TRUE(storage::ValidateIsolatedFileSystemId(fsid));
    filesystem_context_->OpenPluginPrivateFileSystem(
        url::Origin::Create(origin), storage::kFileSystemTypePluginPrivate,
        fsid, plugin_name, storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce(&RemovePluginPrivateDataTester::OnFileSystemOpened,
                       base::Unretained(this), &await_completion));
    await_completion.BlockUntilNotified();
    return fsid;
  }

  // Creates a file named |file_name| in the PluginPrivateFileSystem identified
  // by |origin| and |fsid|. Returns the URL for the created file. The file
  // must not already exist or the test will fail.
  storage::FileSystemURL CreateFile(const GURL& origin,
                                    const std::string& fsid,
                                    const std::string& file_name) {
    AwaitCompletionHelper await_completion;
    std::string root = storage::GetIsolatedFileSystemRootURIString(
        origin, fsid, ppapi::kPluginPrivateRootName);
    storage::FileSystemURL file_url =
        filesystem_context_->CrackURL(GURL(root + file_name));
    storage::AsyncFileUtil* file_util = filesystem_context_->GetAsyncFileUtil(
        storage::kFileSystemTypePluginPrivate);
    std::unique_ptr<storage::FileSystemOperationContext> operation_context =
        std::make_unique<storage::FileSystemOperationContext>(
            filesystem_context_);
    operation_context->set_allowed_bytes_growth(
        storage::QuotaManager::kNoLimit);
    file_util->EnsureFileExists(
        std::move(operation_context), file_url,
        base::BindOnce(&RemovePluginPrivateDataTester::OnFileCreated,
                       base::Unretained(this), &await_completion));
    await_completion.BlockUntilNotified();
    return file_url;
  }

  void DeleteFile(storage::FileSystemURL file_url) {
    AwaitCompletionHelper await_completion;
    storage::AsyncFileUtil* file_util = filesystem_context_->GetAsyncFileUtil(
        storage::kFileSystemTypePluginPrivate);
    std::unique_ptr<storage::FileSystemOperationContext> operation_context =
        std::make_unique<storage::FileSystemOperationContext>(
            filesystem_context_);
    file_util->DeleteFile(
        std::move(operation_context), file_url,
        base::BindOnce(&RemovePluginPrivateDataTester::OnFileDeleted,
                       base::Unretained(this), &await_completion));
    await_completion.BlockUntilNotified();
  }

  // Sets the last_access_time and last_modified_time to |time_stamp| on the
  // file specified by |file_url|. The file must already exist.
  void SetFileTimestamp(const storage::FileSystemURL& file_url,
                        const base::Time& time_stamp) {
    AwaitCompletionHelper await_completion;
    storage::AsyncFileUtil* file_util = filesystem_context_->GetAsyncFileUtil(
        storage::kFileSystemTypePluginPrivate);
    std::unique_ptr<storage::FileSystemOperationContext> operation_context =
        std::make_unique<storage::FileSystemOperationContext>(
            filesystem_context_);
    file_util->Touch(
        std::move(operation_context), file_url, time_stamp, time_stamp,
        base::BindOnce(&RemovePluginPrivateDataTester::OnFileTouched,
                       base::Unretained(this), &await_completion));
    await_completion.BlockUntilNotified();
  }

  void OnFileSystemOpened(AwaitCompletionHelper* await_completion,
                          base::File::Error result) {
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
    await_completion->Notify();
  }

  void OnFileCreated(AwaitCompletionHelper* await_completion,
                     base::File::Error result,
                     bool created) {
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
    EXPECT_TRUE(created);
    await_completion->Notify();
  }

  void OnFileDeleted(AwaitCompletionHelper* await_completion,
                     base::File::Error result) {
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
    await_completion->Notify();
  }

  void OnFileTouched(AwaitCompletionHelper* await_completion,
                     base::File::Error result) {
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
    await_completion->Notify();
  }

  // If |origin| exists in the PluginPrivateFileSystem, set
  // |data_exists_for_origin| to true, false otherwise.
  void CheckIfDataExistsForOriginOnFileTaskRunner(
      const url::Origin& origin,
      bool* data_exists_for_origin,
      AwaitCompletionHelper* await_completion) {
    storage::FileSystemBackend* backend =
        filesystem_context_->GetFileSystemBackend(
            storage::kFileSystemTypePluginPrivate);
    storage::FileSystemQuotaUtil* quota_util = backend->GetQuotaUtil();

    // Determine the set of origins used.
    std::vector<url::Origin> origins =
        quota_util->GetOriginsForTypeOnFileTaskRunner(
            storage::kFileSystemTypePluginPrivate);
    *data_exists_for_origin = base::Contains(origins, origin);

    // AwaitCompletionHelper and MessageLoop don't work on a
    // SequencedTaskRunner, so post a task on the IO thread.
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&AwaitCompletionHelper::Notify,
                                  base::Unretained(await_completion)));
  }

  // We don't own this pointer.
  storage::FileSystemContext* filesystem_context_;

  // Keep track of the URL for the ClearKey file so that it can be written to
  // or deleted.
  storage::FileSystemURL clearkey_file_;

  DISALLOW_COPY_AND_ASSIGN(RemovePluginPrivateDataTester);
};
#endif  // BUILDFLAG(ENABLE_PLUGINS)

class MockDataRemovalObserver : public StoragePartition::DataRemovalObserver {
 public:
  explicit MockDataRemovalObserver(StoragePartition* partition) {
    observer_.Add(partition);
  }

  MOCK_METHOD4(OnOriginDataCleared,
               void(uint32_t,
                    base::RepeatingCallback<bool(const url::Origin&)>,
                    base::Time,
                    base::Time));

 private:
  ScopedObserver<StoragePartition, StoragePartition::DataRemovalObserver>
      observer_{this};
};

bool IsWebSafeSchemeForTest(const std::string& scheme) {
  return scheme == url::kHttpScheme;
}

bool DoesOriginMatchForUnprotectedWeb(
    const url::Origin& origin,
    storage::SpecialStoragePolicy* special_storage_policy) {
  if (IsWebSafeSchemeForTest(origin.scheme()))
    return !special_storage_policy->IsStorageProtected(origin.GetURL());

  return false;
}

bool DoesOriginMatchForBothProtectedAndUnprotectedWeb(
    const url::Origin& origin,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return true;
}

bool DoesOriginMatchUnprotected(
    const url::Origin& desired_origin,
    const url::Origin& origin,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return origin.scheme() != desired_origin.scheme();
}

void ClearQuotaData(content::StoragePartition* partition,
                    base::RunLoop* loop_to_quit) {
  partition->ClearData(
      kAllQuotaRemoveMask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      GURL(), base::Time(), base::Time::Max(), loop_to_quit->QuitClosure());
}

void ClearQuotaDataWithOriginMatcher(
    content::StoragePartition* partition,
    StoragePartition::OriginMatcherFunction origin_matcher,
    const base::Time delete_begin,
    base::RunLoop* loop_to_quit) {
  partition->ClearData(kAllQuotaRemoveMask,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                       std::move(origin_matcher), nullptr, false, delete_begin,
                       base::Time::Max(), loop_to_quit->QuitClosure());
}

void ClearQuotaDataForOrigin(content::StoragePartition* partition,
                             const GURL& remove_origin,
                             const base::Time delete_begin,
                             base::RunLoop* loop_to_quit) {
  partition->ClearData(kAllQuotaRemoveMask,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                       remove_origin, delete_begin, base::Time::Max(),
                       loop_to_quit->QuitClosure());
}

void ClearQuotaDataForNonPersistent(content::StoragePartition* partition,
                                    const base::Time delete_begin,
                                    base::RunLoop* loop_to_quit) {
  partition->ClearData(kAllQuotaRemoveMask,
                       ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT,
                       GURL(), delete_begin, base::Time::Max(),
                       loop_to_quit->QuitClosure());
}

void ClearCookies(content::StoragePartition* partition,
                  const base::Time delete_begin,
                  const base::Time delete_end,
                  base::RunLoop* run_loop) {
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_COOKIES,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, GURL(),
                       delete_begin, delete_end, run_loop->QuitClosure());
}

void ClearCookiesMatchingInfo(content::StoragePartition* partition,
                              CookieDeletionFilterPtr delete_filter,
                              base::RunLoop* run_loop) {
  base::Time delete_begin;
  if (delete_filter->created_after_time.has_value())
    delete_begin = delete_filter->created_after_time.value();
  base::Time delete_end;
  if (delete_filter->created_before_time.has_value())
    delete_end = delete_filter->created_before_time.value();
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_COOKIES,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                       StoragePartition::OriginMatcherFunction(),
                       std::move(delete_filter), false, delete_begin,
                       delete_end, run_loop->QuitClosure());
}

void ClearStuff(uint32_t remove_mask,
                content::StoragePartition* partition,
                const base::Time delete_begin,
                const base::Time delete_end,
                StoragePartition::OriginMatcherFunction origin_matcher,
                base::RunLoop* run_loop) {
  partition->ClearData(remove_mask,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                       std::move(origin_matcher), nullptr, false, delete_begin,
                       delete_end, run_loop->QuitClosure());
}

void ClearData(content::StoragePartition* partition, base::RunLoop* run_loop) {
  base::Time time;
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, GURL(),
                       time, time, run_loop->QuitClosure());
}

void ClearCodeCache(content::StoragePartition* partition,
                    base::Time begin_time,
                    base::Time end_time,
                    base::RepeatingCallback<bool(const GURL&)> url_predicate,
                    base::RunLoop* run_loop) {
  partition->ClearCodeCaches(begin_time, end_time, url_predicate,
                             run_loop->QuitClosure());
}

bool FilterURL(const GURL& filter_url, const GURL& url) {
  return url == filter_url;
}

#if BUILDFLAG(ENABLE_PLUGINS)
void ClearPluginPrivateData(content::StoragePartition* partition,
                            const GURL& storage_origin,
                            const base::Time delete_begin,
                            const base::Time delete_end,
                            base::RunLoop* run_loop) {
  partition->ClearData(
      StoragePartitionImpl::REMOVE_DATA_MASK_PLUGIN_PRIVATE_DATA,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, storage_origin,
      delete_begin, delete_end, run_loop->QuitClosure());
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

bool FilterMatchesCookie(const CookieDeletionFilterPtr& filter,
                         const net::CanonicalCookie& cookie) {
  return network::DeletionFilterToInfo(filter.Clone()).Matches(cookie);
}

}  // namespace

class StoragePartitionImplTest : public testing::Test {
 public:
  StoragePartitionImplTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()) {
    // Configures the Conversion API to run in memory to speed up it's
    // initialization and avoid timeouts. See https://crbug.com/1080764.
    ConversionManagerImpl::RunInMemoryForTesting();
    feature_list_.InitAndEnableFeature(features::kConversionMeasurement);
  }

  storage::MockQuotaManager* GetMockManager() {
    if (!quota_manager_.get()) {
      quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
          browser_context_->IsOffTheRecord(), browser_context_->GetPath(),
          GetIOThreadTaskRunner({}).get(),
          browser_context_->GetSpecialStoragePolicy());
      auto quota_client = base::MakeRefCounted<storage::MockQuotaClient>(
          quota_manager_->proxy(), base::span<const storage::MockOriginData>(),
          storage::QuotaClientType::kFileSystem);
      quota_manager_->proxy()->RegisterClient(
          std::move(quota_client), storage::QuotaClientType::kFileSystem,
          {blink::mojom::StorageType::kTemporary,
           blink::mojom::StorageType::kPersistent});
    }
    return quota_manager_.get();
  }

  TestBrowserContext* browser_context() { return browser_context_.get(); }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;

  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionImplTest);
};

class StoragePartitionShaderClearTest : public testing::Test {
 public:
  StoragePartitionShaderClearTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()) {
    InitShaderCacheFactorySingleton(base::ThreadTaskRunnerHandle::Get());
    GetShaderCacheFactorySingleton()->SetCacheInfo(
        kDefaultClientId,
        BrowserContext::GetDefaultStoragePartition(browser_context())
            ->GetPath());
    cache_ = GetShaderCacheFactorySingleton()->Get(kDefaultClientId);
  }

  ~StoragePartitionShaderClearTest() override {
    cache_ = nullptr;
    GetShaderCacheFactorySingleton()->RemoveCacheInfo(kDefaultClientId);
  }

  void InitCache() {
    net::TestCompletionCallback available_cb;
    int rv = cache_->SetAvailableCallback(available_cb.callback());
    ASSERT_EQ(net::OK, available_cb.GetResult(rv));
    EXPECT_EQ(0, cache_->Size());

    cache_->Cache(kCacheKey, kCacheValue);

    net::TestCompletionCallback complete_cb;

    rv = cache_->SetCacheCompleteCallback(complete_cb.callback());
    ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  }

  size_t Size() { return cache_->Size(); }

  TestBrowserContext* browser_context() { return browser_context_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;

  scoped_refptr<gpu::ShaderDiskCache> cache_;
};

// Tests ---------------------------------------------------------------------

TEST_F(StoragePartitionShaderClearTest, ClearShaderCache) {
  InitCache();
  EXPECT_EQ(1u, Size());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearData,
                                BrowserContext::GetDefaultStoragePartition(
                                    browser_context()),
                                &run_loop));
  run_loop.Run();
  EXPECT_EQ(0u, Size());
}

TEST_F(StoragePartitionImplTest, QuotaClientTypesGeneration) {
  EXPECT_THAT(StoragePartitionImpl::GenerateQuotaClientTypes(
                  StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS),
              testing::ElementsAre(storage::QuotaClientType::kFileSystem));
  EXPECT_THAT(StoragePartitionImpl::GenerateQuotaClientTypes(
                  StoragePartition::REMOVE_DATA_MASK_WEBSQL),
              testing::ElementsAre(storage::QuotaClientType::kDatabase));
  EXPECT_THAT(StoragePartitionImpl::GenerateQuotaClientTypes(
                  StoragePartition::REMOVE_DATA_MASK_APPCACHE),
              testing::ElementsAre(storage::QuotaClientType::kAppcache));
  EXPECT_THAT(StoragePartitionImpl::GenerateQuotaClientTypes(
                  StoragePartition::REMOVE_DATA_MASK_INDEXEDDB),
              testing::ElementsAre(storage::QuotaClientType::kIndexedDatabase));
  EXPECT_THAT(
      StoragePartitionImpl::GenerateQuotaClientTypes(kAllQuotaRemoveMask),
      testing::UnorderedElementsAre(
          storage::QuotaClientType::kFileSystem,
          storage::QuotaClientType::kDatabase,
          storage::QuotaClientType::kAppcache,
          storage::QuotaClientType::kIndexedDatabase));
}

void PopulateTestQuotaManagedPersistentData(storage::MockQuotaManager* manager,
                                            const url::Origin& origin1,
                                            const url::Origin& origin2) {
  manager->AddOrigin(origin1, kPersistent, {kClientFile}, base::Time());
  manager->AddOrigin(origin2, kPersistent, {kClientFile},
                     base::Time::Now() - base::TimeDelta::FromDays(1));

  EXPECT_TRUE(manager->OriginHasData(origin1, kPersistent, kClientFile));
  EXPECT_TRUE(manager->OriginHasData(origin2, kPersistent, kClientFile));
}

void PopulateTestQuotaManagedTemporaryData(storage::MockQuotaManager* manager,
                                           const url::Origin& origin1,
                                           const url::Origin& origin2) {
  manager->AddOrigin(origin1, kTemporary, {kClientFile}, base::Time::Now());
  manager->AddOrigin(origin2, kTemporary, {kClientFile},
                     base::Time::Now() - base::TimeDelta::FromDays(1));

  EXPECT_TRUE(manager->OriginHasData(origin1, kTemporary, kClientFile));
  EXPECT_TRUE(manager->OriginHasData(origin2, kTemporary, kClientFile));
}

void PopulateTestQuotaManagedData(storage::MockQuotaManager* manager,
                                  const url::Origin& origin1,
                                  const url::Origin& origin2,
                                  const url::Origin& origin3) {
  // Set up origin1 with a temporary quota, origin2 with a persistent quota, and
  // origin3 with both. origin1 is modified now, origin2 is modified at the
  // beginning of time, and origin3 is modified one day ago.
  PopulateTestQuotaManagedTemporaryData(manager, origin1, origin3);
  PopulateTestQuotaManagedPersistentData(manager, origin2, origin3);
  EXPECT_FALSE(manager->OriginHasData(origin1, kPersistent, kClientFile));
  EXPECT_FALSE(manager->OriginHasData(origin2, kTemporary, kClientFile));
}

void PopulateTestQuotaManagedNonBrowsingData(
    const url::Origin& origin,
    storage::MockQuotaManager* manager) {
  manager->AddOrigin(origin, kTemporary, {kClientFile}, base::Time());
  manager->AddOrigin(origin, kPersistent, {kClientFile}, base::Time());
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverBoth) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  PopulateTestQuotaManagedData(GetMockManager(), kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin3, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin3, kPersistent, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverOnlyTemporary) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));

  PopulateTestQuotaManagedTemporaryData(GetMockManager(), kOrigin1, kOrigin2);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverOnlyPersistent) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));

  PopulateTestQuotaManagedPersistentData(GetMockManager(), kOrigin1, kOrigin2);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverNeither) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin3, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin3, kPersistent, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverSpecificOrigin) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  PopulateTestQuotaManagedData(GetMockManager(), kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaDataForOrigin, partition,
                                kOrigin1.GetURL(), base::Time(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin3, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin3, kPersistent, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForLastHour) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  PopulateTestQuotaManagedData(GetMockManager(), kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearQuotaDataForOrigin, partition, GURL(),
                     base::Time::Now() - base::TimeDelta::FromHours(1),
                     &run_loop));
  run_loop.Run();

  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin3, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin3, kPersistent, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForLastWeek) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  PopulateTestQuotaManagedData(GetMockManager(), kOrigin1, kOrigin2, kOrigin3);

  base::RunLoop run_loop;
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(GetMockManager());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearQuotaDataForNonPersistent, partition,
                     base::Time::Now() - base::TimeDelta::FromDays(7),
                     &run_loop));
  run_loop.Run();

  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin3, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin3, kPersistent, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedUnprotectedOrigins) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  // Protect kOrigin1.
  auto mock_policy = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
  mock_policy->AddProtected(kOrigin1.GetURL());

  PopulateTestQuotaManagedData(GetMockManager(), kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(GetMockManager());
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearQuotaDataWithOriginMatcher, partition,
                     base::BindRepeating(&DoesOriginMatchForUnprotectedWeb),
                     base::Time(), &run_loop));
  run_loop.Run();

  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin3, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin3, kPersistent, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedProtectedOrigins) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  // Protect kOrigin1.
  auto mock_policy = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
  mock_policy->AddProtected(kOrigin1.GetURL());

  PopulateTestQuotaManagedData(GetMockManager(), kOrigin1, kOrigin2, kOrigin3);

  // Try to remove kOrigin1. Expect success.
  base::RunLoop run_loop;
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(GetMockManager());
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearQuotaDataWithOriginMatcher, partition,
                     base::BindRepeating(
                         &DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                     base::Time(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin3, kTemporary, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->OriginHasData(kOrigin3, kPersistent, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedIgnoreDevTools) {
  const url::Origin kOrigin =
      url::Origin::Create(GURL("devtools://abcdefghijklmnopqrstuvw/"));

  PopulateTestQuotaManagedNonBrowsingData(kOrigin, GetMockManager());

  base::RunLoop run_loop;
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(GetMockManager());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearQuotaDataWithOriginMatcher, partition,
                     base::BindRepeating(&DoesOriginMatchUnprotected, kOrigin),
                     base::Time(), &run_loop));
  run_loop.Run();

  // Check that devtools data isn't removed.
  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin, kTemporary, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->OriginHasData(kOrigin, kPersistent, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveCookieForever) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://host1:1/"));

  StoragePartition* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());

  RemoveCookieTester tester(partition);
  tester.AddCookie(kOrigin);
  ASSERT_TRUE(tester.ContainsCookie(kOrigin));

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearCookies, partition, base::Time(),
                                base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.ContainsCookie(kOrigin));
}

TEST_F(StoragePartitionImplTest, RemoveCookieLastHour) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://host1:1/"));

  StoragePartition* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());

  RemoveCookieTester tester(partition);
  tester.AddCookie(kOrigin);
  ASSERT_TRUE(tester.ContainsCookie(kOrigin));

  base::Time an_hour_ago = base::Time::Now() - base::TimeDelta::FromHours(1);

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearCookies, partition, an_hour_ago,
                                base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.ContainsCookie(kOrigin));
}

TEST_F(StoragePartitionImplTest, RemoveCookieWithDeleteInfo) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://host1:1/"));

  StoragePartition* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());

  RemoveCookieTester tester(partition);
  tester.AddCookie(kOrigin);
  ASSERT_TRUE(tester.ContainsCookie(kOrigin));

  base::RunLoop run_loop2;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearCookiesMatchingInfo, partition,
                                CookieDeletionFilter::New(), &run_loop2));
  run_loop2.RunUntilIdle();
  EXPECT_FALSE(tester.ContainsCookie(kOrigin));
}

TEST_F(StoragePartitionImplTest, RemoveUnprotectedLocalStorageForever) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  // Protect kOrigin1.
  auto mock_policy = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
  mock_policy->AddProtected(kOrigin1.GetURL());

  RemoveLocalStorageTester tester(task_environment(), browser_context());

  tester.AddDOMStorageTestData(kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClearStuff, StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
          partition, base::Time(), base::Time::Max(),
          base::BindRepeating(&DoesOriginMatchForUnprotectedWeb), &run_loop));
  run_loop.Run();
  // ClearData only guarantees that tasks to delete data are scheduled when its
  // callback is invoked. It doesn't guarantee data has actually been cleared.
  // So run all scheduled tasks to make sure data is cleared.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplTest, RemoveProtectedLocalStorageForever) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  // Protect kOrigin1.
  auto mock_policy = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
  mock_policy->AddProtected(kOrigin1.GetURL());

  RemoveLocalStorageTester tester(task_environment(), browser_context());

  tester.AddDOMStorageTestData(kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearStuff,
                     StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                     partition, base::Time(), base::Time::Max(),
                     base::BindRepeating(
                         &DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                     &run_loop));
  run_loop.Run();
  // ClearData only guarantees that tasks to delete data are scheduled when its
  // callback is invoked. It doesn't guarantee data has actually been cleared.
  // So run all scheduled tasks to make sure data is cleared.
  base::RunLoop().RunUntilIdle();

  // Even if kOrigin1 is protected, it will be deleted since we specify
  // ClearData to delete protected data.
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplTest, RemoveLocalStorageForLastWeek) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  RemoveLocalStorageTester tester(task_environment(), browser_context());

  tester.AddDOMStorageTestData(kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  base::Time a_week_ago = base::Time::Now() - base::TimeDelta::FromDays(7);

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearStuff,
                     StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                     partition, a_week_ago, base::Time::Max(),
                     base::BindRepeating(
                         &DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                     &run_loop));
  run_loop.Run();
  // ClearData only guarantees that tasks to delete data are scheduled when its
  // callback is invoked. It doesn't guarantee data has actually been cleared.
  // So run all scheduled tasks to make sure data is cleared.
  base::RunLoop().RunUntilIdle();

  // kOrigin1 and kOrigin2 do not have age more than a week.
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplTest, ClearCodeCache) {
  const GURL kResourceURL("http://host4/script.js");

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  // Ensure code cache is initialized.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(partition->GetGeneratedCodeCacheContext() != nullptr);

  RemoveCodeCacheTester tester(partition->GetGeneratedCodeCacheContext());

  GURL origin = GURL("http://host1:1/");
  std::string data("SomeData");
  tester.AddEntry(RemoveCodeCacheTester::kJs, kResourceURL, origin, data);
  EXPECT_TRUE(
      tester.ContainsEntry(RemoveCodeCacheTester::kJs, kResourceURL, origin));
  EXPECT_EQ(tester.received_data(), data);

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearCodeCache, partition, base::Time(), base::Time(),
                     base::RepeatingCallback<bool(const GURL&)>(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(
      tester.ContainsEntry(RemoveCodeCacheTester::kJs, kResourceURL, origin));

  // Make sure there isn't a second invalid callback sitting in the queue.
  // (this used to be a bug).
  base::RunLoop().RunUntilIdle();
}

TEST_F(StoragePartitionImplTest, ClearCodeCacheSpecificURL) {
  const GURL kResourceURL("http://host4/script.js");
  const GURL kFilterResourceURLForCodeCache("http://host5/script.js");

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  // Ensure code cache is initialized.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(partition->GetGeneratedCodeCacheContext() != nullptr);

  RemoveCodeCacheTester tester(partition->GetGeneratedCodeCacheContext());

  GURL origin = GURL("http://host1:1/");
  std::string data("SomeData");
  tester.AddEntry(RemoveCodeCacheTester::kJs, kResourceURL, origin, data);
  tester.AddEntry(RemoveCodeCacheTester::kJs, kFilterResourceURLForCodeCache,
                  origin, data);
  EXPECT_TRUE(
      tester.ContainsEntry(RemoveCodeCacheTester::kJs, kResourceURL, origin));
  EXPECT_TRUE(tester.ContainsEntry(RemoveCodeCacheTester::kJs,
                                   kFilterResourceURLForCodeCache, origin));
  EXPECT_EQ(tester.received_data(), data);

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClearCodeCache, partition, base::Time(), base::Time(),
          base::BindRepeating(&FilterURL, kFilterResourceURLForCodeCache),
          &run_loop));
  run_loop.Run();

  EXPECT_TRUE(
      tester.ContainsEntry(RemoveCodeCacheTester::kJs, kResourceURL, origin));
  EXPECT_FALSE(tester.ContainsEntry(RemoveCodeCacheTester::kJs,
                                    kFilterResourceURLForCodeCache, origin));

  // Make sure there isn't a second invalid callback sitting in the queue.
  // (this used to be a bug).
  base::RunLoop().RunUntilIdle();
}

TEST_F(StoragePartitionImplTest, ClearCodeCacheDateRange) {
  const GURL kResourceURL("http://host4/script.js");
  const GURL kFilterResourceURLForCodeCache("http://host5/script.js");

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  // Ensure code cache is initialized.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(partition->GetGeneratedCodeCacheContext() != nullptr);

  RemoveCodeCacheTester tester(partition->GetGeneratedCodeCacheContext());

  base::Time current_time = base::Time::NowFromSystemTime();
  base::Time out_of_range_time = current_time - base::TimeDelta::FromHours(3);
  base::Time begin_time = current_time - base::TimeDelta::FromHours(2);
  base::Time in_range_time = current_time - base::TimeDelta::FromHours(1);

  GURL origin = GURL("http://host1:1/");
  std::string data("SomeData");
  tester.AddEntry(RemoveCodeCacheTester::kJs, kResourceURL, origin, data);
  EXPECT_TRUE(
      tester.ContainsEntry(RemoveCodeCacheTester::kJs, kResourceURL, origin));
  EXPECT_EQ(tester.received_data(), data);
  tester.SetLastUseTime(RemoveCodeCacheTester::kJs, kResourceURL, origin,
                        out_of_range_time);

  // Add a new entry.
  tester.AddEntry(RemoveCodeCacheTester::kJs, kFilterResourceURLForCodeCache,
                  origin, data);
  EXPECT_TRUE(tester.ContainsEntry(RemoveCodeCacheTester::kJs,
                                   kFilterResourceURLForCodeCache, origin));
  tester.SetLastUseTime(RemoveCodeCacheTester::kJs,
                        kFilterResourceURLForCodeCache, origin, in_range_time);

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClearCodeCache, partition, begin_time, current_time,
          base::BindRepeating(&FilterURL, kFilterResourceURLForCodeCache),
          &run_loop));
  run_loop.Run();

  EXPECT_TRUE(
      tester.ContainsEntry(RemoveCodeCacheTester::kJs, kResourceURL, origin));
  EXPECT_FALSE(tester.ContainsEntry(RemoveCodeCacheTester::kJs,
                                    kFilterResourceURLForCodeCache, origin));

  // Make sure there isn't a second invalid callback sitting in the queue.
  // (this used to be a bug).
  base::RunLoop().RunUntilIdle();
}

TEST_F(StoragePartitionImplTest, ClearWasmCodeCache) {
  const GURL kResourceURL("http://host4/script.js");

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  // Ensure code cache is initialized.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(partition->GetGeneratedCodeCacheContext() != nullptr);

  RemoveCodeCacheTester tester(partition->GetGeneratedCodeCacheContext());

  GURL origin = GURL("http://host1:1/");
  std::string data("SomeData.wasm");
  tester.AddEntry(RemoveCodeCacheTester::kWebAssembly, kResourceURL, origin,
                  data);
  EXPECT_TRUE(tester.ContainsEntry(RemoveCodeCacheTester::kWebAssembly,
                                   kResourceURL, origin));
  EXPECT_EQ(tester.received_data(), data);

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearCodeCache, partition, base::Time(), base::Time(),
                     base::RepeatingCallback<bool(const GURL&)>(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.ContainsEntry(RemoveCodeCacheTester::kWebAssembly,
                                    kResourceURL, origin));

  // Make sure there isn't a second invalid callback sitting in the queue.
  // (this used to be a bug).
  base::RunLoop().RunUntilIdle();
}

TEST_F(StoragePartitionImplTest, ClearCodeCacheIncognito) {
  browser_context()->set_is_off_the_record(true);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  base::RunLoop().RunUntilIdle();
  // We should not create GeneratedCodeCacheContext for off the record mode.
  EXPECT_EQ(nullptr, partition->GetGeneratedCodeCacheContext());

  base::RunLoop run_loop;
  // This shouldn't crash.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearCodeCache, partition, base::Time(), base::Time(),
                     base::RepeatingCallback<bool(const GURL&)>(), &run_loop));
  run_loop.Run();
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(StoragePartitionImplTest, RemovePluginPrivateDataForever) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  RemovePluginPrivateDataTester tester(partition->GetFileSystemContext());
  tester.AddPluginPrivateTestData(kOrigin1.GetURL(), kOrigin2.GetURL());
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearPluginPrivateData, partition, GURL(),
                                base::Time(), base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin2));
}

TEST_F(StoragePartitionImplTest, RemovePluginPrivateDataLastWeek) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  base::Time a_week_ago = base::Time::Now() - base::TimeDelta::FromDays(7);

  RemovePluginPrivateDataTester tester(partition->GetFileSystemContext());
  tester.AddPluginPrivateTestData(kOrigin1.GetURL(), kOrigin2.GetURL());
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearPluginPrivateData, partition, GURL(),
                                a_week_ago, base::Time::Max(), &run_loop));
  run_loop.Run();

  // Origin1 has 1 file from 10 days ago, so it should remain around.
  // Origin2 has a current file, so it should be removed (even though the
  // second file is much older).
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin2));
}

TEST_F(StoragePartitionImplTest, RemovePluginPrivateDataForOrigin) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  RemovePluginPrivateDataTester tester(partition->GetFileSystemContext());
  tester.AddPluginPrivateTestData(kOrigin1.GetURL(), kOrigin2.GetURL());
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearPluginPrivateData, partition, kOrigin1.GetURL(),
                     base::Time(), base::Time::Max(), &run_loop));
  run_loop.Run();

  // Only Origin1 should be deleted.
  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));
}

TEST_F(StoragePartitionImplTest, RemovePluginPrivateDataAfterDeletion) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  RemovePluginPrivateDataTester tester(partition->GetFileSystemContext());
  tester.AddPluginPrivateTestData(kOrigin1.GetURL(), kOrigin2.GetURL());
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  // Delete the single file saved for |kOrigin1|. This does not remove the
  // origin from the list of Origins. However, ClearPluginPrivateData() will
  // remove it.
  tester.DeleteClearKeyTestData();
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClearPluginPrivateData, partition, GURL(),
                                base::Time(), base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin2));
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

TEST(StoragePartitionImplStaticTest, CreatePredicateForHostCookies) {
  GURL url("http://www.example.com/");
  GURL url2("https://www.example.com/");
  GURL url3("https://www.google.com/");

  base::Optional<base::Time> server_time = base::nullopt;
  CookieDeletionFilterPtr deletion_filter = CookieDeletionFilter::New();
  deletion_filter->host_name = url.host();

  base::Time now = base::Time::Now();
  std::vector<std::unique_ptr<CanonicalCookie>> valid_cookies;
  valid_cookies.push_back(
      CanonicalCookie::Create(url, "A=B", now, server_time));
  valid_cookies.push_back(
      CanonicalCookie::Create(url, "C=F", now, server_time));
  // We should match a different scheme with the same host.
  valid_cookies.push_back(
      CanonicalCookie::Create(url2, "A=B", now, server_time));

  std::vector<std::unique_ptr<CanonicalCookie>> invalid_cookies;
  // We don't match domain cookies.
  invalid_cookies.push_back(CanonicalCookie::Create(
      url2, "A=B;domain=.example.com", now, server_time));
  invalid_cookies.push_back(
      CanonicalCookie::Create(url3, "A=B", now, server_time));

  for (const auto& cookie : valid_cookies) {
    EXPECT_TRUE(FilterMatchesCookie(deletion_filter, *cookie))
        << cookie->DebugString();
  }
  for (const auto& cookie : invalid_cookies) {
    EXPECT_FALSE(FilterMatchesCookie(deletion_filter, *cookie))
        << cookie->DebugString();
  }
}

TEST_F(StoragePartitionImplTest, ConversionsClearDataForOrigin) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  ConversionManagerImpl* conversion_manager = partition->GetConversionManager();

  base::Time now = base::Time::Now();
  auto impression =
      ImpressionBuilder(now).SetExpiry(base::TimeDelta::FromDays(2)).Build();
  conversion_manager->HandleImpression(impression);
  conversion_manager->HandleConversion(DefaultConversion());

  base::RunLoop run_loop;
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_CONVERSIONS, 0,
                       impression.impression_origin().GetURL(), now, now,
                       run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(
      GetConversionsToReportForTesting(conversion_manager, base::Time::Max())
          .empty());
}

TEST_F(StoragePartitionImplTest, ConversionsClearDataWrongMask) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  ConversionManagerImpl* conversion_manager = partition->GetConversionManager();

  base::Time now = base::Time::Now();
  auto impression =
      ImpressionBuilder(now).SetExpiry(base::TimeDelta::FromDays(2)).Build();
  conversion_manager->HandleImpression(impression);
  conversion_manager->HandleConversion(DefaultConversion());

  EXPECT_FALSE(
      GetConversionsToReportForTesting(conversion_manager, base::Time::Max())
          .empty());

  // Arbitrary non-conversions mask.
  base::RunLoop run_loop;
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_COOKIES, 0,
                       impression.impression_origin().GetURL(), now, now,
                       run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(
      GetConversionsToReportForTesting(conversion_manager, base::Time::Max())
          .empty());
}

TEST_F(StoragePartitionImplTest, ConversionsClearAllData) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  ConversionManagerImpl* conversion_manager = partition->GetConversionManager();

  base::Time now = base::Time::Now();
  for (int i = 0; i < 20; i++) {
    auto origin = url::Origin::Create(
        GURL(base::StringPrintf("https://www.%d.test/", i)));
    auto impression = ImpressionBuilder(now)
                          .SetExpiry(base::TimeDelta::FromDays(2))
                          .SetImpressionOrigin(origin)
                          .SetReportingOrigin(origin)
                          .SetConversionOrigin(origin)
                          .Build();
    conversion_manager->HandleImpression(impression);
  }
  base::RunLoop run_loop;
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_CONVERSIONS, 0,
                       GURL(), now, now, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(
      GetConversionsToReportForTesting(conversion_manager, base::Time::Max())
          .empty());
}

TEST_F(StoragePartitionImplTest, ConversionsClearDataForFilter) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  ConversionManagerImpl* conversion_manager = partition->GetConversionManager();

  base::Time now = base::Time::Now();
  for (int i = 0; i < 5; i++) {
    auto impression =
        url::Origin::Create(GURL(base::StringPrintf("https://imp-%d.com/", i)));
    auto reporter = url::Origin::Create(
        GURL(base::StringPrintf("https://reporter-%d.com/", i)));
    auto conv = url::Origin::Create(
        GURL(base::StringPrintf("https://conv-%d.com/", i)));
    conversion_manager->HandleImpression(
        ImpressionBuilder(now)
            .SetImpressionOrigin(impression)
            .SetReportingOrigin(reporter)
            .SetConversionOrigin(conv)
            .SetExpiry(base::TimeDelta::FromDays(2))
            .Build());
    conversion_manager->HandleConversion(
        StorableConversion("123", conv, reporter));
  }

  EXPECT_EQ(5u, GetConversionsToReportForTesting(conversion_manager,
                                                 base::Time::Max())
                    .size());

  // Match against enough Origins to delete three of the imp/conv pairs.
  base::RunLoop run_loop;
  StoragePartition::OriginMatcherFunction func = base::BindRepeating(
      [](const url::Origin& origin, storage::SpecialStoragePolicy* policy) {
        return origin == url::Origin::Create(GURL("https://imp-2.com/")) ||
               origin == url::Origin::Create(GURL("https://conv-3.com/")) ||
               origin == url::Origin::Create(GURL("https://rep-4.com/")) ||
               origin == url::Origin::Create(GURL("https://imp-4.com/"));
      });
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_CONVERSIONS, 0, func,
                       nullptr, false, now, now, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(2u, GetConversionsToReportForTesting(conversion_manager,
                                                 base::Time::Max())
                    .size());
}

TEST_F(StoragePartitionImplTest, DataRemovalObserver) {
  const uint32_t kTestClearMask =
      content::StoragePartition::REMOVE_DATA_MASK_APPCACHE |
      content::StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  const uint32_t kTestQuotaClearMask = 0;
  const auto kTestOrigin = GURL("https://example.com");
  const auto kBeginTime = base::Time() + base::TimeDelta::FromHours(1);
  const auto kEndTime = base::Time() + base::TimeDelta::FromHours(2);
  const auto origin_callback_valid =
      [&](base::RepeatingCallback<bool(const url::Origin&)> callback) {
        return callback.Run(url::Origin::Create(kTestOrigin));
      };

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  MockDataRemovalObserver observer(partition);

  // Confirm that each of the StoragePartition interfaces for clearing origin
  // based data notify observers appropriately.
  EXPECT_CALL(
      observer,
      OnOriginDataCleared(kTestClearMask, testing::Truly(origin_callback_valid),
                          base::Time(), base::Time::Max()));
  partition->ClearDataForOrigin(kTestClearMask, kTestQuotaClearMask,
                                kTestOrigin);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(
      observer,
      OnOriginDataCleared(kTestClearMask, testing::Truly(origin_callback_valid),
                          kBeginTime, kEndTime));
  partition->ClearData(kTestClearMask, kTestQuotaClearMask, kTestOrigin,
                       kBeginTime, kEndTime, base::DoNothing());
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(
      observer,
      OnOriginDataCleared(kTestClearMask, testing::Truly(origin_callback_valid),
                          kBeginTime, kEndTime));
  partition->ClearData(
      kTestClearMask, kTestQuotaClearMask,
      base::BindLambdaForTesting([&](const url::Origin& origin,
                                     storage::SpecialStoragePolicy* policy) {
        return origin == url::Origin::Create(kTestOrigin);
      }),
      /* cookie_deletion_filter */ nullptr, /* perform_storage_cleanup */ false,
      kBeginTime, kEndTime, base::DoNothing());
}

}  // namespace content
