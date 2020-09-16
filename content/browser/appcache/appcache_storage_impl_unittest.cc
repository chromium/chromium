// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_storage_impl.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/containers/stack.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_database.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_request.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/mock_appcache_policy.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/test/test_utils.h"
#include "sql/test/test_helpers.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

using blink::mojom::StorageType;

namespace {

constexpr int kMockProcessId = 1;
constexpr int kMockQuota = 5000;

// The Reinitialize test needs some http accessible resources to run,
// we mock stuff inprocess for that.
static GURL GetMockUrl(const std::string& path) {
  return GURL("http://mockhost/" + path);
}

const int kProcessId = 1;
std::unique_ptr<base::Thread> background_thread;

bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
  if (params->url_request.url == GetMockUrl("manifest")) {
    URLLoaderInterceptor::WriteResponse("", "CACHE MANIFEST\n",
                                        params->client.get());
    return true;
  } else if (params->url_request.url == GetMockUrl("empty.html")) {
    URLLoaderInterceptor::WriteResponse("", "", params->client.get());
    return true;
  }
  return false;
}

}  // namespace

class AppCacheStorageImplTest : public testing::Test {
 public:
  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
    explicit MockStorageDelegate(AppCacheStorageImplTest* test)
        : loaded_cache_id_(0),
          stored_group_success_(false),
          would_exceed_quota_(false),
          obsoleted_success_(false),
          found_cache_id_(blink::mojom::kAppCacheNoCacheId),
          test_(test) {}

    void OnCacheLoaded(AppCache* cache, int64_t cache_id) override {
      loaded_cache_ = cache;
      loaded_cache_id_ = cache_id;
      test_->ScheduleNextTask();
    }

    void OnGroupLoaded(AppCacheGroup* group,
                       const GURL& manifest_url) override {
      loaded_group_ = group;
      loaded_manifest_url_ = manifest_url;
      loaded_groups_newest_cache_ =
          group ? group->newest_complete_cache() : nullptr;
      test_->ScheduleNextTask();
    }

    void OnGroupAndNewestCacheStored(AppCacheGroup* group,
                                     AppCache* newest_cache,
                                     bool success,
                                     bool would_exceed_quota) override {
      stored_group_ = group;
      stored_group_success_ = success;
      would_exceed_quota_ = would_exceed_quota;
      test_->ScheduleNextTask();
    }

    void OnGroupMadeObsolete(AppCacheGroup* group,
                             bool success,
                             int response_code) override {
      obsoleted_group_ = group;
      obsoleted_success_ = success;
      test_->ScheduleNextTask();
    }

    void OnMainResponseFound(const GURL& url,
                             const AppCacheEntry& entry,
                             const GURL& namespace_entry_url,
                             const AppCacheEntry& fallback_entry,
                             int64_t cache_id,
                             int64_t group_id,
                             const GURL& manifest_url) override {
      found_url_ = url;
      found_entry_ = entry;
      found_namespace_entry_url_ = namespace_entry_url;
      found_fallback_entry_ = fallback_entry;
      found_cache_id_ = cache_id;
      found_group_id_ = group_id;
      found_manifest_url_ = manifest_url;
      test_->ScheduleNextTask();
    }

    scoped_refptr<AppCache> loaded_cache_;
    int64_t loaded_cache_id_;
    scoped_refptr<AppCacheGroup> loaded_group_;
    GURL loaded_manifest_url_;
    scoped_refptr<AppCache> loaded_groups_newest_cache_;
    scoped_refptr<AppCacheGroup> stored_group_;
    bool stored_group_success_;
    bool would_exceed_quota_;
    scoped_refptr<AppCacheGroup> obsoleted_group_;
    bool obsoleted_success_;
    GURL found_url_;
    AppCacheEntry found_entry_;
    GURL found_namespace_entry_url_;
    AppCacheEntry found_fallback_entry_;
    int64_t found_cache_id_;
    int64_t found_group_id_;
    GURL found_manifest_url_;
    AppCacheStorageImplTest* test_;
  };

  class MockQuotaManagerProxy : public storage::QuotaManagerProxy {
   public:
    MockQuotaManagerProxy() : QuotaManagerProxy(nullptr, nullptr) {}

    void NotifyStorageAccessed(const url::Origin& origin,
                               StorageType type) override {
      EXPECT_EQ(StorageType::kTemporary, type);
      ++notify_storage_accessed_count_;
      last_origin_ = origin;
    }

    void NotifyStorageModified(storage::QuotaClientType client_id,
                               const url::Origin& origin,
                               StorageType type,
                               int64_t delta) override {
      EXPECT_EQ(storage::QuotaClientType::kAppcache, client_id);
      EXPECT_EQ(StorageType::kTemporary, type);
      ++notify_storage_modified_count_;
      last_origin_ = origin;
      last_delta_ = delta;
    }

    // Not needed for our tests.
    void RegisterClient(
        scoped_refptr<storage::QuotaClient> client,
        storage::QuotaClientType quota_client_type,
        const std::vector<blink::mojom::StorageType>& storage_types) override {}
    void NotifyOriginInUse(const url::Origin& origin) override {}
    void NotifyOriginNoLongerInUse(const url::Origin& origin) override {}
    void SetUsageCacheEnabled(storage::QuotaClientType client_id,
                              const url::Origin& origin,
                              StorageType type,
                              bool enabled) override {}
    void GetUsageAndQuota(base::SequencedTaskRunner* original_task_runner,
                          const url::Origin& origin,
                          StorageType type,
                          UsageAndQuotaCallback callback) override {
      EXPECT_EQ(StorageType::kTemporary, type);
      if (async_) {
        original_task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::mojom::QuotaStatusCode::kOk, 0, kMockQuota));
        return;
      }
      std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, 0,
                              kMockQuota);
    }

    int notify_storage_accessed_count_ = 0;
    int notify_storage_modified_count_ = 0;
    url::Origin last_origin_;
    int last_delta_ = 0;
    bool async_ = false;

   protected:
    ~MockQuotaManagerProxy() override = default;
  };

  template <class Method>
  void RunMethod(Method method) {
    (this->*method)();
  }

  // Helper callback to run a test on our io_thread. The io_thread is spun up
  // once and reused for all tests.
  template <class Method>
  void MethodWrapper(Method method) {
    SetUpTest();

    // Ensure InitTask execution prior to conducting a test.
    FlushAllTasks();

    // We also have to wait for InitTask completion call to be performed
    // on the UI thread prior to running the test. Its guaranteed to be
    // queued by this time.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppCacheStorageImplTest::RunMethod<Method>,
                                  base::Unretained(this), method));
  }

  static void SetUpTestCase() {
    // We start the background thread as TYPE_IO because we also use the
    // db_thread for the disk_cache which needs to be of TYPE_IO.
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    background_thread =
        std::make_unique<base::Thread>("AppCacheTest::BackgroundThread");
    ASSERT_TRUE(background_thread->StartWithOptions(options));
  }

  static void TearDownTestCase() {
    background_thread.reset();
  }

  // Test harness --------------------------------------------------

  AppCacheStorageImplTest()
      : interceptor_(base::BindRepeating(&InterceptRequest)),
        weak_partition_factory_(static_cast<StoragePartitionImpl*>(
            BrowserContext::GetDefaultStoragePartition(&browser_context_))) {
    ChildProcessSecurityPolicyImpl::GetInstance()->AddForTesting(
        kProcessId, &browser_context_);
    appcache_require_origin_trial_feature_.InitAndDisableFeature(
        blink::features::kAppCacheRequireOriginTrial);
  }

  ~AppCacheStorageImplTest() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(kProcessId);
  }

  template <class Method>
  void RunTestOnUIThread(Method method) {
    base::RunLoop run_loop;
    test_finished_cb_ = run_loop.QuitClosure();
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheStorageImplTest::MethodWrapper<Method>,
                       base::Unretained(this), method));
    run_loop.Run();
  }

  void SetUpTest() {
    service_ = std::make_unique<AppCacheServiceImpl>(nullptr, nullptr);
    service_->set_appcache_policy(&mock_policy_);
    service_->Initialize(base::FilePath());
    mock_quota_manager_proxy_ = base::MakeRefCounted<MockQuotaManagerProxy>();
    service_->quota_manager_proxy_ = mock_quota_manager_proxy_;
    delegate_ = std::make_unique<MockStorageDelegate>(this);
  }

  void TearDownTest() {
    scoped_refptr<base::SequencedTaskRunner> db_runner =
        storage()->db_task_runner_;
    storage()->CancelDelegateCallbacks(delegate());
    group_ = nullptr;
    cache_ = nullptr;
    cache2_ = nullptr;
    mock_quota_manager_proxy_ = nullptr;
    delegate_.reset();
    service_.reset();
    host_remote_.reset();
    FlushTasks(db_runner.get());
    FlushTasks(background_thread->task_runner().get());
    FlushTasks(db_runner.get());
  }

  void TestFinished() {
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppCacheStorageImplTest::TestFinishedUnwound,
                                  base::Unretained(this)));
  }

  void TestFinishedUnwound() {
    TearDownTest();
    std::move(test_finished_cb_).Run();
  }

  void PushNextTask(base::OnceClosure task) {
    task_stack_.push(std::move(task));
  }

  void ScheduleNextTask() {
    if (task_stack_.empty()) {
      return;
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(task_stack_.top()));
    task_stack_.pop();
  }

  static void SignalEvent(base::WaitableEvent* event) { event->Signal(); }

  void FlushAllTasks() {
    FlushTasks(storage()->db_task_runner_.get());
    FlushTasks(background_thread->task_runner().get());
    FlushTasks(storage()->db_task_runner_.get());
  }

  void FlushTasks(base::SequencedTaskRunner* runner) {
    // We pump a task thru the db thread to ensure any tasks previously
    // scheduled on that thread have been performed prior to return.
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    runner->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheStorageImplTest::SignalEvent, &event));
    event.Wait();
  }

  // LoadCache_Miss ----------------------------------------------------

  void LoadCache_Miss() {
    // Attempt to load a cache that doesn't exist. Should
    // complete asynchronously.
    PushNextTask(base::BindOnce(&AppCacheStorageImplTest::Verify_LoadCache_Miss,
                                base::Unretained(this)));

    storage()->LoadCache(111, delegate());
    EXPECT_NE(111, delegate()->loaded_cache_id_);
  }

  void Verify_LoadCache_Miss() {
    EXPECT_EQ(111, delegate()->loaded_cache_id_);
    EXPECT_FALSE(delegate()->loaded_cache_.get());
    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_accessed_count_);
    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_modified_count_);
    TestFinished();
  }

  // LoadCache_NearHit -------------------------------------------------

  void LoadCache_NearHit() {
    // Attempt to load a cache that is currently in use
    // and does not require loading from storage. This
    // load should complete syncly.

    // Setup some preconditions. Make an 'unstored' cache for
    // us to load. The ctor should put it in the working set.
    int64_t cache_id = storage()->NewCacheId();
    auto cache = base::MakeRefCounted<AppCache>(storage(), cache_id);
    cache->set_manifest_parser_version(1);
    cache->set_manifest_scope("/");

    // Conduct the test.
    storage()->LoadCache(cache_id, delegate());
    EXPECT_EQ(cache_id, delegate()->loaded_cache_id_);
    EXPECT_EQ(cache.get(), delegate()->loaded_cache_.get());
    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_accessed_count_);
    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_modified_count_);
    TestFinished();
  }

  void LoadCache_OriginTrialSuccess() {
    AddToDatabase(kManifestUrl, 222, 111, valid_token_expires());
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 111;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 1;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));

    storage()->LoadCache(111, delegate());

    PushNextTask(base::BindLambdaForTesting([&]() {
      EXPECT_EQ(111, delegate()->loaded_cache_id_);
      EXPECT_EQ(1, mock_quota_manager_proxy_->notify_storage_accessed_count_);
      EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_modified_count_);
      TestFinished();
    }));
  }

  void LoadCache_OriginTrialFailure() {
    int64_t cache_id = storage()->NewCacheId();
    int64_t group_id = storage()->NewGroupId();
    AddToDatabase(kManifestUrl, group_id, cache_id, invalid_token_expires());
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = cache_id;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 1;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));

    storage()->LoadCache(cache_id, delegate());
    EXPECT_FALSE(delegate()->loaded_cache_.get());
    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_accessed_count_);
    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_modified_count_);
    TestFinished();
  }

  // CreateGroup  --------------------------------------------

  void CreateGroupInEmptyOrigin() {
    // Attempt to load a group that doesn't exist, one should
    // be created for us, but not stored.

    // Since the origin has no groups, the storage class will respond
    // syncly.
    storage()->LoadOrCreateGroup(kManifestUrl, delegate());
    Verify_CreateGroup();
  }

  void CreateGroupInPopulatedOrigin() {
    // Attempt to load a group that doesn't exist, one should
    // be created for us, but not stored.
    PushNextTask(base::BindOnce(&AppCacheStorageImplTest::Verify_CreateGroup,
                                base::Unretained(this)));

    // Since the origin has groups, storage class will have to
    // consult the database and completion will be async.
    storage()->usage_map_[kOrigin] = kDefaultEntrySize;

    storage()->LoadOrCreateGroup(kManifestUrl, delegate());
    EXPECT_FALSE(delegate()->loaded_group_.get());
  }

  void Verify_CreateGroup() {
    EXPECT_EQ(kManifestUrl, delegate()->loaded_manifest_url_);
    EXPECT_TRUE(delegate()->loaded_group_.get());
    EXPECT_TRUE(delegate()->loaded_group_->HasOneRef());
    EXPECT_FALSE(delegate()->loaded_group_->newest_complete_cache());

    // Should not have been stored in the database.
    AppCacheDatabase::GroupRecord record;
    EXPECT_FALSE(
        database()->FindGroup(delegate()->loaded_group_->group_id(), &record));

    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_accessed_count_);
    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_modified_count_);

    TestFinished();
  }

  // LoadGroupAndCache_FarHit  --------------------------------------

  void LoadGroupAndCache_FarHit() {
    // Attempt to load a cache that is not currently in use
    // and does require loading from disk. This
    // load should complete asynchronously.
    PushNextTask(
        base::BindOnce(&AppCacheStorageImplTest::Verify_LoadCache_Far_Hit,
                       base::Unretained(this)));

    // Setup some preconditions. Create a group and newest cache that
    // appear to be "stored" and "not currently in use".
    MakeCacheAndGroup(kManifestUrl, 1, 1, valid_token_expires(), true);
    group_ = nullptr;
    cache_ = nullptr;

    // Conduct the cache load test, completes async
    storage()->LoadCache(1, delegate());
  }

  void Verify_LoadCache_Far_Hit() {
    EXPECT_TRUE(delegate()->loaded_cache_.get());
    EXPECT_TRUE(delegate()->loaded_cache_->HasOneRef());
    EXPECT_EQ(1, delegate()->loaded_cache_id_);

    // The group should also have been loaded.
    EXPECT_TRUE(delegate()->loaded_cache_->owning_group());
    EXPECT_TRUE(delegate()->loaded_cache_->owning_group()->HasOneRef());
    EXPECT_EQ(1, delegate()->loaded_cache_->owning_group()->group_id());

    EXPECT_EQ(1, mock_quota_manager_proxy_->notify_storage_accessed_count_);
    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_modified_count_);

    // Drop things from the working set.
    delegate()->loaded_cache_ = nullptr;
    EXPECT_FALSE(delegate()->loaded_group_.get());

    // Conduct the group load test, also complete asynchronously.
    PushNextTask(
        base::BindOnce(&AppCacheStorageImplTest::Verify_LoadGroup_Far_Hit,
                       base::Unretained(this)));

    storage()->LoadOrCreateGroup(kManifestUrl, delegate());
  }

  void Verify_LoadGroup_Far_Hit() {
    EXPECT_TRUE(delegate()->loaded_group_.get());
    EXPECT_EQ(kManifestUrl, delegate()->loaded_manifest_url_);
    EXPECT_TRUE(delegate()->loaded_group_->newest_complete_cache());
    delegate()->loaded_groups_newest_cache_ = nullptr;
    EXPECT_TRUE(delegate()->loaded_group_->HasOneRef());
    EXPECT_EQ(2, mock_quota_manager_proxy_->notify_storage_accessed_count_);
    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_modified_count_);
    TestFinished();
  }

  // StoreNewGroup  --------------------------------------

  void StoreNewGroup() {
    // Store a group and its newest cache. Should complete asynchronously.
    PushNextTask(base::BindOnce(&AppCacheStorageImplTest::Verify_StoreNewGroup,
                                base::Unretained(this)));

    // Setup some preconditions. Create a group and newest cache that
    // appear to be "unstored".
    group_ = base::MakeRefCounted<AppCacheGroup>(storage(), kManifestUrl,
                                                 storage()->NewGroupId());
    cache_ = base::MakeRefCounted<AppCache>(storage(), storage()->NewCacheId());
    cache_->set_manifest_parser_version(1);
    cache_->set_manifest_scope("/");
    cache_->AddEntry(kEntryUrl,
                     AppCacheEntry(AppCacheEntry::MASTER, 1, kDefaultEntrySize,
                                   /*padding_size=*/0));
    // Hold a ref to the cache simulate the UpdateJob holding that ref,
    // and hold a ref to the group to simulate the CacheHost holding that ref.

    // Have the quota manager return asynchronously for this test.
    mock_quota_manager_proxy_->async_ = true;

    // Conduct the store test.
    storage()->StoreGroupAndNewestCache(group_.get(), cache_.get(), delegate());
  }

  void Verify_StoreNewGroup() {
    EXPECT_TRUE(delegate()->stored_group_success_);
    EXPECT_EQ(group_.get(), delegate()->stored_group_.get());
    EXPECT_EQ(cache_.get(), group_->newest_complete_cache());
    EXPECT_TRUE(cache_->is_complete());

    // Should have been stored in the database.
    AppCacheDatabase::GroupRecord group_record;
    AppCacheDatabase::CacheRecord cache_record;
    EXPECT_TRUE(database()->FindGroup(group_->group_id(), &group_record));
    EXPECT_TRUE(database()->FindCache(cache_->cache_id(), &cache_record));

    // Verify quota bookkeeping
    EXPECT_EQ(kDefaultEntrySize, storage()->usage_map_[kOrigin]);
    EXPECT_EQ(1, mock_quota_manager_proxy_->notify_storage_modified_count_);
    EXPECT_EQ(kOrigin, mock_quota_manager_proxy_->last_origin_);
    EXPECT_EQ(kDefaultEntrySize, mock_quota_manager_proxy_->last_delta_);

    TestFinished();
  }

  // StoreExistingGroup  --------------------------------------

  void StoreExistingGroup() {
    // Store a group and its newest cache. Should complete asynchronously.
    PushNextTask(
        base::BindOnce(&AppCacheStorageImplTest::Verify_StoreExistingGroup,
                       base::Unretained(this)));

    // Setup some preconditions. Create a group and old complete cache
    // that appear to be "stored"
    MakeCacheAndGroup(kManifestUrl, 1, 1, invalid_token_expires(), true);
    EXPECT_EQ(kDefaultEntrySize + kDefaultEntryPadding,
              storage()->usage_map_[kOrigin]);

    // And a newest unstored complete cache.
    cache2_ = base::MakeRefCounted<AppCache>(storage(), 2);
    cache2_->set_manifest_parser_version(1);
    cache2_->set_manifest_scope("/");
    cache2_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT, 1,
                                               kDefaultEntrySize + 100,
                                               kDefaultEntryPadding + 1000));

    // Conduct the test.
    storage()->StoreGroupAndNewestCache(group_.get(), cache2_.get(),
                                        delegate());
    EXPECT_FALSE(delegate()->stored_group_success_);
  }

  void Verify_StoreExistingGroup() {
    EXPECT_TRUE(delegate()->stored_group_success_);
    EXPECT_EQ(group_.get(), delegate()->stored_group_.get());
    EXPECT_EQ(cache2_.get(), group_->newest_complete_cache());
    EXPECT_TRUE(cache2_->is_complete());

    // The new cache should have been stored in the database.
    AppCacheDatabase::GroupRecord group_record;
    AppCacheDatabase::CacheRecord cache_record;
    EXPECT_TRUE(database()->FindGroup(1, &group_record));
    EXPECT_TRUE(database()->FindCache(2, &cache_record));

    // The old cache should have been deleted
    EXPECT_FALSE(database()->FindCache(1, &cache_record));

    // Verify quota bookkeeping
    EXPECT_EQ(kDefaultEntrySize + 100 + kDefaultEntryPadding + 1000,
              storage()->usage_map_[kOrigin]);
    EXPECT_EQ(1, mock_quota_manager_proxy_->notify_storage_modified_count_);
    EXPECT_EQ(kOrigin, mock_quota_manager_proxy_->last_origin_);
    EXPECT_EQ(100 + 1000, mock_quota_manager_proxy_->last_delta_);

    TestFinished();
  }

  // StoreExistingGroupExistingCache  -------------------------------

  void StoreExistingGroupExistingCache() {
    // Store a group with updates to its existing newest complete cache.
    // Setup some preconditions. Create a group and a complete cache that
    // appear to be "stored".

    // Setup some preconditions. Create a group and old complete cache
    // that appear to be "stored"
    MakeCacheAndGroup(kManifestUrl, 1, 1, invalid_token_expires(), true);
    EXPECT_EQ(kDefaultEntrySize + kDefaultEntryPadding,
              storage()->usage_map_[kOrigin]);

    // Change the cache.
    base::Time now = base::Time::Now();
    cache_->set_manifest_parser_version(1);
    cache_->set_manifest_scope("/");
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT,
                                              /*response_id=*/1,
                                              /*response_size=*/100,
                                              /*padding_size=*/10));
    cache_->set_update_time(now);

    PushNextTask(base::BindOnce(
        &AppCacheStorageImplTest::Verify_StoreExistingGroupExistingCache,
        base::Unretained(this), now));

    // Conduct the test.
    EXPECT_EQ(cache_.get(), group_->newest_complete_cache());
    storage()->StoreGroupAndNewestCache(group_.get(), cache_.get(), delegate());
    EXPECT_FALSE(delegate()->stored_group_success_);
  }

  void Verify_StoreExistingGroupExistingCache(base::Time expected_update_time) {
    EXPECT_TRUE(delegate()->stored_group_success_);
    EXPECT_EQ(cache_.get(), group_->newest_complete_cache());

    AppCacheDatabase::CacheRecord cache_record;
    EXPECT_TRUE(database()->FindCache(1, &cache_record));
    EXPECT_EQ(1, cache_record.cache_id);
    EXPECT_EQ(1, cache_record.group_id);
    EXPECT_FALSE(cache_record.online_wildcard);
    EXPECT_TRUE(expected_update_time == cache_record.update_time);
    EXPECT_EQ(100 + kDefaultEntrySize, cache_record.cache_size);
    EXPECT_EQ(10 + kDefaultEntryPadding, cache_record.padding_size);

    std::vector<AppCacheDatabase::EntryRecord> entry_records;
    EXPECT_TRUE(database()->FindEntriesForCache(1, &entry_records));
    EXPECT_EQ(2U, entry_records.size());
    if (entry_records[0].url == kDefaultEntryUrl)
      entry_records.erase(entry_records.begin());
    EXPECT_EQ(1, entry_records[0].cache_id);
    EXPECT_EQ(kEntryUrl, entry_records[0].url);
    EXPECT_EQ(AppCacheEntry::EXPLICIT, entry_records[0].flags);
    EXPECT_EQ(1, entry_records[0].response_id);
    EXPECT_EQ(100, entry_records[0].response_size);
    EXPECT_EQ(10, entry_records[0].padding_size);

    // Verify quota bookkeeping
    EXPECT_EQ(100 + 10 + kDefaultEntrySize + kDefaultEntryPadding,
              storage()->usage_map_[kOrigin]);
    EXPECT_EQ(1, mock_quota_manager_proxy_->notify_storage_modified_count_);
    EXPECT_EQ(kOrigin, mock_quota_manager_proxy_->last_origin_);
    EXPECT_EQ(100 + 10, mock_quota_manager_proxy_->last_delta_);

    TestFinished();
  }

  // FailStoreGroup_SizeTooBig / FailStoreGroup_PaddingTooBig   ----------------

  void FailStoreGroup_SizeTooBig() {
    // Store a group and its newest cache. Should complete asynchronously.
    PushNextTask(base::BindOnce(&AppCacheStorageImplTest::Verify_FailStoreGroup,
                                base::Unretained(this)));

    // Set up some preconditions. Create a group and newest cache that
    // appear to be "unstored" and big enough to exceed the 5M limit.
    const int64_t kTooBig = 10 * 1024 * 1024;  // 10M
    group_ = base::MakeRefCounted<AppCacheGroup>(storage(), kManifestUrl,
                                                 storage()->NewGroupId());
    cache_ = base::MakeRefCounted<AppCache>(storage(), storage()->NewCacheId());
    cache_->set_manifest_parser_version(1);
    cache_->set_manifest_scope("/");
    cache_->AddEntry(kManifestUrl, AppCacheEntry(AppCacheEntry::MANIFEST,
                                                 /*response_id=*/1,
                                                 /*response_size=*/kTooBig,
                                                 /*padding_size=*/0));
    // Hold a ref to the cache to simulate the UpdateJob holding that ref,
    // and hold a ref to the group to simulate the CacheHost holding that ref.

    // Conduct the store test.
    storage()->StoreGroupAndNewestCache(group_.get(), cache_.get(), delegate());
    EXPECT_FALSE(delegate()->stored_group_success_);  // Expected to be async.
  }

  void FailStoreGroup_PaddingTooBig() {
    // Store a group and its newest cache. Should complete asynchronously.
    PushNextTask(base::BindOnce(&AppCacheStorageImplTest::Verify_FailStoreGroup,
                                base::Unretained(this)));

    // Set up some preconditions. Create a group and newest cache that
    // appear to be "unstored" and big enough to exceed the 5M limit.
    const int64_t kTooBig = 10 * 1024 * 1024;  // 10M
    group_ = base::MakeRefCounted<AppCacheGroup>(storage(), kManifestUrl,
                                                 storage()->NewGroupId());
    cache_ = base::MakeRefCounted<AppCache>(storage(), storage()->NewCacheId());
    cache_->set_manifest_parser_version(1);
    cache_->set_manifest_scope("/");
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT,
                                              /*response_id=*/1,
                                              /*response_size=*/1,
                                              /*padding_size=*/kTooBig));
    // Hold a ref to the cache to simulate the UpdateJob holding that ref,
    // and hold a ref to the group to simulate the CacheHost holding that ref.

    // Conduct the store test.
    storage()->StoreGroupAndNewestCache(group_.get(), cache_.get(), delegate());
    EXPECT_FALSE(delegate()->stored_group_success_);  // Expected to be async.
  }

  void Verify_FailStoreGroup() {
    EXPECT_FALSE(delegate()->stored_group_success_);
    EXPECT_TRUE(delegate()->would_exceed_quota_);

    // Should not have been stored in the database.
    AppCacheDatabase::GroupRecord group_record;
    AppCacheDatabase::CacheRecord cache_record;
    EXPECT_FALSE(database()->FindGroup(group_->group_id(), &group_record));
    EXPECT_FALSE(database()->FindCache(cache_->cache_id(), &cache_record));

    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_accessed_count_);
    EXPECT_EQ(0, mock_quota_manager_proxy_->notify_storage_modified_count_);

    TestFinished();
  }

  // MakeGroupObsolete  -------------------------------

  void MakeGroupObsolete() {
    // Make a group obsolete, should complete asynchronously.
    PushNextTask(
        base::BindOnce(&AppCacheStorageImplTest::Verify_MakeGroupObsolete,
                       base::Unretained(this)));

    // Setup some preconditions. Create a group and newest cache that
    // appears to be "stored" and "currently in use".
    MakeCacheAndGroup(kManifestUrl, 1, 1, invalid_token_expires(), true);
    EXPECT_EQ(kDefaultEntrySize + kDefaultEntryPadding,
              storage()->usage_map_[kOrigin]);

    // Also insert some related records.
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.flags = AppCacheEntry::FALLBACK;
    entry_record.response_id = 1;
    entry_record.url = kEntryUrl;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));

    AppCacheDatabase::NamespaceRecord fallback_namespace_record;
    fallback_namespace_record.cache_id = 1;
    fallback_namespace_record.namespace_.target_url = kEntryUrl;
    fallback_namespace_record.namespace_.namespace_url = kFallbackNamespace;
    fallback_namespace_record.origin = url::Origin::Create(kManifestUrl);
    EXPECT_TRUE(database()->InsertNamespace(&fallback_namespace_record));

    AppCacheDatabase::OnlineSafeListRecord online_safelist_record;
    online_safelist_record.cache_id = 1;
    online_safelist_record.namespace_url = kOnlineNamespace;
    EXPECT_TRUE(database()->InsertOnlineSafeList(&online_safelist_record));

    // Conduct the test.
    storage()->MakeGroupObsolete(group_.get(), delegate(), 0);
    EXPECT_FALSE(group_->is_obsolete());
  }

  void Verify_MakeGroupObsolete() {
    EXPECT_TRUE(delegate()->obsoleted_success_);
    EXPECT_EQ(group_.get(), delegate()->obsoleted_group_.get());
    EXPECT_TRUE(group_->is_obsolete());
    EXPECT_TRUE(storage()->usage_map_.empty());

    // The cache and group have been deleted from the database.
    AppCacheDatabase::GroupRecord group_record;
    AppCacheDatabase::CacheRecord cache_record;
    EXPECT_FALSE(database()->FindGroup(1, &group_record));
    EXPECT_FALSE(database()->FindCache(1, &cache_record));

    // The related records should have been deleted too.
    std::vector<AppCacheDatabase::EntryRecord> entry_records;
    database()->FindEntriesForCache(1, &entry_records);
    EXPECT_TRUE(entry_records.empty());
    std::vector<AppCacheDatabase::NamespaceRecord> intercept_records;
    std::vector<AppCacheDatabase::NamespaceRecord> fallback_records;
    database()->FindNamespacesForCache(1, &intercept_records,
                                       &fallback_records);
    EXPECT_TRUE(fallback_records.empty());
    std::vector<AppCacheDatabase::OnlineSafeListRecord> safelist_records;
    database()->FindOnlineSafeListForCache(1, &safelist_records);
    EXPECT_TRUE(safelist_records.empty());

    // Verify quota bookkeeping
    EXPECT_TRUE(storage()->usage_map_.empty());
    EXPECT_EQ(1, mock_quota_manager_proxy_->notify_storage_modified_count_);
    EXPECT_EQ(kOrigin, mock_quota_manager_proxy_->last_origin_);
    EXPECT_EQ(-(kDefaultEntrySize + kDefaultEntryPadding),
              mock_quota_manager_proxy_->last_delta_);

    TestFinished();
  }

  // MarkEntryAsForeign  -------------------------------

  void MarkEntryAsForeign() {
    // Setup some preconditions. Create a cache with an entry
    // in storage and in the working set.
    MakeCacheAndGroup(kManifestUrl, 1, 1, invalid_token_expires(), true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT));
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 0;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));
    EXPECT_FALSE(cache_->GetEntry(kEntryUrl)->IsForeign());

    // Conduct the test.
    storage()->MarkEntryAsForeign(kEntryUrl, 1);

    // The entry in the working set should have been updated syncly.
    EXPECT_TRUE(cache_->GetEntry(kEntryUrl)->IsForeign());
    EXPECT_TRUE(cache_->GetEntry(kEntryUrl)->IsExplicit());

    // And the entry in storage should also be updated, but that
    // happens asynchronously on the db thread.
    FlushAllTasks();
    AppCacheDatabase::EntryRecord entry_record2;
    EXPECT_TRUE(database()->FindEntry(1, kEntryUrl, &entry_record2));
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN,
              entry_record2.flags);
    TestFinished();
  }

  // MarkEntryAsForeignWithLoadInProgress  -------------------------------

  void MarkEntryAsForeignWithLoadInProgress() {
    PushNextTask(base::BindOnce(
        &AppCacheStorageImplTest::Verify_MarkEntryAsForeignWithLoadInProgress,
        base::Unretained(this)));

    // Setup some preconditions. Create a cache with an entry
    // in storage, but not in the working set.
    MakeCacheAndGroup(kManifestUrl, 1, 1, invalid_token_expires(), true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT));
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 0;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));
    EXPECT_FALSE(cache_->GetEntry(kEntryUrl)->IsForeign());
    EXPECT_TRUE(cache_->HasOneRef());
    cache_ = nullptr;
    group_ = nullptr;

    // Conduct the test, start a cache load, and prior to completion
    // of that load, mark the entry as foreign.
    storage()->LoadCache(1, delegate());
    storage()->MarkEntryAsForeign(kEntryUrl, 1);
  }

  void Verify_MarkEntryAsForeignWithLoadInProgress() {
    EXPECT_EQ(1, delegate()->loaded_cache_id_);
    EXPECT_TRUE(delegate()->loaded_cache_.get());

    // The entry in the working set should have been updated upon load.
    EXPECT_TRUE(delegate()->loaded_cache_->GetEntry(kEntryUrl)->IsForeign());
    EXPECT_TRUE(delegate()->loaded_cache_->GetEntry(kEntryUrl)->IsExplicit());

    // And the entry in storage should also be updated.
    FlushAllTasks();
    AppCacheDatabase::EntryRecord entry_record;
    EXPECT_TRUE(database()->FindEntry(1, kEntryUrl, &entry_record));
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN,
              entry_record.flags);
    TestFinished();
  }

  // FindNoMainResponse  -------------------------------

  void FindNoMainResponse() {
    PushNextTask(
        base::BindOnce(&AppCacheStorageImplTest::Verify_FindNoMainResponse,
                       base::Unretained(this)));

    // Conduct the test.
    storage()->FindResponseForMainRequest(kEntryUrl, GURL(), delegate());
    EXPECT_NE(kEntryUrl, delegate()->found_url_);
  }

  void Verify_FindNoMainResponse() {
    EXPECT_EQ(kEntryUrl, delegate()->found_url_);
    EXPECT_TRUE(delegate()->found_manifest_url_.is_empty());
    EXPECT_EQ(blink::mojom::kAppCacheNoCacheId, delegate()->found_cache_id_);
    EXPECT_EQ(blink::mojom::kAppCacheNoResponseId,
              delegate()->found_entry_.response_id());
    EXPECT_EQ(blink::mojom::kAppCacheNoResponseId,
              delegate()->found_fallback_entry_.response_id());
    EXPECT_TRUE(delegate()->found_namespace_entry_url_.is_empty());
    EXPECT_EQ(0, delegate()->found_entry_.types());
    EXPECT_EQ(0, delegate()->found_fallback_entry_.types());
    TestFinished();
  }

  // BasicFindMainResponse  -------------------------------

  void BasicFindMainResponseInDatabase() { BasicFindMainResponse(true); }

  void BasicFindMainResponseInWorkingSet() { BasicFindMainResponse(false); }

  void BasicFindMainResponse(bool drop_from_working_set) {
    PushNextTask(
        base::BindOnce(&AppCacheStorageImplTest::Verify_BasicFindMainResponse,
                       base::Unretained(this)));

    // Setup some preconditions. Create a complete cache with an entry
    // in storage.
    MakeCacheAndGroup(kManifestUrl, 2, 1, invalid_token_expires(), true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT, 1));
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 1;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));

    // Optionally drop the cache/group pair from the working set.
    if (drop_from_working_set) {
      EXPECT_TRUE(cache_->HasOneRef());
      cache_ = nullptr;
      EXPECT_TRUE(group_->HasOneRef());
      group_ = nullptr;
    }

    // Conduct the test.
    storage()->FindResponseForMainRequest(kEntryUrl, GURL(), delegate());
    EXPECT_NE(kEntryUrl, delegate()->found_url_);
  }

  void BasicFindMainResponse_OriginTrialFailure() {
    PushNextTask(
        base::BindOnce(&AppCacheStorageImplTest::Verify_FindNoMainResponse,
                       base::Unretained(this)));

    // Add cache/group/entry to the database.
    AddToDatabase(kManifestUrl, 2, 1, invalid_token_expires());
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 1;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));

    // Conduct the test.
    storage()->FindResponseForMainRequest(kEntryUrl, GURL(), delegate());
    EXPECT_NE(kEntryUrl, delegate()->found_url_);
  }

  void Verify_BasicFindMainResponse() {
    EXPECT_EQ(kEntryUrl, delegate()->found_url_);
    EXPECT_EQ(kManifestUrl, delegate()->found_manifest_url_);
    EXPECT_EQ(1, delegate()->found_cache_id_);
    EXPECT_EQ(2, delegate()->found_group_id_);
    EXPECT_EQ(1, delegate()->found_entry_.response_id());
    EXPECT_TRUE(delegate()->found_entry_.IsExplicit());
    EXPECT_FALSE(delegate()->found_fallback_entry_.has_response_id());
    TestFinished();
  }

  // BasicFindMainFallbackResponse  -------------------------------

  void BasicFindMainFallbackResponseInDatabase() {
    BasicFindMainFallbackResponse(true);
  }

  void BasicFindMainFallbackResponseInWorkingSet() {
    BasicFindMainFallbackResponse(false);
  }

  void BasicFindMainFallbackResponse(bool drop_from_working_set) {
    PushNextTask(base::BindOnce(
        &AppCacheStorageImplTest::Verify_BasicFindMainFallbackResponse,
        base::Unretained(this)));

    // Setup some preconditions. Create a complete cache with a
    // fallback namespace and entry.
    MakeCacheAndGroup(kManifestUrl, 2, 1, valid_token_expires(), true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::FALLBACK, 1));
    cache_->AddEntry(kEntryUrl2, AppCacheEntry(AppCacheEntry::FALLBACK, 2));
    cache_->fallback_namespaces_.emplace_back(APPCACHE_FALLBACK_NAMESPACE,
                                              kFallbackNamespace2, kEntryUrl2);
    cache_->fallback_namespaces_.emplace_back(APPCACHE_FALLBACK_NAMESPACE,
                                              kFallbackNamespace, kEntryUrl);
    AppCacheDatabase::CacheRecord cache_record;
    std::vector<AppCacheDatabase::EntryRecord> entries;
    std::vector<AppCacheDatabase::NamespaceRecord> intercepts;
    std::vector<AppCacheDatabase::NamespaceRecord> fallbacks;
    std::vector<AppCacheDatabase::OnlineSafeListRecord> safelists;
    cache_->ToDatabaseRecords(group_.get(), &cache_record, &entries,
                              &intercepts, &fallbacks, &safelists);

    for (const auto& entry : entries) {
      // MakeCacheAndGroup has inserted the default entry record already.
      if (entry.url != kDefaultEntryUrl)
        EXPECT_TRUE(database()->InsertEntry(&entry));
    }

    EXPECT_TRUE(database()->InsertNamespaceRecords(fallbacks));
    EXPECT_TRUE(database()->InsertOnlineSafeListRecords(safelists));
    if (drop_from_working_set) {
      EXPECT_TRUE(cache_->HasOneRef());
      cache_ = nullptr;
      EXPECT_TRUE(group_->HasOneRef());
      group_ = nullptr;
    }

    // Conduct the test. The test url is in both fallback namespace urls,
    // but should match the longer of the two.
    storage()->FindResponseForMainRequest(kFallbackTestUrl, GURL(), delegate());
    EXPECT_NE(kFallbackTestUrl, delegate()->found_url_);
  }

  void Verify_BasicFindMainFallbackResponse() {
    EXPECT_EQ(kFallbackTestUrl, delegate()->found_url_);
    EXPECT_EQ(kManifestUrl, delegate()->found_manifest_url_);
    EXPECT_EQ(1, delegate()->found_cache_id_);
    EXPECT_EQ(2, delegate()->found_group_id_);
    EXPECT_FALSE(delegate()->found_entry_.has_response_id());
    EXPECT_EQ(2, delegate()->found_fallback_entry_.response_id());
    EXPECT_EQ(kEntryUrl2, delegate()->found_namespace_entry_url_);
    EXPECT_TRUE(delegate()->found_fallback_entry_.IsFallback());
    TestFinished();
  }

  void FindMainFallbackResponse_OriginTrialFailure() {
    PushNextTask(base::BindLambdaForTesting([&]() {
      EXPECT_EQ(kFallbackTestUrl, delegate()->found_url_);
      EXPECT_TRUE(delegate()->found_manifest_url_.is_empty());
      EXPECT_EQ(0, delegate()->found_cache_id_);
      EXPECT_EQ(0, delegate()->found_group_id_);
      EXPECT_FALSE(delegate()->found_entry_.has_response_id());
      EXPECT_EQ(0, delegate()->found_fallback_entry_.response_id());
      EXPECT_TRUE(delegate()->found_namespace_entry_url_.is_empty());
      TestFinished();
    }));

    // Setup some preconditions. Create a complete cache with a
    // fallback namespace and entry.
    MakeCacheAndGroup(kManifestUrl, 2, 1, invalid_token_expires(), true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::FALLBACK, 1));
    cache_->AddEntry(kEntryUrl2, AppCacheEntry(AppCacheEntry::FALLBACK, 2));
    cache_->fallback_namespaces_.emplace_back(APPCACHE_FALLBACK_NAMESPACE,
                                              kFallbackNamespace2, kEntryUrl2);
    cache_->fallback_namespaces_.emplace_back(APPCACHE_FALLBACK_NAMESPACE,
                                              kFallbackNamespace, kEntryUrl);
    AppCacheDatabase::CacheRecord cache_record;
    std::vector<AppCacheDatabase::EntryRecord> entries;
    std::vector<AppCacheDatabase::NamespaceRecord> intercepts;
    std::vector<AppCacheDatabase::NamespaceRecord> fallbacks;
    std::vector<AppCacheDatabase::OnlineSafeListRecord> safelists;
    cache_->ToDatabaseRecords(group_.get(), &cache_record, &entries,
                              &intercepts, &fallbacks, &safelists);

    for (const auto& entry : entries) {
      // MakeCacheAndGroup has inserted the default entry record already.
      if (entry.url != kDefaultEntryUrl)
        EXPECT_TRUE(database()->InsertEntry(&entry));
    }

    cache_ = nullptr;
    group_ = nullptr;

    EXPECT_TRUE(database()->InsertNamespaceRecords(fallbacks));
    EXPECT_TRUE(database()->InsertOnlineSafeListRecords(safelists));

    // Conduct the test. Although the test url is in both fallback namespace
    // urls, it will match neither of them because its group does not have a
    // valid origin trial token.
    storage()->FindResponseForMainRequest(kFallbackTestUrl, GURL(), delegate());
    EXPECT_NE(kFallbackTestUrl, delegate()->found_url_);
  }

  // BasicFindMainInterceptResponse  -------------------------------

  void BasicFindMainInterceptResponseInDatabase() {
    PushNextTask(base::BindOnce(
        &AppCacheStorageImplTest::Verify_BasicFindMainInterceptResponse,
        base::Unretained(this)));
    BasicFindMainInterceptResponse(true, valid_token_expires());
  }

  void BasicFindMainInterceptResponseInWorkingSet() {
    PushNextTask(base::BindOnce(
        &AppCacheStorageImplTest::Verify_BasicFindMainInterceptResponse,
        base::Unretained(this)));
    BasicFindMainInterceptResponse(false, valid_token_expires());
  }

  void FindMainInterceptResponse_OriginTrialFailure() {
    PushNextTask(base::BindLambdaForTesting([&]() {
      EXPECT_EQ(kInterceptTestUrl, delegate()->found_url_);
      EXPECT_TRUE(delegate()->found_manifest_url_.is_empty());
      EXPECT_EQ(0, delegate()->found_cache_id_);
      EXPECT_EQ(0, delegate()->found_group_id_);
      EXPECT_FALSE(delegate()->found_entry_.has_response_id());
      EXPECT_EQ(0, delegate()->found_fallback_entry_.response_id());
      EXPECT_TRUE(delegate()->found_namespace_entry_url_.is_empty());
      TestFinished();
    }));
    BasicFindMainInterceptResponse(true, invalid_token_expires());
  }

  void BasicFindMainInterceptResponse(bool drop_from_working_set,
                                      base::Time token_expires) {
    // Setup some preconditions. Create a complete cache with an
    // intercept namespace and entry.
    MakeCacheAndGroup(kManifestUrl, 2, 1, token_expires, true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::INTERCEPT, 1));
    cache_->AddEntry(kEntryUrl2, AppCacheEntry(AppCacheEntry::INTERCEPT, 2));
    cache_->intercept_namespaces_.emplace_back(
        APPCACHE_INTERCEPT_NAMESPACE, kInterceptNamespace2, kEntryUrl2);
    cache_->intercept_namespaces_.emplace_back(APPCACHE_INTERCEPT_NAMESPACE,
                                               kInterceptNamespace, kEntryUrl);
    AppCacheDatabase::CacheRecord cache_record;
    std::vector<AppCacheDatabase::EntryRecord> entries;
    std::vector<AppCacheDatabase::NamespaceRecord> intercepts;
    std::vector<AppCacheDatabase::NamespaceRecord> fallbacks;
    std::vector<AppCacheDatabase::OnlineSafeListRecord> safelists;
    cache_->ToDatabaseRecords(group_.get(), &cache_record, &entries,
                              &intercepts, &fallbacks, &safelists);

    for (const auto& entry : entries) {
      // MakeCacheAndGroup has inserted  the default entry record already
      if (entry.url != kDefaultEntryUrl)
        EXPECT_TRUE(database()->InsertEntry(&entry));
    }

    EXPECT_TRUE(database()->InsertNamespaceRecords(intercepts));
    EXPECT_TRUE(database()->InsertOnlineSafeListRecords(safelists));
    if (drop_from_working_set) {
      EXPECT_TRUE(cache_->HasOneRef());
      cache_ = nullptr;
      EXPECT_TRUE(group_->HasOneRef());
      group_ = nullptr;
    }

    // Conduct the test. The test url is in both intercept namespaces,
    // but should match the longer of the two.
    storage()->FindResponseForMainRequest(kInterceptTestUrl, GURL(),
                                          delegate());
    EXPECT_NE(kInterceptTestUrl, delegate()->found_url_);
  }

  void Verify_BasicFindMainInterceptResponse() {
    EXPECT_EQ(kInterceptTestUrl, delegate()->found_url_);
    EXPECT_EQ(kManifestUrl, delegate()->found_manifest_url_);
    EXPECT_EQ(1, delegate()->found_cache_id_);
    EXPECT_EQ(2, delegate()->found_group_id_);
    EXPECT_EQ(2, delegate()->found_entry_.response_id());
    EXPECT_TRUE(delegate()->found_entry_.IsIntercept());
    EXPECT_EQ(kEntryUrl2, delegate()->found_namespace_entry_url_);
    EXPECT_FALSE(delegate()->found_fallback_entry_.has_response_id());
    TestFinished();
  }

  // FindMainResponseWithMultipleHits  -------------------------------

  void FindMainResponseWithMultipleHits() {
    PushNextTask(base::BindOnce(
        &AppCacheStorageImplTest::Verify_FindMainResponseWithMultipleHits,
        base::Unretained(this)));

    // Setup some preconditions, create a few caches with an identical set
    // of entries and fallback namespaces. Only the last one remains in
    // the working set to simulate appearing as "in use".
    MakeMultipleHitCacheAndGroup(kManifestUrl, 1);
    MakeMultipleHitCacheAndGroup(kManifestUrl2, 2);
    MakeMultipleHitCacheAndGroup(kManifestUrl3, 3);

    // Conduct the test, we should find the response from the last cache
    // since it's "in use".
    storage()->FindResponseForMainRequest(kEntryUrl, GURL(), delegate());
    EXPECT_NE(kEntryUrl, delegate()->found_url_);
  }

  void MakeMultipleHitCacheAndGroup(const GURL& manifest_url, int id) {
    MakeCacheAndGroup(manifest_url, id, id, invalid_token_expires(), true);
    AppCacheDatabase::EntryRecord entry_record;

    // Add an entry for kEntryUrl
    entry_record.cache_id = id;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = id;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));
    cache_->AddEntry(entry_record.url, AppCacheEntry(entry_record.flags,
                                                     entry_record.response_id));

    // Add an entry for the manifestUrl
    entry_record.cache_id = id;
    entry_record.url = manifest_url;
    entry_record.flags = AppCacheEntry::MANIFEST;
    entry_record.response_id = id + kManifestEntryIdOffset;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));
    cache_->AddEntry(entry_record.url, AppCacheEntry(entry_record.flags,
                                                     entry_record.response_id));

    // Add a fallback entry and namespace
    entry_record.cache_id = id;
    entry_record.url = kEntryUrl2;
    entry_record.flags = AppCacheEntry::FALLBACK;
    entry_record.response_id = id + kFallbackEntryIdOffset;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));
    cache_->AddEntry(entry_record.url, AppCacheEntry(entry_record.flags,
                                                     entry_record.response_id));
    AppCacheDatabase::NamespaceRecord fallback_namespace_record;
    fallback_namespace_record.cache_id = id;
    fallback_namespace_record.namespace_.target_url = entry_record.url;
    fallback_namespace_record.namespace_.namespace_url = kFallbackNamespace;
    fallback_namespace_record.origin = url::Origin::Create(manifest_url);
    EXPECT_TRUE(database()->InsertNamespace(&fallback_namespace_record));
    cache_->fallback_namespaces_.emplace_back(APPCACHE_FALLBACK_NAMESPACE,
                                              kFallbackNamespace, kEntryUrl2);
  }

  void Verify_FindMainResponseWithMultipleHits() {
    EXPECT_EQ(kEntryUrl, delegate()->found_url_);
    EXPECT_EQ(kManifestUrl3, delegate()->found_manifest_url_);
    EXPECT_EQ(3, delegate()->found_cache_id_);
    EXPECT_EQ(3, delegate()->found_group_id_);
    EXPECT_EQ(3, delegate()->found_entry_.response_id());
    EXPECT_TRUE(delegate()->found_entry_.IsExplicit());
    EXPECT_FALSE(delegate()->found_fallback_entry_.has_response_id());

    // Conduct another test preferring kManifestUrl
    delegate_ = std::make_unique<MockStorageDelegate>(this);
    PushNextTask(base::BindOnce(
        &AppCacheStorageImplTest::Verify_FindMainResponseWithMultipleHits2,
        base::Unretained(this)));
    storage()->FindResponseForMainRequest(kEntryUrl, kManifestUrl, delegate());
    EXPECT_NE(kEntryUrl, delegate()->found_url_);
  }

  void Verify_FindMainResponseWithMultipleHits2() {
    EXPECT_EQ(kEntryUrl, delegate()->found_url_);
    EXPECT_EQ(kManifestUrl, delegate()->found_manifest_url_);
    EXPECT_EQ(1, delegate()->found_cache_id_);
    EXPECT_EQ(1, delegate()->found_group_id_);
    EXPECT_EQ(1, delegate()->found_entry_.response_id());
    EXPECT_TRUE(delegate()->found_entry_.IsExplicit());
    EXPECT_FALSE(delegate()->found_fallback_entry_.has_response_id());

    // Conduct the another test preferring kManifestUrl2
    delegate_ = std::make_unique<MockStorageDelegate>(this);
    PushNextTask(base::BindOnce(
        &AppCacheStorageImplTest::Verify_FindMainResponseWithMultipleHits3,
        base::Unretained(this)));
    storage()->FindResponseForMainRequest(kEntryUrl, kManifestUrl2, delegate());
    EXPECT_NE(kEntryUrl, delegate()->found_url_);
  }

  void Verify_FindMainResponseWithMultipleHits3() {
    EXPECT_EQ(kEntryUrl, delegate()->found_url_);
    EXPECT_EQ(kManifestUrl2, delegate()->found_manifest_url_);
    EXPECT_EQ(2, delegate()->found_cache_id_);
    EXPECT_EQ(2, delegate()->found_group_id_);
    EXPECT_EQ(2, delegate()->found_entry_.response_id());
    EXPECT_TRUE(delegate()->found_entry_.IsExplicit());
    EXPECT_FALSE(delegate()->found_fallback_entry_.has_response_id());

    // Conduct another test with no preferred manifest that hits the fallback.
    delegate_ = std::make_unique<MockStorageDelegate>(this);
    PushNextTask(base::BindOnce(
        &AppCacheStorageImplTest::Verify_FindMainResponseWithMultipleHits4,
        base::Unretained(this)));
    storage()->FindResponseForMainRequest(kFallbackTestUrl, GURL(), delegate());
    EXPECT_NE(kFallbackTestUrl, delegate()->found_url_);
  }

  void Verify_FindMainResponseWithMultipleHits4() {
    EXPECT_EQ(kFallbackTestUrl, delegate()->found_url_);
    EXPECT_EQ(kManifestUrl3, delegate()->found_manifest_url_);
    EXPECT_EQ(3, delegate()->found_cache_id_);
    EXPECT_EQ(3, delegate()->found_group_id_);
    EXPECT_FALSE(delegate()->found_entry_.has_response_id());
    EXPECT_EQ(3 + kFallbackEntryIdOffset,
              delegate()->found_fallback_entry_.response_id());
    EXPECT_TRUE(delegate()->found_fallback_entry_.IsFallback());
    EXPECT_EQ(kEntryUrl2, delegate()->found_namespace_entry_url_);

    // Conduct another test preferring kManifestUrl2 that hits the fallback.
    delegate_ = std::make_unique<MockStorageDelegate>(this);
    PushNextTask(base::BindOnce(
        &AppCacheStorageImplTest::Verify_FindMainResponseWithMultipleHits5,
        base::Unretained(this)));
    storage()->FindResponseForMainRequest(kFallbackTestUrl, kManifestUrl2,
                                          delegate());
    EXPECT_NE(kFallbackTestUrl, delegate()->found_url_);
  }

  void Verify_FindMainResponseWithMultipleHits5() {
    EXPECT_EQ(kFallbackTestUrl, delegate()->found_url_);
    EXPECT_EQ(kManifestUrl2, delegate()->found_manifest_url_);
    EXPECT_EQ(2, delegate()->found_cache_id_);
    EXPECT_EQ(2, delegate()->found_group_id_);
    EXPECT_FALSE(delegate()->found_entry_.has_response_id());
    EXPECT_EQ(2 + kFallbackEntryIdOffset,
              delegate()->found_fallback_entry_.response_id());
    EXPECT_TRUE(delegate()->found_fallback_entry_.IsFallback());
    EXPECT_EQ(kEntryUrl2, delegate()->found_namespace_entry_url_);

    TestFinished();
  }

  // FindMainResponseExclusions  -------------------------------

  void FindMainResponseExclusionsInDatabase() {
    FindMainResponseExclusions(true);
  }

  void FindMainResponseExclusionsInWorkingSet() {
    FindMainResponseExclusions(false);
  }

  void FindMainResponseExclusions(bool drop_from_working_set) {
    // Setup some preconditions. Create a complete cache with a
    // foreign entry, an online namespace, and a second online
    // namespace nested within a fallback namespace.
    MakeCacheAndGroup(kManifestUrl, 1, 1, invalid_token_expires(), true);
    cache_->AddEntry(
        kEntryUrl,
        AppCacheEntry(AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN, 1));
    cache_->AddEntry(kEntryUrl2, AppCacheEntry(AppCacheEntry::FALLBACK, 2));
    cache_->fallback_namespaces_.emplace_back(APPCACHE_FALLBACK_NAMESPACE,
                                              kFallbackNamespace, kEntryUrl2);
    cache_->online_safelist_namespaces_.emplace_back(APPCACHE_NETWORK_NAMESPACE,
                                                     kOnlineNamespace, GURL());
    cache_->online_safelist_namespaces_.emplace_back(
        APPCACHE_NETWORK_NAMESPACE, kOnlineNamespaceWithinFallback, GURL());

    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN;
    entry_record.response_id = 1;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));
    AppCacheDatabase::OnlineSafeListRecord safelist_record;
    safelist_record.cache_id = 1;
    safelist_record.namespace_url = kOnlineNamespace;
    EXPECT_TRUE(database()->InsertOnlineSafeList(&safelist_record));
    AppCacheDatabase::NamespaceRecord fallback_namespace_record;
    fallback_namespace_record.cache_id = 1;
    fallback_namespace_record.namespace_.target_url = kEntryUrl2;
    fallback_namespace_record.namespace_.namespace_url = kFallbackNamespace;
    fallback_namespace_record.origin = url::Origin::Create(kManifestUrl);
    EXPECT_TRUE(database()->InsertNamespace(&fallback_namespace_record));
    safelist_record.cache_id = 1;
    safelist_record.namespace_url = kOnlineNamespaceWithinFallback;
    EXPECT_TRUE(database()->InsertOnlineSafeList(&safelist_record));
    if (drop_from_working_set) {
      cache_ = nullptr;
      group_ = nullptr;
    }

    // We should not find anything for the foreign entry.
    PushNextTask(
        base::BindOnce(&AppCacheStorageImplTest::Verify_ExclusionNotFound,
                       base::Unretained(this), kEntryUrl, 1));
    storage()->FindResponseForMainRequest(kEntryUrl, GURL(), delegate());
  }

  void Verify_ExclusionNotFound(GURL expected_url, int phase) {
    EXPECT_EQ(expected_url, delegate()->found_url_);
    EXPECT_TRUE(delegate()->found_manifest_url_.is_empty());
    EXPECT_EQ(blink::mojom::kAppCacheNoCacheId, delegate()->found_cache_id_);
    EXPECT_EQ(0, delegate()->found_group_id_);
    EXPECT_EQ(blink::mojom::kAppCacheNoResponseId,
              delegate()->found_entry_.response_id());
    EXPECT_EQ(blink::mojom::kAppCacheNoResponseId,
              delegate()->found_fallback_entry_.response_id());
    EXPECT_TRUE(delegate()->found_namespace_entry_url_.is_empty());
    EXPECT_EQ(0, delegate()->found_entry_.types());
    EXPECT_EQ(0, delegate()->found_fallback_entry_.types());

    if (phase == 1) {
      // We should not find anything for the online namespace.
      PushNextTask(
          base::BindOnce(&AppCacheStorageImplTest::Verify_ExclusionNotFound,
                         base::Unretained(this), kOnlineNamespace, 2));
      storage()->FindResponseForMainRequest(kOnlineNamespace, GURL(),
                                            delegate());
      return;
    }
    if (phase == 2) {
      // We should not find anything for the online namespace nested within
      // the fallback namespace.
      PushNextTask(base::BindOnce(
          &AppCacheStorageImplTest::Verify_ExclusionNotFound,
          base::Unretained(this), kOnlineNamespaceWithinFallback, 3));
      storage()->FindResponseForMainRequest(kOnlineNamespaceWithinFallback,
                                            GURL(), delegate());
      return;
    }

    TestFinished();
  }

  // Reinitialize -------------------------------
  // These tests are somewhat of a system integration test.
  // They rely on running a mock http server on our IO thread,
  // and involves other appcache classes to get some code
  // coverage thruout when Reinitialize happens.

  class MockServiceObserver : public AppCacheServiceImpl::Observer {
   public:
    explicit MockServiceObserver(AppCacheStorageImplTest* test) : test_(test) {}

    void OnServiceReinitialized(
        AppCacheStorageReference* old_storage_ref) override {
      observed_old_storage_ = old_storage_ref;
      test_->ScheduleNextTask();
    }

    scoped_refptr<AppCacheStorageReference> observed_old_storage_;
    AppCacheStorageImplTest* test_;
  };

  class MockAppCacheFrontend : public blink::mojom::AppCacheFrontend {
   public:
    MockAppCacheFrontend() = default;

    void CacheSelected(blink::mojom::AppCacheInfoPtr info) override {}
    void EventRaised(blink::mojom::AppCacheEventID event_id) override {}
    void ProgressEventRaised(const GURL& url,
                             int32_t num_total,
                             int32_t num_complete) override {}
    void ErrorEventRaised(
        blink::mojom::AppCacheErrorDetailsPtr details) override {
      error_event_was_raised_ = true;
    }
    void LogMessage(blink::mojom::ConsoleMessageLevel log_level,
                    const std::string& message) override {}
    void SetSubresourceFactory(
        mojo::PendingRemote<network::mojom::URLLoaderFactory>
            url_loader_factory) override {}

    bool error_event_was_raised_ = false;
  };

  enum ReinitTestCase {
    CORRUPT_CACHE_ON_INSTALL,
    CORRUPT_CACHE_ON_LOAD_EXISTING,
    CORRUPT_SQL_ON_INSTALL
  };

  void Reinitialize1() {
    // Recover from a corrupt disk cache discovered while
    // installing a new appcache.
    Reinitialize(CORRUPT_CACHE_ON_INSTALL);
  }

  void Reinitialize2() {
    // Recover from a corrupt disk cache discovered while
    // trying to load a resource from an existing appcache.
    Reinitialize(CORRUPT_CACHE_ON_LOAD_EXISTING);
  }

  void Reinitialize3() {
    // Recover from a corrupt sql database discovered while
    // installing a new appcache.
    Reinitialize(CORRUPT_SQL_ON_INSTALL);
  }

  void Reinitialize(ReinitTestCase test_case) {
    // Unlike all of the other tests, this one actually read/write files.
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

    AppCacheDatabase db(temp_directory_.GetPath().AppendASCII("Index"));
    EXPECT_TRUE(db.LazyOpen(true));

    if (test_case == CORRUPT_CACHE_ON_INSTALL ||
        test_case == CORRUPT_CACHE_ON_LOAD_EXISTING) {
      // Create a corrupt/unopenable disk_cache index file.
      const std::string kCorruptData("deadbeef");
      base::FilePath disk_cache_directory =
          temp_directory_.GetPath().AppendASCII("Cache");
      ASSERT_TRUE(base::CreateDirectory(disk_cache_directory));
      base::FilePath index_file = disk_cache_directory.AppendASCII("index");
      EXPECT_TRUE(base::WriteFile(index_file, kCorruptData));

      // Also add a corrupt entry file so that simple disk_cache does not try
      // to automatically recover from the corrupted index.
      base::FilePath entry_file =
          disk_cache_directory.AppendASCII("01234567_0");
      EXPECT_TRUE(base::WriteFile(entry_file, kCorruptData));
    }

    // Create records for a degenerate cached manifest that only contains
    // one entry for the manifest file resource.
    if (test_case == CORRUPT_CACHE_ON_LOAD_EXISTING) {
      AppCacheDatabase db(temp_directory_.GetPath().AppendASCII("Index"));
      GURL manifest_url = GetMockUrl("manifest");

      AppCacheDatabase::GroupRecord group_record;
      group_record.group_id = 1;
      group_record.manifest_url = manifest_url;
      group_record.origin = url::Origin::Create(manifest_url);
      EXPECT_TRUE(db.InsertGroup(&group_record));
      AppCacheDatabase::CacheRecord cache_record;
      cache_record.cache_id = 1;
      cache_record.group_id = 1;
      cache_record.online_wildcard = false;
      cache_record.update_time = kZeroTime;
      cache_record.cache_size = kDefaultEntrySize;
      cache_record.padding_size = 0;
      cache_record.manifest_parser_version = 1;
      cache_record.manifest_scope = std::string("/");
      EXPECT_TRUE(db.InsertCache(&cache_record));
      AppCacheDatabase::EntryRecord entry_record;
      entry_record.cache_id = 1;
      entry_record.url = manifest_url;
      entry_record.flags = AppCacheEntry::MANIFEST;
      entry_record.response_id = 1;
      entry_record.response_size = kDefaultEntrySize;
      entry_record.padding_size = 0;
      EXPECT_TRUE(db.InsertEntry(&entry_record));
    }

    // Recreate the service to point at the db and corruption on disk.
    service_ = std::make_unique<AppCacheServiceImpl>(
        nullptr, weak_partition_factory_.GetWeakPtr());

    service_->set_appcache_policy(&mock_policy_);
    service_->Initialize(temp_directory_.GetPath());
    mock_quota_manager_proxy_ = base::MakeRefCounted<MockQuotaManagerProxy>();
    service_->quota_manager_proxy_ = mock_quota_manager_proxy_;
    delegate_ = std::make_unique<MockStorageDelegate>(this);

    // Additional setup to observe reinitailize happens.
    observer_ = std::make_unique<MockServiceObserver>(this);
    service_->AddObserver(observer_.get());

    // We continue after the init task is complete including the callback
    // on the current thread.
    FlushAllTasks();
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheStorageImplTest::Continue_Reinitialize,
                       base::Unretained(this), test_case));
  }

  void Continue_Reinitialize(ReinitTestCase test_case) {
    const int kMockRenderFrameId = MSG_ROUTING_NONE;
    if (test_case == CORRUPT_SQL_ON_INSTALL) {
      // Break the db file
      EXPECT_FALSE(database()->was_corruption_detected());
      ASSERT_TRUE(sql::test::CorruptSizeInHeader(
          temp_directory_.GetPath().AppendASCII("Index")));
    }

    if (test_case == CORRUPT_CACHE_ON_INSTALL ||
        test_case == CORRUPT_SQL_ON_INSTALL) {
      // Try to create a new appcache, the resulting update job will
      // eventually fail when it gets to disk cache initialization.
      host1_id_ = base::UnguessableToken::Create();
      service_->RegisterHost(
          host_remote_.BindNewPipeAndPassReceiver(), BindFrontend(), host1_id_,
          kMockRenderFrameId, kMockProcessId,
          ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
              kMockProcessId),
          GetBadMessageCallback());
      AppCacheHost* host1 = service_->GetHost(host1_id_);
      const GURL kEmptyPageUrl(GetMockUrl("empty.html"));
      host1->SetSiteForCookiesForTesting(
          net::SiteForCookies::FromUrl(kEmptyPageUrl));
      host1->SelectCache(kEmptyPageUrl, blink::mojom::kAppCacheNoCacheId,
                         GetMockUrl("manifest"));
    } else {
      ASSERT_EQ(CORRUPT_CACHE_ON_LOAD_EXISTING, test_case);
      // Try to access the existing cache manifest.
      // The URLRequestJob  will eventually fail when it gets to disk
      // cache initialization.
      host2_id_ = base::UnguessableToken::Create();
      service_->RegisterHost(
          host_remote_.BindNewPipeAndPassReceiver(), BindFrontend(), host2_id_,
          kMockRenderFrameId, kMockProcessId,
          ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
              kMockProcessId),
          GetBadMessageCallback());
      AppCacheHost* host2 = service_->GetHost(host2_id_);
      network::ResourceRequest request;
      request.url = GetMockUrl("manifest");
      handler_ = host2->CreateRequestHandler(
          std::make_unique<AppCacheRequest>(request),
          blink::mojom::ResourceType::kMainFrame, false);
      handler_->MaybeCreateLoader(request, nullptr, base::DoNothing(),
                                  base::DoNothing());
    }

    PushNextTask(base::BindOnce(&AppCacheStorageImplTest::Verify_Reinitialized,
                                base::Unretained(this), test_case));
  }

  void Verify_Reinitialized(ReinitTestCase test_case) {
    // Verify we got notified of reinit and a new storage instance is created,
    // and that the old data has been deleted.
    EXPECT_TRUE(observer_->observed_old_storage_.get());
    EXPECT_TRUE(observer_->observed_old_storage_->storage() != storage());
    EXPECT_FALSE(PathExists(
        temp_directory_.GetPath().AppendASCII("Cache").AppendASCII("index")));
    EXPECT_FALSE(PathExists(temp_directory_.GetPath().AppendASCII("Index")));

    if (test_case == CORRUPT_SQL_ON_INSTALL) {
      AppCacheStorageImpl* storage = static_cast<AppCacheStorageImpl*>(
          observer_->observed_old_storage_->storage());
      EXPECT_TRUE(storage->database_->was_corruption_detected());
    }

    // Verify that the hosts saw appropriate events.
    if (test_case == CORRUPT_CACHE_ON_INSTALL ||
        test_case == CORRUPT_SQL_ON_INSTALL) {
      EXPECT_TRUE(frontend_.error_event_was_raised_);
      AppCacheHost* host1 = service_->GetHost(host1_id_);
      EXPECT_FALSE(host1->associated_cache());
      EXPECT_FALSE(host1->group_being_updated_.get());
      EXPECT_TRUE(host1->disabled_storage_reference_.get());
    } else {
      ASSERT_EQ(CORRUPT_CACHE_ON_LOAD_EXISTING, test_case);
      AppCacheHost* host2 = service_->GetHost(host2_id_);
      EXPECT_TRUE(host2->disabled_storage_reference_.get());
    }

    // Cleanup and claim victory.
    service_->EraseHost(host1_id_);
    service_->EraseHost(host2_id_);
    service_->RemoveObserver(observer_.get());
    handler_.reset();
    observer_.reset();
    TestFinished();
  }

  // Test case helpers --------------------------------------------------

  AppCacheServiceImpl* service() { return service_.get(); }

  AppCacheStorageImpl* storage() {
    return static_cast<AppCacheStorageImpl*>(service()->storage());
  }

  AppCacheDatabase* database() { return storage()->database_.get(); }

  MockStorageDelegate* delegate() { return delegate_.get(); }

  mojo::PendingRemote<blink::mojom::AppCacheFrontend> BindFrontend() {
    mojo::PendingRemote<blink::mojom::AppCacheFrontend> result;
    frontend_receivers_.Add(&frontend_,
                            result.InitWithNewPipeAndPassReceiver());
    return result;
  }

  mojo::ReportBadMessageCallback GetBadMessageCallback() {
    return base::BindOnce(&AppCacheStorageImplTest::OnBadMessage,
                          base::Unretained(this));
  }

  void OnBadMessage(const std::string& reason) { NOTREACHED(); }

  static base::Time invalid_token_expires() { return base::Time(); }

  static base::Time valid_token_expires() {
    return base::Time::Now() + base::TimeDelta::FromDays(10);
  }

  void MakeCacheAndGroup(const GURL& manifest_url,
                         int64_t group_id,
                         int64_t cache_id,
                         base::Time token_expires,
                         bool add_to_database) {
    AppCacheEntry default_entry(AppCacheEntry::EXPLICIT,
                                cache_id + kDefaultEntryIdOffset,
                                kDefaultEntrySize, kDefaultEntryPadding);
    group_ =
        base::MakeRefCounted<AppCacheGroup>(storage(), manifest_url, group_id);
    cache_ = base::MakeRefCounted<AppCache>(storage(), cache_id);
    cache_->AddEntry(kDefaultEntryUrl, default_entry);
    cache_->set_complete(true);
    group_->AddCache(cache_.get());
    if (add_to_database)
      AddToDatabase(manifest_url, group_id, cache_id, token_expires);
  }

  void AddToDatabase(const GURL& manifest_url,
                     int64_t group_id,
                     int64_t cache_id,
                     base::Time token_expires) {
    url::Origin manifest_origin(url::Origin::Create(manifest_url));
    AppCacheEntry default_entry(AppCacheEntry::EXPLICIT,
                                cache_id + kDefaultEntryIdOffset,
                                kDefaultEntrySize, kDefaultEntryPadding);

    AppCacheDatabase::GroupRecord group_record;
    group_record.group_id = group_id;
    group_record.manifest_url = manifest_url;
    group_record.origin = manifest_origin;
    EXPECT_TRUE(database()->InsertGroup(&group_record));
    AppCacheDatabase::CacheRecord cache_record;
    cache_record.cache_id = cache_id;
    cache_record.group_id = group_id;
    cache_record.online_wildcard = false;
    cache_record.update_time = kZeroTime;
    cache_record.cache_size = kDefaultEntrySize;
    cache_record.padding_size = kDefaultEntryPadding;
    cache_record.manifest_parser_version = 1;
    cache_record.manifest_scope = std::string("/");
    cache_record.token_expires = token_expires;
    EXPECT_TRUE(database()->InsertCache(&cache_record));
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = cache_id;
    entry_record.url = kDefaultEntryUrl;
    entry_record.flags = default_entry.types();
    entry_record.response_id = default_entry.response_id();
    entry_record.response_size = default_entry.response_size();
    entry_record.padding_size = default_entry.padding_size();
    EXPECT_TRUE(database()->InsertEntry(&entry_record));

    storage()->usage_map_[manifest_origin] =
        default_entry.response_size() + default_entry.padding_size();
  }

  // Data members --------------------------------------------------
  BrowserTaskEnvironment task_environment_;

  base::OnceClosure test_finished_cb_;
  base::stack<base::OnceClosure> task_stack_;
  MockAppCachePolicy mock_policy_;
  std::unique_ptr<AppCacheServiceImpl> service_;
  std::unique_ptr<MockStorageDelegate> delegate_;
  scoped_refptr<MockQuotaManagerProxy> mock_quota_manager_proxy_;
  scoped_refptr<AppCacheGroup> group_;
  scoped_refptr<AppCache> cache_;
  scoped_refptr<AppCache> cache2_;

  base::UnguessableToken host1_id_;
  base::UnguessableToken host2_id_;
  mojo::Remote<blink::mojom::AppCacheHost> host_remote_;

  // Specifically for the Reinitalize test.
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<MockServiceObserver> observer_;
  MockAppCacheFrontend frontend_;
  mojo::ReceiverSet<blink::mojom::AppCacheFrontend> frontend_receivers_;
  std::unique_ptr<AppCacheRequestHandler> handler_;
  URLLoaderInterceptor interceptor_;
  TestBrowserContext browser_context_;
  base::test::ScopedFeatureList appcache_require_origin_trial_feature_;
  base::WeakPtrFactory<StoragePartitionImpl> weak_partition_factory_;

  // Test data
  const base::Time kZeroTime;
  const GURL kManifestUrl = GURL("http://blah/manifest");
  const GURL kManifestUrl2 = GURL("http://blah/manifest2");
  const GURL kManifestUrl3 = GURL("http://blah/manifest3");
  const GURL kEntryUrl = GURL("http://blah/entry");
  const GURL kEntryUrl2 = GURL("http://blah/entry2");
  const GURL kFallbackNamespace = GURL("http://blah/fallback_namespace/");
  const GURL kFallbackNamespace2 =
      GURL("http://blah/fallback_namespace/longer");
  const GURL kFallbackTestUrl =
      GURL("http://blah/fallback_namespace/longer/test");
  const GURL kOnlineNamespace = GURL("http://blah/online_namespace");
  const GURL kOnlineNamespaceWithinFallback =
      GURL("http://blah/fallback_namespace/online/");
  const GURL kInterceptNamespace = GURL("http://blah/intercept_namespace/");
  const GURL kInterceptNamespace2 =
      GURL("http://blah/intercept_namespace/longer/");
  const GURL kInterceptTestUrl =
      GURL("http://blah/intercept_namespace/longer/test");
  const GURL kInterceptPatternNamespace =
      GURL("http://blah/intercept_pattern/*/bar");
  const GURL kInterceptPatternTestPositiveUrl =
      GURL("http://blah/intercept_pattern/foo/bar");
  const GURL kInterceptPatternTestNegativeUrl =
      GURL("http://blah/intercept_pattern/foo/not_bar");
  const GURL kFallbackPatternNamespace =
      GURL("http://blah/fallback_pattern/*/bar");
  const GURL kFallbackPatternTestPositiveUrl =
      GURL("http://blah/fallback_pattern/foo/bar");
  const GURL kFallbackPatternTestNegativeUrl =
      GURL("http://blah/fallback_pattern/foo/not_bar");
  const url::Origin kOrigin = url::Origin::Create(kManifestUrl.GetOrigin());

  const int kManifestEntryIdOffset = 100;
  const int kFallbackEntryIdOffset = 1000;

  const GURL kDefaultEntryUrl =
      GURL("http://blah/makecacheandgroup_default_entry");
  const int kDefaultEntrySize = 10;
  const int kDefaultEntryPadding = 10000;
  const int kDefaultEntryIdOffset = 12345;
};

TEST_F(AppCacheStorageImplTest, LoadCache_Miss) {
  RunTestOnUIThread(&AppCacheStorageImplTest::LoadCache_Miss);
}

TEST_F(AppCacheStorageImplTest, LoadCache_NearHit) {
  RunTestOnUIThread(&AppCacheStorageImplTest::LoadCache_NearHit);
}

TEST_F(AppCacheStorageImplTest, LoadCache_OriginTrialSuccess) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kAppCacheRequireOriginTrial);
  RunTestOnUIThread(&AppCacheStorageImplTest::LoadCache_OriginTrialSuccess);
}

TEST_F(AppCacheStorageImplTest, LoadCache_OriginTrialFailure) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kAppCacheRequireOriginTrial);
  RunTestOnUIThread(&AppCacheStorageImplTest::LoadCache_OriginTrialFailure);
}

TEST_F(AppCacheStorageImplTest, CreateGroupInEmptyOrigin) {
  RunTestOnUIThread(&AppCacheStorageImplTest::CreateGroupInEmptyOrigin);
}

TEST_F(AppCacheStorageImplTest, CreateGroupInPopulatedOrigin) {
  RunTestOnUIThread(&AppCacheStorageImplTest::CreateGroupInPopulatedOrigin);
}

TEST_F(AppCacheStorageImplTest, LoadGroupAndCache_FarHit) {
  RunTestOnUIThread(&AppCacheStorageImplTest::LoadGroupAndCache_FarHit);
}

TEST_F(AppCacheStorageImplTest, LoadGroupAndCache_OriginTrialSuccess) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kAppCacheRequireOriginTrial);
  RunTestOnUIThread(&AppCacheStorageImplTest::LoadGroupAndCache_FarHit);
}

TEST_F(AppCacheStorageImplTest, StoreNewGroup) {
  RunTestOnUIThread(&AppCacheStorageImplTest::StoreNewGroup);
}

TEST_F(AppCacheStorageImplTest, StoreExistingGroup) {
  RunTestOnUIThread(&AppCacheStorageImplTest::StoreExistingGroup);
}

TEST_F(AppCacheStorageImplTest, StoreExistingGroupExistingCache) {
  RunTestOnUIThread(&AppCacheStorageImplTest::StoreExistingGroupExistingCache);
}

TEST_F(AppCacheStorageImplTest, FailStoreGroup_SizeTooBig) {
  RunTestOnUIThread(&AppCacheStorageImplTest::FailStoreGroup_SizeTooBig);
}

TEST_F(AppCacheStorageImplTest, FailStoreGroup_PaddingTooBig) {
  RunTestOnUIThread(&AppCacheStorageImplTest::FailStoreGroup_PaddingTooBig);
}

TEST_F(AppCacheStorageImplTest, MakeGroupObsolete) {
  RunTestOnUIThread(&AppCacheStorageImplTest::MakeGroupObsolete);
}

TEST_F(AppCacheStorageImplTest, MarkEntryAsForeign) {
  RunTestOnUIThread(&AppCacheStorageImplTest::MarkEntryAsForeign);
}

TEST_F(AppCacheStorageImplTest, MarkEntryAsForeignWithLoadInProgress) {
  RunTestOnUIThread(
      &AppCacheStorageImplTest::MarkEntryAsForeignWithLoadInProgress);
}

TEST_F(AppCacheStorageImplTest, FindNoMainResponse) {
  RunTestOnUIThread(&AppCacheStorageImplTest::FindNoMainResponse);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainResponseInDatabase) {
  RunTestOnUIThread(&AppCacheStorageImplTest::BasicFindMainResponseInDatabase);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainResponse_OriginTrialFailure) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kAppCacheRequireOriginTrial);
  RunTestOnUIThread(
      &AppCacheStorageImplTest::BasicFindMainResponse_OriginTrialFailure);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainResponse_OriginTrialSuccess) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kAppCacheRequireOriginTrial);
  RunTestOnUIThread(
      &AppCacheStorageImplTest::BasicFindMainFallbackResponseInDatabase);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainResponseInWorkingSet) {
  RunTestOnUIThread(
      &AppCacheStorageImplTest::BasicFindMainResponseInWorkingSet);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainFallbackResponseInDatabase) {
  RunTestOnUIThread(
      &AppCacheStorageImplTest::BasicFindMainFallbackResponseInDatabase);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainFallbackResponseInWorkingSet) {
  RunTestOnUIThread(
      &AppCacheStorageImplTest::BasicFindMainFallbackResponseInWorkingSet);
}

TEST_F(AppCacheStorageImplTest, FindMainFallbackResponse_OriginTrialSuccess) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kAppCacheRequireOriginTrial);
  RunTestOnUIThread(
      &AppCacheStorageImplTest::BasicFindMainFallbackResponseInDatabase);
}

TEST_F(AppCacheStorageImplTest, FindMainFallbackResponse_OriginTrialFailure) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kAppCacheRequireOriginTrial);
  RunTestOnUIThread(
      &AppCacheStorageImplTest::FindMainFallbackResponse_OriginTrialFailure);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainInterceptResponseInDatabase) {
  RunTestOnUIThread(
      &AppCacheStorageImplTest::BasicFindMainInterceptResponseInDatabase);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainInterceptResponseInWorkingSet) {
  RunTestOnUIThread(
      &AppCacheStorageImplTest::BasicFindMainInterceptResponseInWorkingSet);
}

TEST_F(AppCacheStorageImplTest, FindMainInterceptResponse_OriginTrialSuccess) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kAppCacheRequireOriginTrial);
  RunTestOnUIThread(
      &AppCacheStorageImplTest::BasicFindMainInterceptResponseInDatabase);
}

TEST_F(AppCacheStorageImplTest, FindMainInterceptResponse_OriginTrialFailure) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kAppCacheRequireOriginTrial);
  RunTestOnUIThread(
      &AppCacheStorageImplTest::FindMainInterceptResponse_OriginTrialFailure);
}

TEST_F(AppCacheStorageImplTest, FindMainResponseWithMultipleHits) {
  RunTestOnUIThread(&AppCacheStorageImplTest::FindMainResponseWithMultipleHits);
}

TEST_F(AppCacheStorageImplTest, FindMainResponseExclusionsInDatabase) {
  RunTestOnUIThread(
      &AppCacheStorageImplTest::FindMainResponseExclusionsInDatabase);
}

TEST_F(AppCacheStorageImplTest, FindMainResponseExclusionsInWorkingSet) {
  RunTestOnUIThread(
      &AppCacheStorageImplTest::FindMainResponseExclusionsInWorkingSet);
}

TEST_F(AppCacheStorageImplTest, Reinitialize1) {
  RunTestOnUIThread(&AppCacheStorageImplTest::Reinitialize1);
}

TEST_F(AppCacheStorageImplTest, Reinitialize2) {
  RunTestOnUIThread(&AppCacheStorageImplTest::Reinitialize2);
}

TEST_F(AppCacheStorageImplTest, Reinitialize3) {
  RunTestOnUIThread(&AppCacheStorageImplTest::Reinitialize3);
}

// That's all folks!

}  // namespace content
