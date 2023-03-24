// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/local_storage_database.pb.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/partition.mojom.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/async_shared_storage_database_impl.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "components/services/storage/storage_service_impl.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/gpu/gpu_disk_cache_factory.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_permissions_cache.h"
#include "content/browser/interest_group/interest_group_permissions_checker.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/generated_code_cache_settings.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/cookie_manager.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_client.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/java_interfaces.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#endif  // BUILDFLAG(IS_ANDROID)

using net::CanonicalCookie;
using CookieDeletionFilter = network::mojom::CookieDeletionFilter;
using CookieDeletionFilterPtr = network::mojom::CookieDeletionFilterPtr;

namespace content {
namespace {

using ::attribution_reporting::SuitableOrigin;

const char kCacheKey[] = "key";
const char kCacheValue[] = "cached value";

const blink::mojom::StorageType kTemporary =
    blink::mojom::StorageType::kTemporary;
const blink::mojom::StorageType kSyncable =
    blink::mojom::StorageType::kSyncable;

const storage::QuotaClientType kClientFile =
    storage::QuotaClientType::kFileSystem;

const uint32_t kAllQuotaRemoveMask =
    StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
    StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
    StoragePartition::REMOVE_DATA_MASK_WEBSQL;

class AwaitCompletionHelper {
 public:
  AwaitCompletionHelper() : start_(false), already_quit_(false) {}

  AwaitCompletionHelper(const AwaitCompletionHelper&) = delete;
  AwaitCompletionHelper& operator=(const AwaitCompletionHelper&) = delete;

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
};

class RemoveCookieTester {
 public:
  explicit RemoveCookieTester(StoragePartition* storage_partition)
      : storage_partition_(storage_partition) {}

  RemoveCookieTester(const RemoveCookieTester&) = delete;
  RemoveCookieTester& operator=(const RemoveCookieTester&) = delete;

  // Returns true, if the given cookie exists in the cookie store.
  bool ContainsCookie(const url::Origin& origin) {
    get_cookie_success_ = false;
    storage_partition_->GetCookieManagerForBrowserProcess()->GetCookieList(
        origin.GetURL(), net::CookieOptions::MakeAllInclusive(),
        net::CookiePartitionKeyCollection(),
        base::BindOnce(&RemoveCookieTester::GetCookieListCallback,
                       base::Unretained(this)));
    await_completion_.BlockUntilNotified();
    return get_cookie_success_;
  }

  void AddCookie(const url::Origin& origin) {
    net::CookieInclusionStatus status;
    std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
        origin.GetURL(), "A=1", base::Time::Now(),
        absl::nullopt /* server_time */,
        absl::nullopt /* cookie_partition_key */, &status));
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
  raw_ptr<StoragePartition> storage_partition_;
};

class RemoveInterestGroupTester {
 public:
  explicit RemoveInterestGroupTester(StoragePartitionImpl* storage_partition)
      : storage_partition_(storage_partition) {}

  RemoveInterestGroupTester(const RemoveInterestGroupTester&) = delete;
  RemoveInterestGroupTester& operator=(const RemoveInterestGroupTester&) =
      delete;

  // Returns true, if the given interest group owner has any interest groups in
  // InterestGroupStorage.
  bool ContainsInterestGroupOwner(const url::Origin& origin) {
    get_interest_group_success_ = false;
    EXPECT_TRUE(storage_partition_->GetInterestGroupManager());
    static_cast<InterestGroupManagerImpl*>(
        storage_partition_->GetInterestGroupManager())
        ->GetInterestGroupsForOwner(
            origin, base::BindOnce(
                        &RemoveInterestGroupTester::GetInterestGroupsCallback,
                        base::Unretained(this)));
    await_completion_.BlockUntilNotified();
    return get_interest_group_success_;
  }

  bool ContainsInterestGroupKAnon(const url::Origin& origin) {
    contains_kanon_ = false;
    EXPECT_TRUE(storage_partition_->GetInterestGroupManager());
    static_cast<InterestGroupManagerImpl*>(
        storage_partition_->GetInterestGroupManager())
        ->GetLastKAnonymityReported(
            k_anon_key,
            base::BindOnce(
                &RemoveInterestGroupTester::GetLastKAnonymityReportedCallback,
                base::Unretained(this)));
    await_completion_.BlockUntilNotified();
    return contains_kanon_;
  }

  void AddInterestGroup(const url::Origin& origin) {
    EXPECT_TRUE(storage_partition_->GetInterestGroupManager());
    blink::InterestGroup group;
    group.owner = origin;
    group.name = "Name";
    group.expiry = base::Time::Now() + base::Days(30);
    group.bidding_url = origin.GetURL().Resolve("/bidding.js");
    group.ads.emplace();
    group.ads->push_back(blink::InterestGroup::Ad(
        GURL("https://owner.example.com/ad1"), "metadata"));

    InterestGroupManagerImpl* interest_group_manager =
        static_cast<InterestGroupManagerImpl*>(
            storage_partition_->GetInterestGroupManager());
    interest_group_manager->JoinInterestGroup(group, origin.GetURL());

    // Update the K-anonymity so that we can tell when it gets removed.
    k_anon_key = KAnonKeyForAdBid(group, GURL("https://owner.example.com/ad1"));
    interest_group_manager->UpdateLastKAnonymityReported(k_anon_key);
  }

 private:
  void GetInterestGroupsCallback(std::vector<StorageInterestGroup> groups) {
    get_interest_group_success_ = groups.size() > 0;
    await_completion_.Notify();
  }

  void GetLastKAnonymityReportedCallback(
      absl::optional<base::Time> last_reported) {
    contains_kanon_ =
        last_reported.has_value() && last_reported.value() > base::Time::Min();
    await_completion_.Notify();
  }

  bool get_interest_group_success_ = false;
  bool contains_kanon_ = false;
  std::string k_anon_key;
  AwaitCompletionHelper await_completion_;
  raw_ptr<StoragePartitionImpl> storage_partition_;
};

class RemoveLocalStorageTester {
 public:
  RemoveLocalStorageTester(content::BrowserTaskEnvironment* task_environment,
                           TestBrowserContext* browser_context)
      : task_environment_(task_environment),
        storage_partition_(browser_context->GetDefaultStoragePartition()),
        dom_storage_context_(storage_partition_->GetDOMStorageContext()) {}

  RemoveLocalStorageTester(const RemoveLocalStorageTester&) = delete;
  RemoveLocalStorageTester& operator=(const RemoveLocalStorageTester&) = delete;

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
      if (origin == infos_[i].storage_key.origin())
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
        storage::kLocalStorageLeveldbName, absl::nullopt,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::BindLambdaForTesting([&](leveldb::Status status) {
          ASSERT_TRUE(status.ok());
          open_loop.Quit();
        }));
    open_loop.Run();

    base::RunLoop populate_loop;
    database->database().PostTaskWithThisObject(
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
    storage::LocalStorageStorageKeyMetaData data;
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> entries;

    base::Time now = base::Time::Now();
    data.set_last_modified(now.ToInternalValue());
    data.set_size_bytes(16);
    ASSERT_TRUE(
        db.Put(CreateMetaDataKey(origin1),
               base::as_bytes(base::make_span(data.SerializeAsString())))
            .ok());
    ASSERT_TRUE(db.Put(CreateDataKey(origin1), {}).ok());

    base::Time one_day_ago = now - base::Days(1);
    data.set_last_modified(one_day_ago.ToInternalValue());
    ASSERT_TRUE(
        db.Put(CreateMetaDataKey(origin2),
               base::as_bytes(base::make_span((data.SerializeAsString()))))
            .ok());
    ASSERT_TRUE(db.Put(CreateDataKey(origin2), {}).ok());

    base::Time sixty_days_ago = now - base::Days(60);
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
    key.reserve(std::size(kMetaPrefix) + serialized_origin.size());
    key.insert(key.end(), kMetaPrefix, kMetaPrefix + std::size(kMetaPrefix));
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
  const raw_ptr<BrowserTaskEnvironment> task_environment_;
  const raw_ptr<StoragePartition> storage_partition_;
  raw_ptr<DOMStorageContext> dom_storage_context_;

  std::vector<content::StorageUsageInfo> infos_;

  AwaitCompletionHelper await_completion_;
};

class RemoveCodeCacheTester {
 public:
  explicit RemoveCodeCacheTester(GeneratedCodeCacheContext* code_cache_context)
      : code_cache_context_(code_cache_context) {}

  RemoveCodeCacheTester(const RemoveCodeCacheTester&) = delete;
  RemoveCodeCacheTester& operator=(const RemoveCodeCacheTester&) = delete;

  enum Cache { kJs, kWebAssembly, kWebUiJs };

  bool ContainsEntry(Cache cache, const GURL& url, const GURL& origin_lock) {
    entry_exists_ = false;
    base::RunLoop loop;
    GeneratedCodeCacheContext::RunOrPostTask(
        code_cache_context_.get(), FROM_HERE,
        base::BindOnce(&RemoveCodeCacheTester::ContainsEntryOnThread,
                       base::Unretained(this), cache, url, origin_lock,
                       loop.QuitClosure()));
    loop.Run();
    return entry_exists_;
  }

  void ContainsEntryOnThread(Cache cache,
                             const GURL& url,
                             const GURL& origin_lock,
                             base::OnceClosure quit) {
    GeneratedCodeCache::ReadDataCallback callback =
        base::BindOnce(&RemoveCodeCacheTester::FetchEntryCallback,
                       base::Unretained(this), std::move(quit));
    GetCache(cache)->FetchEntry(url, origin_lock, net::NetworkIsolationKey(),
                                std::move(callback));
  }

  void AddEntry(Cache cache,
                const GURL& url,
                const GURL& origin_lock,
                const std::string& data) {
    base::RunLoop loop;
    GeneratedCodeCacheContext::RunOrPostTask(
        code_cache_context_.get(), FROM_HERE,
        base::BindOnce(&RemoveCodeCacheTester::AddEntryOnThread,
                       base::Unretained(this), cache, url, origin_lock, data,
                       loop.QuitClosure()));
    loop.Run();
  }

  void AddEntryOnThread(Cache cache,
                        const GURL& url,
                        const GURL& origin_lock,
                        const std::string& data,
                        base::OnceClosure quit) {
    std::vector<uint8_t> data_vector(data.begin(), data.end());
    GetCache(cache)->WriteEntry(url, origin_lock, net::NetworkIsolationKey(),
                                base::Time::Now(), data_vector);
    std::move(quit).Run();
  }

  void SetLastUseTime(Cache cache,
                      const GURL& url,
                      const GURL& origin_lock,
                      base::Time time) {
    base::RunLoop loop;
    GeneratedCodeCacheContext::RunOrPostTask(
        code_cache_context_.get(), FROM_HERE,
        base::BindOnce(&RemoveCodeCacheTester::SetLastUseTimeOnThread,
                       base::Unretained(this), cache, url, origin_lock, time,
                       loop.QuitClosure()));
    loop.Run();
  }

  void SetLastUseTimeOnThread(Cache cache,
                              const GURL& url,
                              const GURL& origin_lock,
                              base::Time time,
                              base::OnceClosure quit) {
    GetCache(cache)->SetLastUsedTimeForTest(
        url, origin_lock, net::NetworkIsolationKey(), time, std::move(quit));
  }

  std::string received_data() { return received_data_; }

 private:
  GeneratedCodeCache* GetCache(Cache cache) {
    if (cache == kJs)
      return code_cache_context_->generated_js_code_cache();
    else if (cache == kWebAssembly)
      return code_cache_context_->generated_wasm_code_cache();
    else
      return code_cache_context_->generated_webui_js_code_cache();
  }

  void FetchEntryCallback(base::OnceClosure quit,
                          const base::Time& response_time,
                          mojo_base::BigBuffer data) {
    if (!response_time.is_null()) {
      entry_exists_ = true;
      received_data_ = std::string(data.data(), data.data() + data.size());
    } else {
      entry_exists_ = false;
    }
    std::move(quit).Run();
  }

  bool entry_exists_;
  AwaitCompletionHelper await_completion_;
  raw_ptr<GeneratedCodeCacheContext> code_cache_context_;
  std::string received_data_;
};

class MockDataRemovalObserver : public StoragePartition::DataRemovalObserver {
 public:
  explicit MockDataRemovalObserver(StoragePartition* partition) {
    observation_.Observe(partition);
  }

  MOCK_METHOD4(OnStorageKeyDataCleared,
               void(uint32_t,
                    content::StoragePartition::StorageKeyMatcherFunction,
                    base::Time,
                    base::Time));

 private:
  base::ScopedObservation<StoragePartition,
                          StoragePartition::DataRemovalObserver>
      observation_{this};
};

bool IsWebSafeSchemeForTest(const std::string& scheme) {
  return scheme == url::kHttpScheme;
}

bool DoesOriginMatchForUnprotectedWeb(
    const blink::StorageKey& storage_key,
    storage::SpecialStoragePolicy* special_storage_policy) {
  if (IsWebSafeSchemeForTest(storage_key.origin().scheme())) {
    return !special_storage_policy->IsStorageProtected(
        storage_key.origin().GetURL());
  }

  return false;
}

bool DoesOriginMatchForBothProtectedAndUnprotectedWeb(
    const blink::StorageKey& storage_key,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return true;
}

bool DoesOriginMatchUnprotected(
    const url::Origin& desired_origin,
    const blink::StorageKey& storage_key,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return storage_key.origin().scheme() != desired_origin.scheme();
}

void ClearQuotaData(content::StoragePartition* partition,
                    base::RunLoop* loop_to_quit) {
  partition->ClearData(kAllQuotaRemoveMask,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                       blink::StorageKey(), base::Time(), base::Time::Max(),
                       loop_to_quit->QuitClosure());
}

void ClearQuotaDataWithOriginMatcher(
    content::StoragePartition* partition,
    StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
    const base::Time delete_begin,
    base::RunLoop* loop_to_quit) {
  partition->ClearData(
      kAllQuotaRemoveMask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      /*filter_builder=*/nullptr, std::move(storage_key_matcher), nullptr,
      false, delete_begin, base::Time::Max(), loop_to_quit->QuitClosure());
}

void ClearQuotaDataForOrigin(content::StoragePartition* partition,
                             const GURL& remove_origin,
                             const base::Time delete_begin,
                             base::RunLoop* loop_to_quit) {
  partition->ClearData(
      kAllQuotaRemoveMask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(remove_origin)),
      delete_begin, base::Time::Max(), loop_to_quit->QuitClosure());
}

void ClearQuotaDataForTemporary(content::StoragePartition* partition,
                                const base::Time delete_begin,
                                base::RunLoop* loop_to_quit) {
  partition->ClearData(kAllQuotaRemoveMask,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_TEMPORARY,
                       blink::StorageKey(), delete_begin, base::Time::Max(),
                       loop_to_quit->QuitClosure());
}

void ClearCookies(content::StoragePartition* partition,
                  const base::Time delete_begin,
                  const base::Time delete_end,
                  base::RunLoop* run_loop) {
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_COOKIES,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                       blink::StorageKey(), delete_begin, delete_end,
                       run_loop->QuitClosure());
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
                       /*filter_builder=*/nullptr,
                       StoragePartition::StorageKeyPolicyMatcherFunction(),
                       std::move(delete_filter), false, delete_begin,
                       delete_end, run_loop->QuitClosure());
}

void ClearStuff(
    uint32_t remove_mask,
    content::StoragePartition* partition,
    const base::Time delete_begin,
    const base::Time delete_end,
    BrowsingDataFilterBuilder* filter_builder,
    StoragePartition::StorageKeyPolicyMatcherFunction storage_key_matcher,
    base::RunLoop* run_loop) {
  partition->ClearData(
      remove_mask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      filter_builder, std::move(storage_key_matcher), nullptr, false,
      delete_begin, delete_end, run_loop->QuitClosure());
}

void ClearData(content::StoragePartition* partition, base::RunLoop* run_loop) {
  base::Time time;
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                       blink::StorageKey(), time, time,
                       run_loop->QuitClosure());
}

void ClearDataForOrigin(uint32_t remove_mask,
                        content::StoragePartition* partition,
                        const GURL& origin,
                        base::RunLoop* run_loop) {
  partition->ClearDataForOrigin(
      remove_mask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, origin,
      run_loop->QuitClosure());
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

void ClearInterestGroups(content::StoragePartition* partition,
                         const base::Time delete_begin,
                         const base::Time delete_end,
                         base::RunLoop* run_loop) {
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUPS,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                       blink::StorageKey(), delete_begin, delete_end,
                       run_loop->QuitClosure());
}

void ClearInterestGroupsAndKAnon(content::StoragePartition* partition,
                                 const base::Time delete_begin,
                                 const base::Time delete_end,
                                 base::RunLoop* run_loop) {
  partition->ClearData(
      StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUPS |
          StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUPS_INTERNAL,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, blink::StorageKey(),
      delete_begin, delete_end, run_loop->QuitClosure());
}

void ClearInterestGroupPermissionsCache(content::StoragePartition* partition,
                                        const base::Time delete_begin,
                                        const base::Time delete_end,
                                        base::RunLoop* run_loop) {
  partition->ClearData(
      StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUP_PERMISSIONS_CACHE,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, blink::StorageKey(),
      delete_begin, delete_end, run_loop->QuitClosure());
}

bool FilterMatchesCookie(const CookieDeletionFilterPtr& filter,
                         const net::CanonicalCookie& cookie) {
  return network::DeletionFilterToInfo(filter.Clone())
      .Matches(cookie,
               net::CookieAccessParams{
                   net::CookieAccessSemantics::NONLEGACY, false,
                   net::CookieSamePartyStatus::kNoSamePartyEnforcement});
}

}  // namespace

class StoragePartitionImplTest : public testing::Test {
 public:
  StoragePartitionImplTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()) {
    // Prevent test flakiness as a result of randomized responses in the
    // Attribution Reporting API.
    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kAttributionReportingDebugMode);

    feature_list_.InitWithFeatures({blink::features::kInterestGroupStorage,
                                    blink::features::kSharedStorageAPI},
                                   {});
  }

  StoragePartitionImplTest(const StoragePartitionImplTest&) = delete;
  StoragePartitionImplTest& operator=(const StoragePartitionImplTest&) = delete;

  storage::MockQuotaManager* GetMockManager() {
    if (!quota_manager_.get()) {
      quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
          browser_context_->IsOffTheRecord(), browser_context_->GetPath(),
          GetIOThreadTaskRunner({}).get(),
          browser_context_->GetSpecialStoragePolicy());
      mojo::PendingRemote<storage::mojom::QuotaClient> quota_client;
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<storage::MockQuotaClient>(
              quota_manager_->proxy(), storage::QuotaClientType::kFileSystem),
          quota_client.InitWithNewPipeAndPassReceiver());
      quota_manager_->proxy()->RegisterClient(
          std::move(quota_client), storage::QuotaClientType::kFileSystem,
          {blink::mojom::StorageType::kTemporary,
           blink::mojom::StorageType::kSyncable});
    }
    return quota_manager_.get();
  }

  TestBrowserContext* browser_context() { return browser_context_.get(); }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  base::test::ScopedCommandLine command_line_;
  base::test::ScopedFeatureList feature_list_;
  // Configures the Attribution Reporting API to run in memory to speed up its
  // initialization and avoid timeouts. See https://crbug.com/1080764.
  AttributionManagerImpl::ScopedUseInMemoryStorageForTesting
      attribution_manager_in_memory_setting_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
};

class StoragePartitionShaderClearTest : public testing::Test {
 public:
  StoragePartitionShaderClearTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()) {
    InitGpuDiskCacheFactorySingleton();

    gpu::GpuDiskCacheType type = gpu::GpuDiskCacheType::kGlShaders;
    auto handle = GetGpuDiskCacheFactorySingleton()->GetCacheHandle(
        type, browser_context()->GetDefaultStoragePartition()->GetPath().Append(
                  gpu::GetGpuDiskCacheSubdir(type)));
    cache_ =
        GetGpuDiskCacheFactorySingleton()->Create(handle, base::DoNothing());
  }

  ~StoragePartitionShaderClearTest() override { cache_ = nullptr; }

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

  scoped_refptr<gpu::GpuDiskCache> cache_;
};

// Tests ---------------------------------------------------------------------

TEST_F(StoragePartitionShaderClearTest, ClearShaderCache) {
  InitCache();
  EXPECT_EQ(1u, Size());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearData,
                                browser_context()->GetDefaultStoragePartition(),
                                &run_loop));
  run_loop.Run();
  EXPECT_EQ(0u, Size());
}

TEST_F(StoragePartitionImplTest, QuotaClientTypesGeneration) {
  EXPECT_THAT(
      StoragePartitionImpl::GenerateQuotaClientTypes(
          StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS),
      testing::UnorderedElementsAre(storage::QuotaClientType::kFileSystem));
  EXPECT_THAT(StoragePartitionImpl::GenerateQuotaClientTypes(
                  StoragePartition::REMOVE_DATA_MASK_WEBSQL),
              testing::ElementsAre(storage::QuotaClientType::kDatabase));
  EXPECT_THAT(StoragePartitionImpl::GenerateQuotaClientTypes(
                  StoragePartition::REMOVE_DATA_MASK_INDEXEDDB),
              testing::ElementsAre(storage::QuotaClientType::kIndexedDatabase));
  EXPECT_THAT(
      StoragePartitionImpl::GenerateQuotaClientTypes(kAllQuotaRemoveMask),
      testing::UnorderedElementsAre(
          storage::QuotaClientType::kFileSystem,
          storage::QuotaClientType::kDatabase,
          storage::QuotaClientType::kIndexedDatabase));
}

storage::BucketInfo AddQuotaManagedBucket(
    storage::MockQuotaManager* manager,
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type,
    base::Time modified = base::Time::Now()) {
  storage::BucketInfo bucket =
      manager->CreateBucket({storage_key, bucket_name}, type);
  manager->AddBucket(bucket, {kClientFile}, modified);
  EXPECT_TRUE(manager->BucketHasData(bucket, kClientFile));
  return bucket;
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverBoth) {
  const blink::StorageKey kStorageKey1 =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  const blink::StorageKey kStorageKey2 =
      blink::StorageKey::CreateFromStringForTesting("http://host2:1/");
  const blink::StorageKey kStorageKey3 =
      blink::StorageKey::CreateFromStringForTesting("http://host3:1/");

  AddQuotaManagedBucket(GetMockManager(), kStorageKey1,
                        storage::kDefaultBucketName, kTemporary);
  AddQuotaManagedBucket(GetMockManager(), kStorageKey2,
                        storage::kDefaultBucketName, kTemporary);
  AddQuotaManagedBucket(GetMockManager(), kStorageKey2,
                        storage::kDefaultBucketName, kSyncable);
  AddQuotaManagedBucket(GetMockManager(), kStorageKey3,
                        storage::kDefaultBucketName, kSyncable);
  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 4);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 0);
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverOnlyTemporary) {
  const blink::StorageKey kStorageKey1 =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  const blink::StorageKey kStorageKey2 =
      blink::StorageKey::CreateFromStringForTesting("http://host2:1/");

  AddQuotaManagedBucket(GetMockManager(), kStorageKey1,
                        storage::kDefaultBucketName, kTemporary);
  AddQuotaManagedBucket(GetMockManager(), kStorageKey2,
                        storage::kDefaultBucketName, kTemporary);
  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 2);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 0);
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverOnlySyncable) {
  const blink::StorageKey kStorageKey1 =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  const blink::StorageKey kStorageKey2 =
      blink::StorageKey::CreateFromStringForTesting("http://host2:1/");

  AddQuotaManagedBucket(GetMockManager(), kStorageKey1,
                        storage::kDefaultBucketName, kSyncable);
  AddQuotaManagedBucket(GetMockManager(), kStorageKey2,
                        storage::kDefaultBucketName, kSyncable);
  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 2);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 0);
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverNeither) {
  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 0);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 0);
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverSpecificOrigin) {
  const blink::StorageKey kStorageKey1 =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  const blink::StorageKey kStorageKey2 =
      blink::StorageKey::CreateFromStringForTesting("http://host2:1/");
  const blink::StorageKey kStorageKey3 =
      blink::StorageKey::CreateFromStringForTesting("http://host3:1/");

  storage::BucketInfo host1_temp_bucket = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey1, storage::kDefaultBucketName, kTemporary);
  storage::BucketInfo host2_temp_bucket = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey2, storage::kDefaultBucketName, kTemporary);
  storage::BucketInfo host2_sync_bucket = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey2, storage::kDefaultBucketName, kSyncable);
  storage::BucketInfo host3_sync_bucket = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey3, storage::kDefaultBucketName, kSyncable);

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 4);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearQuotaDataForOrigin, partition,
                     kStorageKey1.origin().GetURL(), base::Time(), &run_loop));
  run_loop.Run();

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 3);
  EXPECT_FALSE(GetMockManager()->BucketHasData(host1_temp_bucket, kClientFile));
  EXPECT_TRUE(GetMockManager()->BucketHasData(host2_temp_bucket, kClientFile));
  EXPECT_TRUE(GetMockManager()->BucketHasData(host2_sync_bucket, kClientFile));
  EXPECT_TRUE(GetMockManager()->BucketHasData(host3_sync_bucket, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForLastHour) {
  const blink::StorageKey kStorageKey1 =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  const blink::StorageKey kStorageKey2 =
      blink::StorageKey::CreateFromStringForTesting("http://host2:1/");
  const blink::StorageKey kStorageKey3 =
      blink::StorageKey::CreateFromStringForTesting("http://host3:1/");

  // Buckets modified now.
  base::Time now = base::Time::Now();
  storage::BucketInfo host1_temp_bucket_now = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey1, "temp_bucket_now", kTemporary, now);
  storage::BucketInfo host1_sync_bucket_now =
      AddQuotaManagedBucket(GetMockManager(), kStorageKey1,
                            storage::kDefaultBucketName, kSyncable, now);
  storage::BucketInfo host2_temp_bucket_now = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey2, "temp_bucket_now", kTemporary, now);
  storage::BucketInfo host2_sync_bucket_now =
      AddQuotaManagedBucket(GetMockManager(), kStorageKey2,
                            storage::kDefaultBucketName, kSyncable, now);

  // Buckets modified a day ago.
  base::Time yesterday = now - base::Days(1);
  storage::BucketInfo host1_temp_bucket_yesterday =
      AddQuotaManagedBucket(GetMockManager(), kStorageKey1,
                            "temp_bucket_yesterday", kTemporary, yesterday);
  storage::BucketInfo host2_temp_bucket_yesterday =
      AddQuotaManagedBucket(GetMockManager(), kStorageKey2,
                            "temp_bucket_yesterday", kTemporary, yesterday);
  storage::BucketInfo host3_sync_bucket_yesterday =
      AddQuotaManagedBucket(GetMockManager(), kStorageKey3,
                            storage::kDefaultBucketName, kSyncable, yesterday);

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 7);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaDataForOrigin, partition, GURL(),
                                base::Time::Now() - base::Hours(1), &run_loop));
  run_loop.Run();

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 3);
  EXPECT_FALSE(
      GetMockManager()->BucketHasData(host1_temp_bucket_now, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->BucketHasData(host1_sync_bucket_now, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->BucketHasData(host2_temp_bucket_now, kClientFile));
  EXPECT_FALSE(
      GetMockManager()->BucketHasData(host2_sync_bucket_now, kClientFile));
  EXPECT_TRUE(GetMockManager()->BucketHasData(host1_temp_bucket_yesterday,
                                              kClientFile));
  EXPECT_TRUE(GetMockManager()->BucketHasData(host3_sync_bucket_yesterday,
                                              kClientFile));
  EXPECT_TRUE(GetMockManager()->BucketHasData(host2_temp_bucket_yesterday,
                                              kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedTemporaryDataForLastWeek) {
  const blink::StorageKey kStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");

  // Buckets modified yesterday.
  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::Days(1);
  storage::BucketInfo temp_bucket_yesterday =
      AddQuotaManagedBucket(GetMockManager(), kStorageKey,
                            "temp_bucket_yesterday", kTemporary, yesterday);
  storage::BucketInfo sync_bucket_yesterday =
      AddQuotaManagedBucket(GetMockManager(), kStorageKey,
                            storage::kDefaultBucketName, kSyncable, yesterday);

  // Buckets modified 10 days ago.
  base::Time ten_days_ago = now - base::Days(10);
  storage::BucketInfo temp_bucket_ten_days_ago = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey, "temp_bucket_ten_days_ago", kTemporary,
      ten_days_ago);

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 3);

  base::RunLoop run_loop;
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaDataForTemporary, partition,
                                base::Time::Now() - base::Days(7), &run_loop));
  run_loop.Run();

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 2);
  EXPECT_FALSE(
      GetMockManager()->BucketHasData(temp_bucket_yesterday, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->BucketHasData(sync_bucket_yesterday, kClientFile));
  EXPECT_TRUE(
      GetMockManager()->BucketHasData(temp_bucket_ten_days_ago, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedUnprotectedOrigins) {
  const blink::StorageKey kStorageKey1 =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  const blink::StorageKey kStorageKey2 =
      blink::StorageKey::CreateFromStringForTesting("http://host2:1/");

  storage::BucketInfo host1_temp_bucket = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey1, storage::kDefaultBucketName, kTemporary);
  storage::BucketInfo host1_sync_bucket = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey1, storage::kDefaultBucketName, kSyncable);
  storage::BucketInfo host2_temp_bucket = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey2, storage::kDefaultBucketName, kTemporary);
  storage::BucketInfo host2_sync_bucket = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey2, storage::kDefaultBucketName, kSyncable);

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 4);

  // Protect kStorageKey1.
  auto mock_policy = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
  mock_policy->AddProtected(kStorageKey1.origin().GetURL());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideQuotaManagerForTesting(GetMockManager());
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearQuotaDataWithOriginMatcher, partition,
                     base::BindRepeating(&DoesOriginMatchForUnprotectedWeb),
                     base::Time(), &run_loop));
  run_loop.Run();

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 2);
  EXPECT_TRUE(GetMockManager()->BucketHasData(host1_temp_bucket, kClientFile));
  EXPECT_TRUE(GetMockManager()->BucketHasData(host1_sync_bucket, kClientFile));
  EXPECT_FALSE(GetMockManager()->BucketHasData(host2_temp_bucket, kClientFile));
  EXPECT_FALSE(GetMockManager()->BucketHasData(host2_sync_bucket, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedProtectedOrigins) {
  const blink::StorageKey kStorageKey1 =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  const blink::StorageKey kStorageKey2 =
      blink::StorageKey::CreateFromStringForTesting("http://host2:1/");

  AddQuotaManagedBucket(GetMockManager(), kStorageKey1,
                        storage::kDefaultBucketName, kTemporary);
  AddQuotaManagedBucket(GetMockManager(), kStorageKey1,
                        storage::kDefaultBucketName, kSyncable);
  AddQuotaManagedBucket(GetMockManager(), kStorageKey2,
                        storage::kDefaultBucketName, kTemporary);
  AddQuotaManagedBucket(GetMockManager(), kStorageKey2,
                        storage::kDefaultBucketName, kSyncable);
  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 4);

  // Protect kStorageKey1.
  auto mock_policy = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
  mock_policy->AddProtected(kStorageKey1.origin().GetURL());

  // Try to remove kStorageKey1. Expect success.
  base::RunLoop run_loop;
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideQuotaManagerForTesting(GetMockManager());
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearQuotaDataWithOriginMatcher, partition,
                     base::BindRepeating(
                         &DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                     base::Time(), &run_loop));
  run_loop.Run();

  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 0);
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedIgnoreDevTools) {
  const blink::StorageKey kStorageKey =
      blink::StorageKey::CreateFromStringForTesting(
          "devtools://abcdefghijklmnopqrstuvw/");

  storage::BucketInfo temp_bucket = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey, storage::kDefaultBucketName, kTemporary,
      base::Time());
  storage::BucketInfo sync_bucket = AddQuotaManagedBucket(
      GetMockManager(), kStorageKey, storage::kDefaultBucketName, kSyncable,
      base::Time());
  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 2);

  base::RunLoop run_loop;
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideQuotaManagerForTesting(GetMockManager());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearQuotaDataWithOriginMatcher, partition,
                                base::BindRepeating(&DoesOriginMatchUnprotected,
                                                    kStorageKey.origin()),
                                base::Time(), &run_loop));
  run_loop.Run();

  // Check that devtools data isn't removed.
  EXPECT_EQ(GetMockManager()->BucketDataCount(kClientFile), 2);
  EXPECT_TRUE(GetMockManager()->BucketHasData(temp_bucket, kClientFile));
  EXPECT_TRUE(GetMockManager()->BucketHasData(sync_bucket, kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveCookieForever) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://host1:1/"));

  StoragePartition* partition = browser_context()->GetDefaultStoragePartition();

  RemoveCookieTester tester(partition);
  tester.AddCookie(kOrigin);
  ASSERT_TRUE(tester.ContainsCookie(kOrigin));

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearCookies, partition, base::Time(),
                                base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.ContainsCookie(kOrigin));
}

TEST_F(StoragePartitionImplTest, RemoveCookieLastHour) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://host1:1/"));

  StoragePartition* partition = browser_context()->GetDefaultStoragePartition();

  RemoveCookieTester tester(partition);
  tester.AddCookie(kOrigin);
  ASSERT_TRUE(tester.ContainsCookie(kOrigin));

  base::Time an_hour_ago = base::Time::Now() - base::Hours(1);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearCookies, partition, an_hour_ago,
                                base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.ContainsCookie(kOrigin));
}

TEST_F(StoragePartitionImplTest, RemoveCookieWithDeleteInfo) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://host1:1/"));

  StoragePartition* partition = browser_context()->GetDefaultStoragePartition();

  RemoveCookieTester tester(partition);
  tester.AddCookie(kOrigin);
  ASSERT_TRUE(tester.ContainsCookie(kOrigin));

  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearCookiesMatchingInfo, partition,
                                CookieDeletionFilter::New(), &run_loop2));
  run_loop2.RunUntilIdle();
  EXPECT_FALSE(tester.ContainsCookie(kOrigin));
}

TEST_F(StoragePartitionImplTest, RemoveInterestGroupForever) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://host1:1/"));

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());

  RemoveInterestGroupTester tester(partition);
  tester.AddInterestGroup(kOrigin);
  ASSERT_TRUE(tester.ContainsInterestGroupOwner(kOrigin));

  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ClearInterestGroups, partition, base::Time(),
                                  base::Time::Max(), &run_loop));
    run_loop.Run();
  }
  EXPECT_FALSE(tester.ContainsInterestGroupOwner(kOrigin));
  EXPECT_TRUE(tester.ContainsInterestGroupKAnon(kOrigin));

  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ClearInterestGroupsAndKAnon, partition,
                                  base::Time(), base::Time::Max(), &run_loop));
    run_loop.Run();
  }
  EXPECT_FALSE(tester.ContainsInterestGroupOwner(kOrigin));
  EXPECT_FALSE(tester.ContainsInterestGroupKAnon(kOrigin));
}

TEST_F(StoragePartitionImplTest, RemoveInterestGroupPermissionsCacheForever) {
  const url::Origin kFrameOrigin =
      url::Origin::Create(GURL("https://host1.test:1/"));
  const url::Origin kInterestGroupOrigin =
      url::Origin::Create(GURL("https://host2.test:2/"));
  const net::NetworkIsolationKey kNetworkIsolationKey(kFrameOrigin,
                                                      kFrameOrigin);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  ASSERT_TRUE(partition->GetInterestGroupManager());

  InterestGroupPermissionsCache& permissions_cache =
      static_cast<InterestGroupManagerImpl*>(
          partition->GetInterestGroupManager())
          ->permissions_checker_for_testing()
          .cache_for_testing();

  permissions_cache.CachePermissions(InterestGroupPermissionsCache::Permissions{
                                         /*can_join=*/true, /*can_leave=*/true},
                                     kFrameOrigin, kInterestGroupOrigin,
                                     kNetworkIsolationKey);
  EXPECT_TRUE(permissions_cache.GetPermissions(
      kFrameOrigin, kInterestGroupOrigin, kNetworkIsolationKey));

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ClearInterestGroupPermissionsCache, partition,
                                base::Time(), base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(permissions_cache.GetPermissions(
      kFrameOrigin, kInterestGroupOrigin, kNetworkIsolationKey));
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
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClearStuff, StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
          partition, base::Time(), base::Time::Max(),
          /*filter_builder=*/nullptr,
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
      browser_context()->GetDefaultStoragePartition());
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearStuff,
                     StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                     partition, base::Time(), base::Time::Max(),
                     /*filter_builder=*/nullptr,
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
      browser_context()->GetDefaultStoragePartition());
  base::Time a_week_ago = base::Time::Now() - base::Days(7);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearStuff,
                     StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                     partition, a_week_ago, base::Time::Max(),
                     /*filter_builder=*/nullptr,
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

TEST_F(StoragePartitionImplTest, RemoveLocalStorageForOrigins) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  RemoveLocalStorageTester tester(task_environment(), browser_context());

  tester.AddDOMStorageTestData(kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());

  auto filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddOrigin(kOrigin1);
  filter_builder->AddOrigin(kOrigin2);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClearStuff, StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
          partition, base::Time::Min(), base::Time::Max(), filter_builder.get(),
          StoragePartition::StorageKeyPolicyMatcherFunction(), &run_loop));
  run_loop.Run();
  // ClearData only guarantees that tasks to delete data are scheduled when its
  // callback is invoked. It doesn't guarantee data has actually been cleared.
  // So run all scheduled tasks to make sure data is cleared.
  base::RunLoop().RunUntilIdle();

  // kOrigin3 is not filtered by the filter builder.
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplTest, RemoveLocalStorageForOneOrigin) {
  const GURL kUrl1 = GURL("http://host1:1/");
  const url::Origin kOrigin1 = url::Origin::Create(kUrl1);
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  RemoveLocalStorageTester tester(task_environment(), browser_context());

  tester.AddDOMStorageTestData(kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearDataForOrigin,
                     StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                     partition, kUrl1, &run_loop));
  run_loop.Run();
  // ClearData only guarantees that tasks to delete data are scheduled when its
  // callback is invoked. It doesn't guarantee data has actually been cleared.
  // So run all scheduled tasks to make sure data is cleared.
  base::RunLoop().RunUntilIdle();

  // kOrigin1 should be cleared.
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplTest, ClearCodeCache) {
  const GURL kResourceURL("http://host4/script.js");

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
      browser_context()->GetDefaultStoragePartition());
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
      browser_context()->GetDefaultStoragePartition());
  // Ensure code cache is initialized.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(partition->GetGeneratedCodeCacheContext() != nullptr);

  RemoveCodeCacheTester tester(partition->GetGeneratedCodeCacheContext());

  base::Time current_time = base::Time::NowFromSystemTime();
  base::Time out_of_range_time = current_time - base::Hours(3);
  base::Time begin_time = current_time - base::Hours(2);
  base::Time in_range_time = current_time - base::Hours(1);

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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
      browser_context()->GetDefaultStoragePartition());
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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

TEST_F(StoragePartitionImplTest, ClearWebUICodeCache) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kWebUICodeCache);

  const GURL kResourceURL("chrome://host4/script.js");

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  // Ensure code cache is initialized.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(partition->GetGeneratedCodeCacheContext() != nullptr);

  RemoveCodeCacheTester tester(partition->GetGeneratedCodeCacheContext());

  GURL origin = GURL("chrome://host1:1/");
  std::string data("SomeData");
  tester.AddEntry(RemoveCodeCacheTester::kWebUiJs, kResourceURL, origin, data);
  EXPECT_TRUE(tester.ContainsEntry(RemoveCodeCacheTester::kWebUiJs,
                                   kResourceURL, origin));
  EXPECT_EQ(tester.received_data(), data);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearCodeCache, partition, base::Time(), base::Time(),
                     base::RepeatingCallback<bool(const GURL&)>(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.ContainsEntry(RemoveCodeCacheTester::kWebUiJs,
                                    kResourceURL, origin));

  // Make sure there isn't a second invalid callback sitting in the queue.
  // (this used to be a bug).
  base::RunLoop().RunUntilIdle();
}

TEST_F(StoragePartitionImplTest, WebUICodeCacheDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kWebUICodeCache);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  // Ensure code cache is initialized.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(partition->GetGeneratedCodeCacheContext() != nullptr);
  base::RunLoop run_loop;
  auto* context = partition->GetGeneratedCodeCacheContext();
  GeneratedCodeCacheContext::RunOrPostTask(
      context, FROM_HERE, base::BindLambdaForTesting([&]() {
        EXPECT_EQ(partition->GetGeneratedCodeCacheContext()
                      ->generated_webui_js_code_cache(),
                  nullptr);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(StoragePartitionImplTest, ClearCodeCacheIncognito) {
  browser_context()->set_is_off_the_record(true);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  base::RunLoop().RunUntilIdle();
  // We should not create GeneratedCodeCacheContext for off the record mode.
  EXPECT_EQ(nullptr, partition->GetGeneratedCodeCacheContext());

  base::RunLoop run_loop;
  // This shouldn't crash.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearCodeCache, partition, base::Time(), base::Time(),
                     base::RepeatingCallback<bool(const GURL&)>(), &run_loop));
  run_loop.Run();
}

TEST(StoragePartitionImplStaticTest, CreatePredicateForHostCookies) {
  GURL url("http://www.example.com/");
  GURL url2("https://www.example.com/");
  GURL url3("https://www.google.com/");

  absl::optional<base::Time> server_time = absl::nullopt;
  CookieDeletionFilterPtr deletion_filter = CookieDeletionFilter::New();
  deletion_filter->host_name = url.host();

  base::Time now = base::Time::Now();
  std::vector<std::unique_ptr<CanonicalCookie>> valid_cookies;
  valid_cookies.push_back(CanonicalCookie::Create(
      url, "A=B", now, server_time, absl::nullopt /* cookie_partition_key */));
  valid_cookies.push_back(CanonicalCookie::Create(
      url, "C=F", now, server_time, absl::nullopt /* cookie_partition_key */));
  // We should match a different scheme with the same host.
  valid_cookies.push_back(CanonicalCookie::Create(
      url2, "A=B", now, server_time, absl::nullopt /* cookie_partition_key */));

  std::vector<std::unique_ptr<CanonicalCookie>> invalid_cookies;
  // We don't match domain cookies.
  invalid_cookies.push_back(
      CanonicalCookie::Create(url2, "A=B;domain=.example.com", now, server_time,
                              absl::nullopt /* cookie_partition_key */));
  invalid_cookies.push_back(CanonicalCookie::Create(
      url3, "A=B", now, server_time, absl::nullopt /* cookie_partition_key */));

  for (const auto& cookie : valid_cookies) {
    EXPECT_TRUE(FilterMatchesCookie(deletion_filter, *cookie))
        << cookie->DebugString();
  }
  for (const auto& cookie : invalid_cookies) {
    EXPECT_FALSE(FilterMatchesCookie(deletion_filter, *cookie))
        << cookie->DebugString();
  }
}

TEST_F(StoragePartitionImplTest, AttributionManagerCreatedInIncognito) {
  browser_context()->set_is_off_the_record(true);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());

  EXPECT_TRUE(partition->GetAttributionManager());
}

TEST_F(StoragePartitionImplTest, ConversionsClearDataForOrigin) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());

  AttributionManager* attribution_manager = partition->GetAttributionManager();

  base::Time now = base::Time::Now();
  auto source = SourceBuilder(now).SetExpiry(base::Days(2)).Build();
  attribution_manager->HandleSource(source, GlobalRenderFrameHostId());
  attribution_manager->HandleTrigger(DefaultTrigger(),
                                     GlobalRenderFrameHostId());

  base::RunLoop run_loop;
  partition->ClearData(
      StoragePartition::REMOVE_DATA_MASK_ATTRIBUTION_REPORTING_SITE_CREATED, 0,
      blink::StorageKey::CreateFirstParty(
          source.common_info().reporting_origin()),
      now, now, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(GetAttributionReportsForTesting(attribution_manager).empty());
}

TEST_F(StoragePartitionImplTest, ConversionsClearDataWrongMask) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());

  AttributionManager* attribution_manager = partition->GetAttributionManager();

  base::Time now = base::Time::Now();
  auto source = SourceBuilder(now).SetExpiry(base::Days(2)).Build();
  attribution_manager->HandleSource(source, GlobalRenderFrameHostId());
  attribution_manager->HandleTrigger(DefaultTrigger(),
                                     GlobalRenderFrameHostId());

  EXPECT_FALSE(GetAttributionReportsForTesting(attribution_manager).empty());

  // Arbitrary non-conversions mask.
  base::RunLoop run_loop;
  partition->ClearData(
      StoragePartition::REMOVE_DATA_MASK_COOKIES, 0,
      blink::StorageKey::CreateFirstParty(source.common_info().source_origin()),
      now, now, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(GetAttributionReportsForTesting(attribution_manager).empty());
}

TEST_F(StoragePartitionImplTest, ConversionsClearAllData) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());

  AttributionManager* attribution_manager = partition->GetAttributionManager();

  base::Time now = base::Time::Now();
  for (int i = 0; i < 20; i++) {
    auto origin = *SuitableOrigin::Deserialize(
        base::StringPrintf("https://www.%d.test/", i));
    auto source = SourceBuilder(now)
                      .SetExpiry(base::Days(2))
                      .SetSourceOrigin(origin)
                      .SetReportingOrigin(origin)
                      .SetDestinationSites({net::SchemefulSite(origin)})
                      .Build();
    attribution_manager->HandleSource(source, GlobalRenderFrameHostId());
  }
  base::RunLoop run_loop;
  partition->ClearData(
      StoragePartition::REMOVE_DATA_MASK_ATTRIBUTION_REPORTING_SITE_CREATED, 0,
      blink::StorageKey(), now, now, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(GetAttributionReportsForTesting(attribution_manager).empty());
}

TEST_F(StoragePartitionImplTest, ConversionsClearDataForFilter) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());

  AttributionManager* attribution_manager = partition->GetAttributionManager();

  base::Time now = base::Time::Now();
  for (int i = 0; i < 5; i++) {
    auto impression = *SuitableOrigin::Deserialize(
        base::StringPrintf("https://imp-%d.com/", i));
    auto reporter = *SuitableOrigin::Deserialize(
        base::StringPrintf("https://rep-%d.com/", i));
    auto conv = *SuitableOrigin::Deserialize(
        base::StringPrintf("https://conv-%d.com/", i));
    attribution_manager->HandleSource(
        SourceBuilder(now)
            .SetSourceOrigin(impression)
            .SetReportingOrigin(reporter)
            .SetDestinationSites({net::SchemefulSite(conv)})
            .SetExpiry(base::Days(2))
            .Build(),
        GlobalRenderFrameHostId());
    attribution_manager->HandleTrigger(TriggerBuilder()
                                           .SetDestinationOrigin(conv)
                                           .SetReportingOrigin(reporter)
                                           .Build(),
                                       GlobalRenderFrameHostId());
  }

  EXPECT_EQ(5u, GetAttributionReportsForTesting(attribution_manager).size());

  // Only those with a matching reporting origin should be deleted.
  base::RunLoop run_loop;
  auto filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kPreserve);
  StoragePartition::StorageKeyPolicyMatcherFunction func =
      base::BindRepeating([](const blink::StorageKey& storage_key,
                             storage::SpecialStoragePolicy* policy) {
        return storage_key == blink::StorageKey::CreateFromStringForTesting(
                                  "https://imp-2.com/") ||
               storage_key == blink::StorageKey::CreateFromStringForTesting(
                                  "https://conv-3.com/") ||
               storage_key == blink::StorageKey::CreateFromStringForTesting(
                                  "https://rep-4.com/");
      });
  partition->ClearData(
      StoragePartition::REMOVE_DATA_MASK_ATTRIBUTION_REPORTING_SITE_CREATED, 0,
      filter_builder.get(), func, nullptr, false, now, now,
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(4u, GetAttributionReportsForTesting(attribution_manager).size());
}

TEST_F(StoragePartitionImplTest, DataRemovalObserver) {
  const uint32_t kTestClearMask =
      content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
      content::StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  const uint32_t kTestQuotaClearMask = 0;
  const auto kTestOrigin = GURL("https://example.com");
  const auto kBeginTime = base::Time() + base::Hours(1);
  const auto kEndTime = base::Time() + base::Hours(2);
  const auto storage_key_callback_valid =
      [&](content::StoragePartition::StorageKeyMatcherFunction callback) {
        return callback.Run(blink::StorageKey::CreateFirstParty(
            url::Origin::Create(kTestOrigin)));
      };

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  MockDataRemovalObserver observer(partition);

  // Confirm that each of the StoragePartition interfaces for clearing origin
  // based data notify observers appropriately.
  EXPECT_CALL(observer,
              OnStorageKeyDataCleared(
                  kTestClearMask, testing::Truly(storage_key_callback_valid),
                  base::Time(), base::Time::Max()));
  base::RunLoop run_loop;
  partition->ClearDataForOrigin(kTestClearMask, kTestQuotaClearMask,
                                kTestOrigin, run_loop.QuitClosure());
  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer,
              OnStorageKeyDataCleared(
                  kTestClearMask, testing::Truly(storage_key_callback_valid),
                  kBeginTime, kEndTime));
  partition->ClearData(
      kTestClearMask, kTestQuotaClearMask,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestOrigin)),
      kBeginTime, kEndTime, base::DoNothing());
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer,
              OnStorageKeyDataCleared(
                  kTestClearMask, testing::Truly(storage_key_callback_valid),
                  kBeginTime, kEndTime));
  partition->ClearData(
      kTestClearMask, kTestQuotaClearMask,
      /*filter_builder=*/nullptr,
      base::BindLambdaForTesting([&](const blink::StorageKey& storage_key,
                                     storage::SpecialStoragePolicy* policy) {
        return storage_key == blink::StorageKey::CreateFirstParty(
                                  url::Origin::Create(kTestOrigin));
      }),
      /*cookie_deletion_filter=*/nullptr, /*perform_storage_cleanup=*/false,
      kBeginTime, kEndTime, base::DoNothing());
}

TEST_F(StoragePartitionImplTest, RemoveAggregationServiceData) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());

  auto aggregation_service = std::make_unique<MockAggregationService>();
  auto* aggregation_service_ptr = aggregation_service.get();
  partition->OverrideAggregationServiceForTesting(
      std::move(aggregation_service));

  const uint32_t kTestClearMask =
      StoragePartition::REMOVE_DATA_MASK_AGGREGATION_SERVICE;
  const uint32_t kTestQuotaClearMask =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL;
  const auto kTestOrigin = GURL("https://example.com");
  const auto kOtherOrigin = GURL("https://example.net");
  const auto kBeginTime = base::Time() + base::Hours(1);
  const auto kEndTime = base::Time() + base::Hours(2);
  const auto invoke_callback =
      [](base::Time delete_begin, base::Time delete_end,
         StoragePartition::StorageKeyMatcherFunction filter,
         base::OnceClosure done) { std::move(done).Run(); };
  const auto is_test_origin_valid =
      [&kTestOrigin](
          content::StoragePartition::StorageKeyMatcherFunction filter) {
        return filter.Run(blink::StorageKey::CreateFirstParty(
            url::Origin::Create(kTestOrigin)));
      };
  const auto is_other_origin_valid =
      [&kOtherOrigin](
          content::StoragePartition::StorageKeyMatcherFunction filter) {
        return filter.Run(blink::StorageKey::CreateFirstParty(
            url::Origin::Create(kOtherOrigin)));
      };
  const auto is_filter_null =
      [&](content::StoragePartition::StorageKeyMatcherFunction filter) {
        return filter.is_null();
      };

  // Verify that each of the StoragePartition interfaces for clearing origin
  // based data calls aggregation service appropriately.
  EXPECT_CALL(
      *aggregation_service_ptr,
      ClearData(
          base::Time(), base::Time::Max(),
          testing::AllOf(testing::Truly(is_test_origin_valid),
                         testing::Not(testing::Truly(is_other_origin_valid))),
          testing::_))
      .WillOnce(invoke_callback);
  {
    base::RunLoop run_loop;
    partition->ClearDataForOrigin(kTestClearMask, kTestQuotaClearMask,
                                  kTestOrigin, run_loop.QuitClosure());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(aggregation_service_ptr);
  }

  EXPECT_CALL(
      *aggregation_service_ptr,
      ClearData(
          kBeginTime, kEndTime,
          testing::AllOf(testing::Truly(is_test_origin_valid),
                         testing::Not(testing::Truly(is_other_origin_valid))),
          testing::_))
      .WillOnce(testing::Invoke(invoke_callback));
  {
    base::RunLoop run_loop;
    partition->ClearData(
        kTestClearMask, kTestQuotaClearMask,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestOrigin)),
        kBeginTime, kEndTime, run_loop.QuitClosure());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(aggregation_service_ptr);
  }

  EXPECT_CALL(
      *aggregation_service_ptr,
      ClearData(
          kBeginTime, kEndTime,
          testing::AllOf(testing::Truly(is_test_origin_valid),
                         testing::Not(testing::Truly(is_other_origin_valid))),
          testing::_))
      .WillOnce(testing::Invoke(invoke_callback));
  {
    base::RunLoop run_loop;
    partition->ClearData(
        kTestClearMask, kTestQuotaClearMask,
        /*filter_builder=*/nullptr,
        base::BindLambdaForTesting([&](const blink::StorageKey& storage_key,
                                       storage::SpecialStoragePolicy* policy) {
          return storage_key == blink::StorageKey::CreateFirstParty(
                                    url::Origin::Create(kTestOrigin));
        }),
        /*cookie_deletion_filter=*/nullptr,
        /*perform_storage_cleanup=*/false, kBeginTime, kEndTime,
        run_loop.QuitClosure());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(aggregation_service_ptr);
  }

  EXPECT_CALL(
      *aggregation_service_ptr,
      ClearData(
          kBeginTime, kEndTime,
          testing::AllOf(testing::Truly(is_test_origin_valid),
                         testing::Not(testing::Truly(is_other_origin_valid))),
          testing::_))
      .WillOnce(testing::Invoke(invoke_callback));
  {
    base::RunLoop run_loop;
    auto filter_builder = BrowsingDataFilterBuilder::Create(
        BrowsingDataFilterBuilder::Mode::kDelete);
    filter_builder->AddOrigin(url::Origin::Create(kTestOrigin));
    partition->ClearData(kTestClearMask, kTestQuotaClearMask,
                         filter_builder.get(),
                         StoragePartition::StorageKeyPolicyMatcherFunction(),
                         /*cookie_deletion_filter=*/nullptr,
                         /*perform_storage_cleanup=*/false, kBeginTime,
                         kEndTime, run_loop.QuitClosure());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(aggregation_service_ptr);
  }

  EXPECT_CALL(*aggregation_service_ptr,
              ClearData(kBeginTime, kEndTime, testing::Truly(is_filter_null),
                        testing::_))
      .WillOnce(testing::Invoke(invoke_callback));
  {
    base::RunLoop run_loop;
    partition->ClearData(kTestClearMask, kTestQuotaClearMask,
                         blink::StorageKey(), kBeginTime, kEndTime,
                         run_loop.QuitClosure());
    run_loop.Run();
  }
}

TEST_F(StoragePartitionImplTest, RemovePrivateAggregationData) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());

  auto private_aggregation_manager =
      std::make_unique<MockPrivateAggregationManager>();
  auto* private_aggregation_manager_ptr = private_aggregation_manager.get();
  partition->OverridePrivateAggregationManagerForTesting(
      std::move(private_aggregation_manager));

  const uint32_t kTestClearMask =
      StoragePartition::REMOVE_DATA_MASK_PRIVATE_AGGREGATION_INTERNAL;
  const uint32_t kTestQuotaClearMask =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL;
  const auto kTestOrigin = GURL("https://example.com");
  const auto kOtherOrigin = GURL("https://example.net");
  const auto kBeginTime = base::Time() + base::Hours(1);
  const auto kEndTime = base::Time() + base::Hours(2);
  const auto invoke_callback =
      [](base::Time delete_begin, base::Time delete_end,
         StoragePartition::StorageKeyMatcherFunction filter,
         base::OnceClosure done) { std::move(done).Run(); };
  const auto is_test_origin_valid =
      [&kTestOrigin](
          content::StoragePartition::StorageKeyMatcherFunction filter) {
        return filter.Run(blink::StorageKey::CreateFirstParty(
            url::Origin::Create(kTestOrigin)));
      };
  const auto is_other_origin_valid =
      [&kOtherOrigin](
          content::StoragePartition::StorageKeyMatcherFunction filter) {
        return filter.Run(blink::StorageKey::CreateFirstParty(
            url::Origin::Create(kOtherOrigin)));
      };
  const auto is_filter_null =
      [&](content::StoragePartition::StorageKeyMatcherFunction filter) {
        return filter.is_null();
      };

  // Verify that each of the StoragePartition interfaces for clearing origin
  // based data calls aggregation service appropriately.
  EXPECT_CALL(
      *private_aggregation_manager_ptr,
      ClearBudgetData(
          base::Time(), base::Time::Max(),
          testing::AllOf(testing::Truly(is_test_origin_valid),
                         testing::Not(testing::Truly(is_other_origin_valid))),
          testing::_))
      .WillOnce(invoke_callback);
  {
    base::RunLoop run_loop;
    partition->ClearDataForOrigin(kTestClearMask, kTestQuotaClearMask,
                                  kTestOrigin, run_loop.QuitClosure());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(private_aggregation_manager_ptr);
  }

  EXPECT_CALL(
      *private_aggregation_manager_ptr,
      ClearBudgetData(
          kBeginTime, kEndTime,
          testing::AllOf(testing::Truly(is_test_origin_valid),
                         testing::Not(testing::Truly(is_other_origin_valid))),
          testing::_))
      .WillOnce(testing::Invoke(invoke_callback));
  {
    base::RunLoop run_loop;
    partition->ClearData(
        kTestClearMask, kTestQuotaClearMask,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestOrigin)),
        kBeginTime, kEndTime, run_loop.QuitClosure());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(private_aggregation_manager_ptr);
  }

  EXPECT_CALL(
      *private_aggregation_manager_ptr,
      ClearBudgetData(
          kBeginTime, kEndTime,
          testing::AllOf(testing::Truly(is_test_origin_valid),
                         testing::Not(testing::Truly(is_other_origin_valid))),
          testing::_))
      .WillOnce(testing::Invoke(invoke_callback));
  {
    base::RunLoop run_loop;
    partition->ClearData(
        kTestClearMask, kTestQuotaClearMask,
        /*filter_builder=*/nullptr,
        base::BindLambdaForTesting([&](const blink::StorageKey& storage_key,
                                       storage::SpecialStoragePolicy* policy) {
          return storage_key == blink::StorageKey::CreateFirstParty(
                                    url::Origin::Create(kTestOrigin));
        }),
        /*cookie_deletion_filter=*/nullptr,
        /*perform_storage_cleanup=*/false, kBeginTime, kEndTime,
        run_loop.QuitClosure());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(private_aggregation_manager_ptr);
  }

  EXPECT_CALL(*private_aggregation_manager_ptr,
              ClearBudgetData(kBeginTime, kEndTime,
                              testing::Truly(is_filter_null), testing::_))
      .WillOnce(testing::Invoke(invoke_callback));
  {
    base::RunLoop run_loop;
    partition->ClearData(kTestClearMask, kTestQuotaClearMask,
                         blink::StorageKey(), kBeginTime, kEndTime,
                         run_loop.QuitClosure());
    run_loop.Run();
  }
}

// https://crbug.com/1221382
// Make sure StorageServiceImpl can be stored in a SequenceLocalStorageSlot and
// that it can be safely destroyed when the thread terminates.
TEST(StorageServiceImplOnSequenceLocalStorage, ThreadDestructionDoesNotFail) {
  mojo::Remote<storage::mojom::StorageService> remote_service;
  mojo::Remote<storage::mojom::Partition> persistent_partition;
  mojo::Remote<storage::mojom::LocalStorageControl> storage_control;
  // These remotes must outlive the thread, otherwise PartitionImpl cleanup will
  // not happen in the ~StorageServiceImpl but on the mojo error handler.
  {
    // When this variable gets out of scope the IO thread will be destroyed
    // along with all objects stored in a SequenceLocalStorageSlot.
    content::BrowserTaskEnvironment task_environment(
        content::BrowserTaskEnvironment::REAL_IO_THREAD);

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::PendingReceiver<storage::mojom::StorageService> receiver) {
              DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
              static base::SequenceLocalStorageSlot<
                  std::unique_ptr<storage::StorageServiceImpl>>
                  service_storage_slot;
              service_storage_slot.GetOrCreateValue() =
                  std::make_unique<storage::StorageServiceImpl>(
                      std::move(receiver),
                      /*io_task_runner=*/nullptr);
            },
            remote_service.BindNewPipeAndPassReceiver()));

    // Make sure PartitionImpl gets to destroy a LocalStorageImpl object.
    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());
    remote_service->BindPartition(
        temp_dir.GetPath(), persistent_partition.BindNewPipeAndPassReceiver());
    persistent_partition->BindLocalStorageControl(
        storage_control.BindNewPipeAndPassReceiver());
    storage_control.FlushForTesting();
  }
}

class StoragePartitionImplSharedStorageTest : public StoragePartitionImplTest {
 public:
  StoragePartitionImplSharedStorageTest()
      : storage_partition_(browser_context()->GetDefaultStoragePartition()),
        shared_storage_manager_(
            static_cast<StoragePartitionImpl*>(storage_partition_)
                ->GetSharedStorageManager()) {
    feature_list_.InitWithFeatures({blink::features::kInterestGroupStorage,
                                    blink::features::kSharedStorageAPI},
                                   {});
  }

  StoragePartitionImplSharedStorageTest(
      const StoragePartitionImplSharedStorageTest&) = delete;
  StoragePartitionImplSharedStorageTest& operator=(
      const StoragePartitionImplSharedStorageTest&) = delete;

  ~StoragePartitionImplSharedStorageTest() override {
    task_environment()->RunUntilIdle();
  }

  scoped_refptr<storage::SpecialStoragePolicy> GetSpecialStoragePolicy() {
    return base::WrapRefCounted<storage::SpecialStoragePolicy>(
        static_cast<content::StoragePartitionImpl*>(storage_partition_)
            ->browser_context()
            ->GetSpecialStoragePolicy());
  }

  // Returns true, if the given origin URL exists.
  bool SharedStorageExistsForOrigin(const url::Origin& origin) {
    for (const auto& info : GetSharedStorageUsage()) {
      if (origin == info->storage_key.origin())
        return true;
    }
    return false;
  }

  void AddSharedStorageTestData(const url::Origin& origin1,
                                const url::Origin& origin2,
                                const url::Origin& origin3) {
    base::FilePath path =
        storage_partition_->GetPath().Append(storage::kSharedStoragePath);
    std::unique_ptr<storage::AsyncSharedStorageDatabase> database =
        storage::AsyncSharedStorageDatabaseImpl::Create(
            path,
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::WithBaseSyncPrimitives(),
                 base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
            GetSpecialStoragePolicy(),
            storage::SharedStorageOptions::Create()->GetDatabaseOptions());

    base::test::TestFuture<bool> future;

    DCHECK(database);
    DCHECK(static_cast<storage::AsyncSharedStorageDatabaseImpl*>(database.get())
               ->GetSequenceBoundDatabaseForTesting());
    static_cast<storage::AsyncSharedStorageDatabaseImpl*>(database.get())
        ->GetSequenceBoundDatabaseForTesting()
        ->AsyncCall(&storage::SharedStorageDatabase::PopulateDatabaseForTesting)
        .WithArgs(origin1, origin2, origin3)
        .Then(future.GetCallback());

    EXPECT_TRUE(future.Get());

    // Ensure that this database is fully closed before checking for existence.
    database.reset();
    task_environment()->RunUntilIdle();

    EXPECT_TRUE(SharedStorageExistsForOrigin(origin1));
    EXPECT_TRUE(SharedStorageExistsForOrigin(origin2));
    EXPECT_TRUE(SharedStorageExistsForOrigin(origin3));

    task_environment()->RunUntilIdle();
  }

 private:
  std::vector<storage::mojom::StorageUsageInfoPtr> GetSharedStorageUsage() {
    DCHECK(shared_storage_manager_);

    base::test::TestFuture<std::vector<storage::mojom::StorageUsageInfoPtr>>
        future;
    shared_storage_manager_->FetchOrigins(future.GetCallback());
    return future.Take();
  }

  base::test::ScopedFeatureList feature_list_;

  // We don't own these pointers.
  const raw_ptr<StoragePartition> storage_partition_;
  raw_ptr<storage::SharedStorageManager> shared_storage_manager_;
};

TEST_F(StoragePartitionImplSharedStorageTest,
       RemoveUnprotectedSharedStorageForever) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  // Protect kOrigin1.
  auto mock_policy = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
  mock_policy->AddProtected(kOrigin1.GetURL());

  AddSharedStorageTestData(kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->GetSharedStorageManager()->OverrideSpecialStoragePolicyForTesting(
      mock_policy.get());

  base::RunLoop clear_run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearStuff,
                     StoragePartitionImpl::REMOVE_DATA_MASK_SHARED_STORAGE,
                     partition, base::Time(), base::Time::Max(),
                     /*filter_builder=*/nullptr,
                     base::BindRepeating(&DoesOriginMatchForUnprotectedWeb),
                     &clear_run_loop));
  clear_run_loop.Run();

  // ClearData only guarantees that tasks to delete data are scheduled when its
  // callback is invoked. It doesn't guarantee data has actually been cleared.
  // So run all scheduled tasks to make sure data is cleared.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(SharedStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(SharedStorageExistsForOrigin(kOrigin2));
  EXPECT_FALSE(SharedStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplSharedStorageTest,
       RemoveProtectedSharedStorageForever) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  // Protect kOrigin1.
  auto mock_policy = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
  mock_policy->AddProtected(kOrigin1.GetURL());

  AddSharedStorageTestData(kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  partition->GetSharedStorageManager()->OverrideSpecialStoragePolicyForTesting(
      mock_policy.get());

  base::RunLoop clear_run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearStuff,
                     StoragePartitionImpl::REMOVE_DATA_MASK_SHARED_STORAGE,
                     partition, base::Time(), base::Time::Max(),
                     /*filter_builder=*/nullptr,
                     base::BindRepeating(
                         &DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                     &clear_run_loop));
  clear_run_loop.Run();

  // ClearData only guarantees that tasks to delete data are scheduled when its
  // callback is invoked. It doesn't guarantee data has actually been cleared.
  // So run all scheduled tasks to make sure data is cleared.
  base::RunLoop().RunUntilIdle();

  // Even if kOrigin1 is protected, it will be deleted since we specify
  // ClearData to delete protected data.
  EXPECT_FALSE(SharedStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(SharedStorageExistsForOrigin(kOrigin2));
  EXPECT_FALSE(SharedStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplSharedStorageTest, RemoveSharedStorageForLastWeek) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  AddSharedStorageTestData(kOrigin1, kOrigin2, kOrigin3);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  DCHECK(partition);
  base::Time a_week_ago = base::Time::Now() - base::Days(7);

  base::RunLoop clear_run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearStuff,
                     StoragePartitionImpl::REMOVE_DATA_MASK_SHARED_STORAGE,
                     partition, a_week_ago, base::Time::Max(),
                     /*filter_builder=*/nullptr,
                     base::BindRepeating(
                         &DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                     &clear_run_loop));
  clear_run_loop.Run();

  // ClearData only guarantees that tasks to delete data are scheduled when its
  // callback is invoked. It doesn't guarantee data has actually been cleared.
  // So run all scheduled tasks to make sure data is cleared.
  base::RunLoop().RunUntilIdle();

  // kOrigin1 and kOrigin2 do not have age more than a week.
  EXPECT_FALSE(SharedStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(SharedStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(SharedStorageExistsForOrigin(kOrigin3));
}

}  // namespace content
