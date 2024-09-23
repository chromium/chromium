// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registry.h"

#include <type_traits>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "components/services/storage/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_router_evaluator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

// When this is enabled, the browser will schedule
// ServiceWorkerStorageControl's response in a kHighest priority
// queue during startup. After startup, it has a normal priority.
BASE_FEATURE(kServiceWorkerStorageControlResponseQueue,
             "ServiceWorkerStorageControlResponseQueue",
             base::FEATURE_ENABLED_BY_DEFAULT);

blink::ServiceWorkerStatusCode DatabaseStatusToStatusCode(
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  switch (status) {
    case storage::mojom::ServiceWorkerDatabaseStatus::kOk:
      return blink::ServiceWorkerStatusCode::kOk;
    case storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound:
      return blink::ServiceWorkerStatusCode::kErrorNotFound;
    case storage::mojom::ServiceWorkerDatabaseStatus::kErrorDisabled:
      return blink::ServiceWorkerStatusCode::kErrorAbort;
    case storage::mojom::ServiceWorkerDatabaseStatus::kErrorStorageDisconnected:
      return blink::ServiceWorkerStatusCode::kErrorStorageDisconnected;
    case storage::mojom::ServiceWorkerDatabaseStatus::kErrorCorrupted:
      return blink::ServiceWorkerStatusCode::kErrorStorageDataCorrupted;
    default:
      return blink::ServiceWorkerStatusCode::kErrorFailed;
  }
}

void RunSoon(const base::Location& from_here, base::OnceClosure closure) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      from_here, std::move(closure));
}

void CompleteFindNow(scoped_refptr<ServiceWorkerRegistration> registration,
                     blink::ServiceWorkerStatusCode status,
                     ServiceWorkerRegistry::FindRegistrationCallback callback) {
  if (registration && registration->is_deleted()) {
    // It's past the point of no return and no longer findable.
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorNotFound,
                            nullptr);
    return;
  }
  std::move(callback).Run(status, std::move(registration));
}

void CompleteFindSoon(
    const base::Location& from_here,
    scoped_refptr<ServiceWorkerRegistration> registration,
    blink::ServiceWorkerStatusCode status,
    ServiceWorkerRegistry::FindRegistrationCallback callback) {
  RunSoon(from_here, base::BindOnce(&CompleteFindNow, std::move(registration),
                                    status, std::move(callback)));
}

void RecordRetryCount(size_t retries, size_t queue_size) {
  base::UmaHistogramCounts100("ServiceWorker.Storage.RetryCountForRecovery",
                              retries);
}

// Notifies quota manager that a disk write operation failed so that it can
// check for storage pressure.
void CheckForClientWriteFailure(
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    const blink::StorageKey& key) {
  if (!quota_manager_proxy)
    return;

  if (status == storage::mojom::ServiceWorkerDatabaseStatus::kErrorFailed ||
      status == storage::mojom::ServiceWorkerDatabaseStatus::kErrorIOError) {
    quota_manager_proxy->OnClientWriteFailed(key);
  }
}

void EraseRegistrationIdCacheEntry(
    base::LRUCache<std::tuple<GURL, blink::StorageKey>, int64_t>&
        registration_id_cache,
    const GURL& scope,
    const blink::StorageKey& key) {
  auto iter = registration_id_cache.Get(std::make_pair(scope, key));
  if (iter != registration_id_cache.end()) {
    registration_id_cache.Erase(iter);
  }
}

void FindRegistrationForClientUrlTraceEventBegin(int64_t trace_event_id,
                                                 const GURL& client_url) {
  CHECK(client_url.is_valid());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "ServiceWorker", "ServiceWorkerRegistry::FindRegistrationForClientUrl",
      TRACE_ID_WITH_SCOPE("ServiceWorkerRegistry::FindRegistrationForClientUrl",
                          trace_event_id),
      "URL", client_url.spec());
}

void FindRegistrationForClientUrlTraceEventEnd(
    int64_t trace_event_id,
    blink::ServiceWorkerStatusCode status,
    std::optional<std::string> info) {
  if (info) {
    TRACE_EVENT_NESTABLE_ASYNC_END2(
        "ServiceWorker", "ServiceWorkerRegistry::FindRegistrationForClientUrl",
        TRACE_ID_WITH_SCOPE(
            "ServiceWorkerRegistry::FindRegistrationForClientUrl",
            trace_event_id),
        "Status", blink::ServiceWorkerStatusToString(status), "Info", *info);
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "ServiceWorker", "ServiceWorkerRegistry::FindRegistrationForClientUrl",
        TRACE_ID_WITH_SCOPE(
            "ServiceWorkerRegistry::FindRegistrationForClientUrl",
            trace_event_id),
        "Status", blink::ServiceWorkerStatusToString(status));
  }
}

std::string RouterRulesToString(blink::ServiceWorkerRouterRules rules) {
  ServiceWorkerRouterEvaluator e(rules);
  if (!e.IsValid()) {
    return "data corruption detected";
  }
  return e.ToString();
}

// The cache size for ServiceWorker's scope cache (https://crbug.com/40254732).
constexpr size_t kServiceWorkerScopeCacheLimitSize = 100;

// The cache size for ServiceWorker's registration cache
// (https://crbug.com/40254732).
constexpr size_t kServiceWorkerRegistrationCacheSize = 100;

}  // namespace

template <typename... ReplyArgs>
class InflightCallWithInvoker final
    : public ServiceWorkerRegistry::InflightCall {
 public:
  using ReplyCallback = base::OnceCallback<void(ReplyArgs...)>;

  // `invoker` is a callback that sends a Mojo IPC over a `mojo::Remote` owned
  // by `registry`.
  explicit InflightCallWithInvoker(
      ServiceWorkerRegistry* registry,
      base::RepeatingCallback<void(InflightCallWithInvoker*, ReplyCallback)>
          invoker,
      ReplyCallback reply_callback)
      : registry_(registry),
        invoker_(invoker),
        reply_callback_(std::move(reply_callback)) {}

  ~InflightCallWithInvoker() override = default;

  void Run() override {
    // Insert a reply trampoline to mark the call as completed when the reply
    // callback is dispatched. `Unretained` is safe: the Mojo IPC endpoint is
    // owned by `registry_`, which also owns `this`. If the Mojo IPC endpoint is
    // destroyed, Mojo guarantees the reply callback will not be dispatched.
    invoker_.Run(this, base::BindOnce(&InflightCallWithInvoker::DidReply,
                                      base::Unretained(this)));
  }

  ServiceWorkerRegistry* registry() { return registry_; }

 private:
  void DidReply(ReplyArgs... reply_args) {
    auto reply_callback = std::move(reply_callback_);
    // Deletes `this`.
    registry_->FinishRemoteCall(this);
    std::move(reply_callback).Run(std::move(reply_args)...);
  }

  // `registry_` owns `this`
  const raw_ptr<ServiceWorkerRegistry> registry_;
  const base::RepeatingCallback<void(InflightCallWithInvoker*, ReplyCallback)>
      invoker_;
  base::OnceCallback<void(ReplyArgs...)> reply_callback_;
};

ServiceWorkerRegistry::ServiceWorkerRegistry(
    ServiceWorkerContextCore* context,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy)
    : context_(context),
      quota_manager_proxy_(quota_manager_proxy),
      special_storage_policy_(special_storage_policy),
      registration_scope_cache_(kServiceWorkerScopeCacheLimitSize),
      registration_id_cache_(kServiceWorkerRegistrationCacheSize) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_);
  Start();
}

ServiceWorkerRegistry::ServiceWorkerRegistry(
    ServiceWorkerContextCore* context,
    ServiceWorkerRegistry* old_registry)
    : ServiceWorkerRegistry(context,
                            old_registry->quota_manager_proxy_.get(),
                            old_registry->special_storage_policy_.get()) {}

ServiceWorkerRegistry::~ServiceWorkerRegistry() = default;

void ServiceWorkerRegistry::CreateNewRegistration(
    blink::mojom::ServiceWorkerRegistrationOptions options,
    const blink::StorageKey& key,
    blink::mojom::AncestorFrameType ancestor_frame_type,
    NewRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (quota_manager_proxy_) {
    // Can be nullptr in tests.
    quota_manager_proxy_->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(key),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::BindOnce(
            &ServiceWorkerRegistry::CreateNewRegistrationWithBucketInfo,
            weak_factory_.GetWeakPtr(), std::move(options), key,
            ancestor_frame_type, std::move(callback)));
  } else {
    CreateInvokerAndStartRemoteCall(
        &storage::mojom::ServiceWorkerStorageControl::GetNewRegistrationId,
        base::BindOnce(&ServiceWorkerRegistry::DidGetNewRegistrationId,
                       weak_factory_.GetWeakPtr(), std::move(options), key,
                       ancestor_frame_type, std::move(callback)));
  }
}

void ServiceWorkerRegistry::CreateNewRegistrationWithBucketInfo(
    blink::mojom::ServiceWorkerRegistrationOptions options,
    const blink::StorageKey& key,
    blink::mojom::AncestorFrameType ancestor_frame_type,
    NewRegistrationCallback callback,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  // Return nullptr if `UpdateOrCreateBucket` fails.
  if (!result.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::GetNewRegistrationId,
      base::BindOnce(&ServiceWorkerRegistry::DidGetNewRegistrationId,
                     weak_factory_.GetWeakPtr(), std::move(options), key,
                     ancestor_frame_type, std::move(callback)));
}

void ServiceWorkerRegistry::CreateNewVersion(
    scoped_refptr<ServiceWorkerRegistration> registration,
    const GURL& script_url,
    blink::mojom::ScriptType script_type,
    NewVersionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(registration);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::GetNewVersionId,
      base::BindOnce(&ServiceWorkerRegistry::DidGetNewVersionId,
                     weak_factory_.GetWeakPtr(), registration, script_url,
                     script_type, std::move(callback)));
}

void ServiceWorkerRegistry::FindRegistrationForClientUrl(
    Purpose purpose,
    const GURL& client_url,
    const blink::StorageKey& key,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // `registration_scope_cache_` provides a full list of scope URLs for the
  // given blink::StorageKey if the matched entry exists. There are the
  // following two scenarios after running the ServiceWorkerLongestScopeMatcher
  // algorithm.
  //
  // If there is a key in `registration_scope_cache_`, and if there is no
  // matched scope in it, this means that there is no registration for the
  // `client_url`. (https://crbug.com/1411197)
  //
  // If there is a key in `registration_scope_cache_`, and if there is a matched
  // scope in it, we can look up `registration_id_cache_`. If we can get
  // registration, we can immediately complete with CompleteFindNow() without
  // calling FindRegistrationForClientUrl() mojo API.
  // (https://crbug.com/1446216)
  bool no_registration = false;
  std::optional<GURL> matched_scope;
  std::optional<std::set<GURL>> scopes;
  auto iter = registration_scope_cache_.Get(key);
  if (iter != registration_scope_cache_.end()) {
    scopes = iter->second;
    blink::ServiceWorkerLongestScopeMatcher matcher(client_url);
    for (const GURL& scope : *scopes) {
      if (matcher.MatchLongest(scope)) {
        matched_scope = scope;
      }
    }
    no_registration = !matched_scope;
  }
  if (purpose == Purpose::kNavigation) {
    base::UmaHistogramBoolean(
        "ServiceWorker.FindRegistrationForClientUrl.SkippedMojoCall."
        "OnNavigation",
        no_registration);
  }
  base::UmaHistogramBoolean(
      "ServiceWorker.FindRegistrationForClientUrl.IsCalledForNavigation",
      purpose == Purpose::kNavigation);
  // To connect this TRACE_EVENT with the callback, Time::Now() is used as a
  // trace event id.
  int64_t trace_event_id =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerRegistry::FindRegistrationForClientUrl",
      TRACE_ID_WITH_SCOPE("ServiceWorkerRegistry", trace_event_id),
      TRACE_EVENT_FLAG_FLOW_OUT, "URL", client_url.spec());
  if (matched_scope) {
    auto it = registration_id_cache_.Get(std::make_pair(*matched_scope, key));
    if (it != registration_id_cache_.end()) {
      int64_t registration_id = it->second;
      std::optional<scoped_refptr<ServiceWorkerRegistration>> registration =
          FindFromLiveRegistrationsForId(registration_id);
      if (registration) {
        FindRegistrationForClientUrlTraceEventBegin(trace_event_id, client_url);
        FindRegistrationForClientUrlTraceEventEnd(
            trace_event_id, blink::ServiceWorkerStatusCode::kOk, std::nullopt);
        CompleteFindNow(std::move(*registration),
                        blink::ServiceWorkerStatusCode::kOk,
                        std::move(callback));
        return;
      }
    }
  }
  std::vector<FindRegistrationCallback>& callbacks =
      find_registration_callbacks_[std::make_pair(client_url, key)];
  callbacks.push_back(std::move(callback));
  if (callbacks.size() >= 2) {
    // Merges duplicate requests into the preceding in-flight request.
    return;
  }
  FindRegistrationForClientUrlTraceEventBegin(trace_event_id, client_url);
  if (no_registration) {
    DidFindRegistrationForClientUrl(
        client_url, key, trace_event_id,
        // Pass a fake callback here as the proper callback will
        // be invoked via find_registration_callbacks_
        /*callback=*/base::DoNothing(),
        storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound, nullptr,
        std::vector(scopes->begin(), scopes->end()));
    return;
  }
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::
          FindRegistrationForClientUrl,
      base::BindOnce(&ServiceWorkerRegistry::DidFindRegistrationForClientUrl,
                     weak_factory_.GetWeakPtr(), client_url, key,
                     trace_event_id,
                     // Pass a fake callback here as the proper callback will
                     // be invoked via find_registration_callbacks_
                     /*callback=*/base::DoNothing()),
      client_url, key);
}

void ServiceWorkerRegistry::FindRegistrationForScope(
    const GURL& scope,
    const blink::StorageKey& key,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_storage_disabled_) {
    RunSoon(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kErrorAbort, nullptr));
    return;
  }

  // Look up installing registration before checking storage.
  scoped_refptr<ServiceWorkerRegistration> installing_registration =
      FindInstallingRegistrationForScope(scope, key);
  if (installing_registration && !installing_registration->is_deleted()) {
    CompleteFindSoon(FROM_HERE, std::move(installing_registration),
                     blink::ServiceWorkerStatusCode::kOk, std::move(callback));
    return;
  }

  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::FindRegistrationForScope,
      base::BindOnce(&ServiceWorkerRegistry::DidFindRegistrationForScope,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      scope, key);
}

void ServiceWorkerRegistry::FindRegistrationForId(
    int64_t registration_id,
    const blink::StorageKey& key,
    FindRegistrationCallback callback) {
  FindRegistrationForIdInternal(registration_id, key, std::move(callback));
}

void ServiceWorkerRegistry::FindRegistrationForIdOnly(
    int64_t registration_id,
    FindRegistrationCallback callback) {
  FindRegistrationForIdInternal(registration_id, /*key=*/std::nullopt,
                                std::move(callback));
}

void ServiceWorkerRegistry::GetRegistrationsForStorageKey(
    const blink::StorageKey& key,
    GetRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::
          GetRegistrationsForStorageKey,
      base::BindOnce(&ServiceWorkerRegistry::DidGetRegistrationsForStorageKey,
                     weak_factory_.GetWeakPtr(), std::move(callback), key),
      key);
}

void ServiceWorkerRegistry::GetStorageUsageForStorageKey(
    const blink::StorageKey& key,
    GetStorageUsageForStorageKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), blink::ServiceWorkerStatusCode::kErrorFailed, 0);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::GetUsageForStorageKey,
      base::BindOnce(&ServiceWorkerRegistry::DidGetStorageUsageForStorageKey,
                     weak_factory_.GetWeakPtr(), std::move(wrapped_callback)),
      key);
}

void ServiceWorkerRegistry::GetAllRegistrationsInfos(
    GetRegistrationsInfosCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::
          GetAllRegistrationsDeprecated,
      base::BindOnce(&ServiceWorkerRegistry::DidGetAllRegistrations,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

ServiceWorkerRegistration* ServiceWorkerRegistry::GetUninstallingRegistration(
    const GURL& scope,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(bashi): Should we check state of ServiceWorkerStorage?
  for (const auto& registration : uninstalling_registrations_) {
    if (registration.second->key() == key &&
        registration.second->scope() == scope) {
      DCHECK(registration.second->is_uninstalling());
      return registration.second.get();
    }
  }
  return nullptr;
}

std::vector<scoped_refptr<ServiceWorkerRegistration>>
ServiceWorkerRegistry::GetUninstallingRegistrationsForStorageKey(
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<scoped_refptr<ServiceWorkerRegistration>> results;
  for (const auto& registration : uninstalling_registrations_) {
    if (registration.second->key() == key) {
      results.push_back(registration.second);
    }
  }
  return results;
}

void ServiceWorkerRegistry::StoreRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(registration);
  DCHECK(version);

  if (is_storage_disabled_) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }

  DCHECK_NE(version->fetch_handler_existence(),
            ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN);
  DCHECK_EQ(registration->status(), ServiceWorkerRegistration::Status::kIntact);

  auto data = storage::mojom::ServiceWorkerRegistrationData::New();
  data->registration_id = registration->id();
  data->scope = registration->scope();
  data->key = registration->key();
  data->script = version->script_url();
  data->script_type = version->script_type();
  data->update_via_cache = registration->update_via_cache();
  data->fetch_handler_type = version->fetch_handler_type();
  data->version_id = version->version_id();
  data->last_update_check = registration->last_update_check();
  data->is_active = (version == registration->active_version());
  if (version->origin_trial_tokens())
    data->origin_trial_tokens = *version->origin_trial_tokens();
  data->navigation_preload_state = blink::mojom::NavigationPreloadState::New();
  data->navigation_preload_state->enabled =
      registration->navigation_preload_state().enabled;
  data->navigation_preload_state->header =
      registration->navigation_preload_state().header;
  data->script_response_time = version->GetInfo().script_response_time;
  for (const blink::mojom::WebFeature feature : version->used_features())
    data->used_features.push_back(feature);
  data->ancestor_frame_type = registration->ancestor_frame_type();
  if (version->router_evaluator()) {
    data->router_rules = version->router_evaluator()->rules();
  }

  // The ServiceWorkerVersion's policy container host might be null if it is
  // stored before loading the main script. This happens in many unittests.
  data->policy_container_policies =
      version->policy_container_host()
          ? version->policy_container_host()
                ->policies()
                .ToMojoPolicyContainerPolicies()
          : blink::mojom::PolicyContainerPolicies::New();

  data->has_hid_event_handlers = version->has_hid_event_handlers();
  data->has_usb_event_handlers = version->has_usb_event_handlers();

  ResourceList resources = version->script_cache_map()->GetResources();
  if (resources.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }

  uint64_t resources_total_size_bytes = 0;
  for (const auto& resource : resources) {
    DCHECK_GE(resource->size_bytes, 0);
    resources_total_size_bytes += resource->size_bytes;
  }
  data->resources_total_size_bytes = resources_total_size_bytes;

  EraseRegistrationIdCacheEntry(registration_id_cache_, registration->scope(),
                                registration->key());

  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::StoreRegistration,
      base::BindOnce(&ServiceWorkerRegistry::DidStoreRegistration,
                     weak_factory_.GetWeakPtr(), registration->id(),
                     resources_total_size_bytes, registration->scope(),
                     registration->key(), std::move(callback)),
      std::move(data), std::move(resources));
}

void ServiceWorkerRegistry::DeleteRegistration(
    scoped_refptr<ServiceWorkerRegistration> registration,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_storage_disabled_) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }

  DCHECK(!registration->is_deleted())
      << "attempt to delete a registration twice";

  EraseRegistrationIdCacheEntry(registration_id_cache_, registration->scope(),
                                registration->key());

  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::DeleteRegistration,
      base::BindOnce(&ServiceWorkerRegistry::DidDeleteRegistration,
                     weak_factory_.GetWeakPtr(), registration->id(),
                     registration->scope(), registration->key(),
                     std::move(callback)),
      registration->id(), registration->key());

  DCHECK(!base::Contains(uninstalling_registrations_, registration->id()));
  uninstalling_registrations_[registration->id()] = registration;
  registration->SetStatus(ServiceWorkerRegistration::Status::kUninstalling);
}

void ServiceWorkerRegistry::NotifyInstallingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(installing_registrations_.find(registration->id()) ==
         installing_registrations_.end());
  installing_registrations_[registration->id()] = registration;
}

void ServiceWorkerRegistry::NotifyDoneInstallingRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  installing_registrations_.erase(registration->id());
  if (status != blink::ServiceWorkerStatusCode::kOk && version) {
    ResourceList resources = version->script_cache_map()->GetResources();
    std::vector<int64_t> resource_ids;
    for (const auto& resource : resources)
      resource_ids.push_back(resource->resource_id);
    DoomUncommittedResources(resource_ids);
  }
}

void ServiceWorkerRegistry::NotifyDoneUninstallingRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerRegistration::Status new_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registration->SetStatus(new_status);
  uninstalling_registrations_.erase(registration->id());
}

void ServiceWorkerRegistry::UpdateToActiveState(int64_t registration_id,
                                                const blink::StorageKey& key,
                                                StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::UpdateToActiveState,
      base::BindOnce(&ServiceWorkerRegistry::DidUpdateToActiveState,
                     weak_factory_.GetWeakPtr(), key, std::move(callback)),
      static_cast<const int64_t>(registration_id), key);
}

void ServiceWorkerRegistry::UpdateLastUpdateCheckTime(
    int64_t registration_id,
    const blink::StorageKey& key,
    base::Time last_update_check_time,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::UpdateLastUpdateCheckTime,
      base::BindOnce(&ServiceWorkerRegistry::DidUpdateRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      static_cast<const int64_t>(registration_id), key,
      static_cast<const base::Time&>(last_update_check_time));
}

void ServiceWorkerRegistry::UpdateNavigationPreloadEnabled(
    int64_t registration_id,
    const blink::StorageKey& key,
    bool enable,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::
          UpdateNavigationPreloadEnabled,
      base::BindOnce(&ServiceWorkerRegistry::DidUpdateRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      static_cast<const int64_t>(registration_id), key,
      static_cast<const bool>(enable));
}

void ServiceWorkerRegistry::UpdateNavigationPreloadHeader(
    int64_t registration_id,
    const blink::StorageKey& key,
    const std::string& value,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::
          UpdateNavigationPreloadHeader,
      base::BindOnce(&ServiceWorkerRegistry::DidUpdateRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      static_cast<const int64_t>(registration_id), key, value);
}

void ServiceWorkerRegistry::UpdateFetchHandlerType(
    int64_t registration_id,
    const blink::StorageKey& key,
    blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::UpdateFetchHandlerType,
      base::BindOnce(&ServiceWorkerRegistry::DidUpdateRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      static_cast<const int64_t>(registration_id), key,
      static_cast<const blink::mojom::ServiceWorkerFetchHandlerType>(
          fetch_handler_type));
}

void ServiceWorkerRegistry::UpdateResourceSha256Checksums(
    int64_t registration_id,
    const blink::StorageKey& key,
    const base::flat_map<int64_t, std::string>& updated_sha256_checksums,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::
          UpdateResourceSha256Checksums,
      base::BindOnce(&ServiceWorkerRegistry::DidUpdateRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      static_cast<const int64_t>(registration_id), key,
      updated_sha256_checksums);
}

void ServiceWorkerRegistry::StoreUncommittedResourceId(
    int64_t resource_id,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::StoreUncommittedResourceId,
      base::BindOnce(&ServiceWorkerRegistry::DidWriteUncommittedResourceIds,
                     weak_factory_.GetWeakPtr(), key),
      static_cast<const int64_t>(resource_id));
}

void ServiceWorkerRegistry::DoomUncommittedResource(int64_t resource_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<int64_t> resource_ids = {resource_id};
  DoomUncommittedResources(resource_ids);
}

void ServiceWorkerRegistry::GetUserData(int64_t registration_id,
                                        const std::vector<std::string>& keys,
                                        GetUserDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::GetUserData,
      base::BindOnce(&ServiceWorkerRegistry::DidGetUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      static_cast<const int64_t>(registration_id), keys);
}

void ServiceWorkerRegistry::GetUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::GetUserDataByKeyPrefix,
      base::BindOnce(&ServiceWorkerRegistry::DidGetUserData,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      static_cast<const int64_t>(registration_id), key_prefix);
}

void ServiceWorkerRegistry::GetUserKeysAndDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::
          GetUserKeysAndDataByKeyPrefix,
      base::BindOnce(&ServiceWorkerRegistry::DidGetUserKeysAndData,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      static_cast<const int64_t>(registration_id), key_prefix);
}

void ServiceWorkerRegistry::StoreUserData(
    int64_t registration_id,
    const blink::StorageKey& key,
    const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      key_value_pairs.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }
  std::vector<storage::mojom::ServiceWorkerUserDataPtr> user_data;
  // TODO(crbug.com/40120038): Change this method to take a vector of
  // storage::mojom::ServiceWorkerUserDataPtr instead of converting
  //|key_value_pairs|.
  for (const auto& kv : key_value_pairs) {
    user_data.push_back(storage::mojom::ServiceWorkerUserData::New(
        registration_id, kv.first, kv.second));
  }

  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), blink::ServiceWorkerStatusCode::kErrorFailed);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::StoreUserData,
      base::BindOnce(&ServiceWorkerRegistry::DidStoreUserData,
                     weak_factory_.GetWeakPtr(), std::move(wrapped_callback),
                     key),
      registration_id, key, std::move(user_data));
}

void ServiceWorkerRegistry::ClearUserData(int64_t registration_id,
                                          const std::vector<std::string>& keys,
                                          StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), blink::ServiceWorkerStatusCode::kErrorFailed);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::ClearUserData,
      base::BindOnce(&ServiceWorkerRegistry::DidClearUserData,
                     weak_factory_.GetWeakPtr(), std::move(wrapped_callback)),
      static_cast<const int64_t>(registration_id), keys);
}

void ServiceWorkerRegistry::ClearUserDataByKeyPrefixes(
    int64_t registration_id,
    const std::vector<std::string>& key_prefixes,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), blink::ServiceWorkerStatusCode::kErrorFailed);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::ClearUserDataByKeyPrefixes,
      base::BindOnce(&ServiceWorkerRegistry::DidClearUserData,
                     weak_factory_.GetWeakPtr(), std::move(wrapped_callback)),
      static_cast<const int64_t>(registration_id), key_prefixes);
}

void ServiceWorkerRegistry::ClearUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), blink::ServiceWorkerStatusCode::kErrorFailed);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::
          ClearUserDataForAllRegistrationsByKeyPrefix,
      base::BindOnce(&ServiceWorkerRegistry::DidClearUserData,
                     weak_factory_.GetWeakPtr(), std::move(wrapped_callback)),
      key_prefix);
}

void ServiceWorkerRegistry::GetUserDataForAllRegistrations(
    const std::string& key,
    GetUserDataForAllRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::vector<std::pair<int64_t, std::string>>(),
      blink::ServiceWorkerStatusCode::kErrorFailed);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::
          GetUserDataForAllRegistrations,
      base::BindOnce(&ServiceWorkerRegistry::DidGetUserDataForAllRegistrations,
                     weak_factory_.GetWeakPtr(), std::move(wrapped_callback)),
      key);
}

void ServiceWorkerRegistry::GetUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::vector<std::pair<int64_t, std::string>>(),
      blink::ServiceWorkerStatusCode::kErrorFailed);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::
          GetUserDataForAllRegistrationsByKeyPrefix,
      base::BindOnce(&ServiceWorkerRegistry::DidGetUserDataForAllRegistrations,
                     weak_factory_.GetWeakPtr(), std::move(wrapped_callback)),
      key_prefix);
}

void ServiceWorkerRegistry::GetRegisteredStorageKeys(
    GetRegisteredStorageKeysCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::vector<blink::StorageKey>());
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::GetRegisteredStorageKeys,
      base::BindOnce(&ServiceWorkerRegistry::DidGetRegisteredStorageKeys,
                     weak_factory_.GetWeakPtr(), std::move(wrapped_callback)));
}

void ServiceWorkerRegistry::PerformStorageCleanup(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto wrapped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback));
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::PerformStorageCleanup,
      base::BindOnce(&ServiceWorkerRegistry::DidPerformStorageCleanup,
                     weak_factory_.GetWeakPtr(), std::move(wrapped_callback)));
}

void ServiceWorkerRegistry::PrepareForDeleteAndStartOver() {
  should_schedule_delete_and_start_over_ = false;
  is_storage_disabled_ = true;
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::Disable,
      base::BindOnce(&ServiceWorkerRegistry::DidDisable,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegistry::DeleteAndStartOver(StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::Delete,
      base::BindOnce(&ServiceWorkerRegistry::DidDeleteAndStartOver,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerRegistry::DisableStorageForTesting(
    base::OnceClosure callback) {
  GetRemoteStorageControl()->Disable(std::move(callback));
}

void ServiceWorkerRegistry::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!special_storage_policy_)
    return;
  storage_policy_observer_.emplace(
      base::BindRepeating(&ServiceWorkerRegistry::ApplyPolicyUpdates,
                          weak_factory_.GetWeakPtr()),
      GetIOThreadTaskRunner({}), special_storage_policy_);

  GetRegisteredStorageKeys(base::BindOnce(
      &ServiceWorkerRegistry::DidGetRegisteredStorageKeysOnStartup,
      weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegistry::FindRegistrationForIdInternal(
    int64_t registration_id,
    const std::optional<blink::StorageKey>& key,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Registration lookup is expected to abort when storage is disabled.
  if (is_storage_disabled_) {
    CompleteFindNow(nullptr, blink::ServiceWorkerStatusCode::kErrorAbort,
                    std::move(callback));
    return;
  }

  // Lookup live registration first.
  std::optional<scoped_refptr<ServiceWorkerRegistration>> registration =
      FindFromLiveRegistrationsForId(registration_id);
  if (registration) {
    blink::ServiceWorkerStatusCode status =
        registration.value() ? blink::ServiceWorkerStatusCode::kOk
                             : blink::ServiceWorkerStatusCode::kErrorNotFound;

    // Only notify access for already stored registrations.
    if (status == blink::ServiceWorkerStatusCode::kOk &&
        (*registration)->IsStored()) {
      // Can be nullptr in tests.
      if (quota_manager_proxy_) {
        // TODO(crbug.com/40213545): pass correct bucket.
        quota_manager_proxy_->NotifyBucketAccessed(
            storage::BucketLocator::ForDefaultBucket((*registration)->key()),
            base::Time::Now());
      }
    }

    CompleteFindNow(std::move(registration.value()), status,
                    std::move(callback));
    return;
  }

  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::FindRegistrationForId,
      base::BindOnce(&ServiceWorkerRegistry::DidFindRegistrationForId,
                     weak_factory_.GetWeakPtr(), registration_id,
                     std::move(callback)),
      static_cast<const int64_t>(registration_id), key);
}

ServiceWorkerRegistration*
ServiceWorkerRegistry::FindInstallingRegistrationForClientUrl(
    const GURL& client_url,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!client_url.has_ref());

  blink::ServiceWorkerLongestScopeMatcher matcher(client_url);
  ServiceWorkerRegistration* match = nullptr;

  // TODO(nhiroki): This searches over installing registrations linearly and it
  // couldn't be scalable. Maybe the regs should be partitioned by origin.
  for (const auto& registration : installing_registrations_)
    if (registration.second->key() == key &&
        matcher.MatchLongest(registration.second->scope()))
      match = registration.second.get();
  return match;
}

ServiceWorkerRegistration*
ServiceWorkerRegistry::FindInstallingRegistrationForScope(
    const GURL& scope,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& registration : installing_registrations_)
    if (registration.second->key() == key &&
        registration.second->scope() == scope)
      return registration.second.get();
  return nullptr;
}

ServiceWorkerRegistration*
ServiceWorkerRegistry::FindInstallingRegistrationForId(
    int64_t registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RegistrationRefsById::const_iterator found =
      installing_registrations_.find(registration_id);
  if (found == installing_registrations_.end())
    return nullptr;
  return found->second.get();
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerRegistry::GetOrCreateRegistration(
    const storage::mojom::ServiceWorkerRegistrationData& data,
    const ResourceList& resources,
    mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
        version_reference) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(data.registration_id);
  if (registration)
    return registration;

  blink::mojom::ServiceWorkerRegistrationOptions options(
      data.scope, data.script_type, data.update_via_cache);
  registration = ServiceWorkerRegistration::Create(
      options, data.key, data.registration_id, context_->AsWeakPtr(),
      data.ancestor_frame_type);
  registration->SetStored();
  registration->set_resources_total_size_bytes(data.resources_total_size_bytes);
  registration->set_last_update_check(data.last_update_check);
  DCHECK(!base::Contains(uninstalling_registrations_, data.registration_id));

  scoped_refptr<ServiceWorkerVersion> version =
      context_->GetLiveVersion(data.version_id);
  if (!version) {
    version = base::MakeRefCounted<ServiceWorkerVersion>(
        registration.get(), data.script, data.script_type, data.version_id,
        std::move(version_reference), context_->AsWeakPtr());
    version->set_fetch_handler_type(data.fetch_handler_type);
    // `has_hid_event_handlers_` in ServiceWorkerVersion should be set before
    // changing the status to ACTIVATED.
    version->set_has_hid_event_handlers(data.has_hid_event_handlers);
    // `has_usb_event_handlers_` in ServiceWorkerVersion should be set before
    // changing the status to ACTIVATED.
    version->set_has_usb_event_handlers(data.has_usb_event_handlers);
    // Set resources before changing the status to ACTIVATED/INSTALLED.
    // |sha256_script_checksum_| in ServiceWorkerVersion should be set before
    // changing the status.
    version->SetResources(resources);
    version->SetStatus(data.is_active ? ServiceWorkerVersion::ACTIVATED
                                      : ServiceWorkerVersion::INSTALLED);
    if (data.origin_trial_tokens)
      version->SetValidOriginTrialTokens(*data.origin_trial_tokens);

    std::set<blink::mojom::WebFeature> used_features(data.used_features.begin(),
                                                     data.used_features.end());
    version->set_used_features(std::move(used_features));
    // policy_container_host could be null for registration restored from old DB
    if (data.policy_container_policies) {
      version->set_policy_container_host(
          base::MakeRefCounted<PolicyContainerHost>(
              PolicyContainerPolicies(*data.policy_container_policies)));
    }
    if (data.router_rules && version->IsStaticRouterEnabled()) {
      auto error = version->SetupRouterEvaluator(*data.router_rules);
      DCHECK_EQ(error, ServiceWorkerRouterEvaluatorErrorEnums::kNoError)
          << "Failed to setup RouterEvaluator from the provided "
          << "rules. Possibly the database is corrupted.";
    }
  }
  version->set_script_response_time_for_devtools(data.script_response_time);

  if (version->status() == ServiceWorkerVersion::ACTIVATED)
    registration->SetActiveVersion(version);
  else if (version->status() == ServiceWorkerVersion::INSTALLED)
    registration->SetWaitingVersion(version);
  else
    NOTREACHED_IN_MIGRATION();

  registration->EnableNavigationPreload(data.navigation_preload_state->enabled);
  registration->SetNavigationPreloadHeader(
      data.navigation_preload_state->header);
  return registration;
}

std::optional<scoped_refptr<ServiceWorkerRegistration>>
ServiceWorkerRegistry::FindFromLiveRegistrationsForId(int64_t registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    // The registration is considered as findable when it's stored or in
    // installing state.
    if (registration->IsStored() ||
        base::Contains(installing_registrations_, registration_id)) {
      return registration;
    }
    // Otherwise, the registration should not be findable even if it's still
    // alive.
    return nullptr;
  }
  // There is no live registration. Storage lookup is required. Returning
  // nullopt results in storage lookup.
  return std::nullopt;
}

void ServiceWorkerRegistry::DoomUncommittedResources(
    const std::vector<int64_t>& resource_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::DoomUncommittedResources,
      base::BindOnce(&ServiceWorkerRegistry::DidDoomUncommittedResourceIds,
                     weak_factory_.GetWeakPtr()),
      resource_ids);
}

void ServiceWorkerRegistry::DidFindRegistrationForClientUrl(
    const GURL& client_url,
    const blink::StorageKey& key,
    int64_t trace_event_id,
    FindRegistrationCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    storage::mojom::ServiceWorkerFindRegistrationResultPtr result,
    const std::optional<std::vector<GURL>>& scopes) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerRegistry::DidFindRegistrationForClientUrl",
      TRACE_ID_WITH_SCOPE("ServiceWorkerRegistry", trace_event_id),
      TRACE_EVENT_FLAG_FLOW_IN);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (database_status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      database_status !=
          storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    DCHECK(!scopes);
    auto it = registration_scope_cache_.Peek(key);
    if (it != registration_scope_cache_.end()) {
      registration_scope_cache_.Erase(it);
    }
    ScheduleDeleteAndStartOver();
  } else if (scopes && !scopes->empty()) {
    registration_scope_cache_.Put(
        key, std::set<GURL>(scopes->begin(), scopes->end()));
  } else {
    auto it = registration_scope_cache_.Peek(key);
    if (it != registration_scope_cache_.end()) {
      registration_scope_cache_.Erase(it);
    }
  }

  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForClientUrl(client_url, key);
    if (installing_registration) {
      blink::ServiceWorkerStatusCode installing_status =
          installing_registration->is_deleted()
              ? blink::ServiceWorkerStatusCode::kErrorNotFound
              : blink::ServiceWorkerStatusCode::kOk;
      FindRegistrationForClientUrlTraceEventEnd(
          trace_event_id, status,
          (installing_status == blink::ServiceWorkerStatusCode::kOk)
              ? "Installing registration is found"
              : "Any registrations are not found");
      RunFindRegistrationCallbacks(client_url, key,
                                   std::move(installing_registration),
                                   installing_status);
      return;
    }
  }

  scoped_refptr<ServiceWorkerRegistration> registration;
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(result);
    DCHECK(result->registration);
    DCHECK(result->version_reference);
    registration =
        GetOrCreateRegistration(*(result->registration), result->resources,
                                std::move(result->version_reference));
    registration_id_cache_.Put(std::make_pair(result->registration->scope, key),
                               result->registration->registration_id);

    if (quota_manager_proxy_) {
      // Can be nullptr in tests.
      quota_manager_proxy_->NotifyBucketAccessed(
          storage::BucketLocator::ForDefaultBucket(registration->key()),
          base::Time::Now());
    }
  }

  FindRegistrationForClientUrlTraceEventEnd(trace_event_id, status,
                                            std::nullopt);
  RunFindRegistrationCallbacks(client_url, key, std::move(registration),
                               status);
}

void ServiceWorkerRegistry::RunFindRegistrationCallbacks(
    const GURL& client_url,
    const blink::StorageKey& key,
    scoped_refptr<ServiceWorkerRegistration> registration,
    blink::ServiceWorkerStatusCode status) {
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerRegistry::RunFindRegistrationCallbacks",
               "client_url", client_url, "status",
               blink::ServiceWorkerStatusToString(status));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter =
      find_registration_callbacks_.find(std::make_pair(client_url, key));
  CHECK(iter != find_registration_callbacks_.end(), base::NotFatalUntil::M130);
  std::vector<FindRegistrationCallback> callbacks = std::move(iter->second);
  find_registration_callbacks_.erase(iter);
  for (FindRegistrationCallback& callback : callbacks) {
    CompleteFindNow(registration, status, std::move(callback));
  }
}

void ServiceWorkerRegistry::DidFindRegistrationForScope(
    FindRegistrationCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    storage::mojom::ServiceWorkerFindRegistrationResultPtr result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (database_status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      database_status !=
          storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }

  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  scoped_refptr<ServiceWorkerRegistration> registration;
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(result);
    DCHECK(result->registration);
    DCHECK(result->version_reference);
    registration =
        GetOrCreateRegistration(*(result->registration), result->resources,
                                std::move(result->version_reference));

    if (quota_manager_proxy_) {
      // Can be nullptr in tests.
      quota_manager_proxy_->NotifyBucketAccessed(
          storage::BucketLocator::ForDefaultBucket(registration->key()),
          base::Time::Now());
    }
  }

  CompleteFindNow(std::move(registration), status, std::move(callback));
}

void ServiceWorkerRegistry::DidFindRegistrationForId(
    int64_t registration_id,
    FindRegistrationCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    storage::mojom::ServiceWorkerFindRegistrationResultPtr result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (database_status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      database_status !=
          storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }

  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForId(registration_id);
    if (installing_registration) {
      CompleteFindNow(std::move(installing_registration),
                      blink::ServiceWorkerStatusCode::kOk, std::move(callback));
      return;
    }
  }

  scoped_refptr<ServiceWorkerRegistration> registration;
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(result);
    DCHECK(result->registration);
    DCHECK(result->version_reference);
    registration =
        GetOrCreateRegistration(*(result->registration), result->resources,
                                std::move(result->version_reference));
    if (quota_manager_proxy_) {
      // Can be nullptr in tests.
      quota_manager_proxy_->NotifyBucketAccessed(
          storage::BucketLocator::ForDefaultBucket(registration->key()),
          base::Time::Now());
    }
  }

  CompleteFindNow(std::move(registration), status, std::move(callback));
}

void ServiceWorkerRegistry::DidGetRegistrationsForStorageKey(
    GetRegistrationsCallback callback,
    const blink::StorageKey& key_filter,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    std::vector<storage::mojom::ServiceWorkerFindRegistrationResultPtr>
        entries) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status != blink::ServiceWorkerStatusCode::kOk &&
      status != blink::ServiceWorkerStatusCode::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(
        status, std::vector<scoped_refptr<ServiceWorkerRegistration>>());
    return;
  }

  // Add all stored registrations.
  std::set<int64_t> registration_ids;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  for (const auto& entry : entries) {
    DCHECK(entry->registration);
    DCHECK(entry->version_reference);
    registration_ids.insert(entry->registration->registration_id);
    registrations.push_back(
        GetOrCreateRegistration(*entry->registration, entry->resources,
                                std::move(entry->version_reference)));
  }

  // Add unstored registrations that are being installed.
  for (const auto& registration : installing_registrations_) {
    if (registration.second->key() != key_filter)
      continue;
    if (registration_ids.insert(registration.first).second)
      registrations.push_back(registration.second);
  }

  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk,
                          std::move(registrations));
}

void ServiceWorkerRegistry::DidGetAllRegistrations(
    GetRegistrationsInfosCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    RegistrationList registration_data_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status != blink::ServiceWorkerStatusCode::kOk &&
      status != blink::ServiceWorkerStatusCode::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(status,
                            std::vector<ServiceWorkerRegistrationInfo>());
    return;
  }

  // Add all stored registrations.
  std::set<int64_t> pushed_registrations;
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (const auto& registration_data : registration_data_list) {
    const bool inserted =
        pushed_registrations.insert(registration_data->registration_id).second;
    DCHECK(inserted);

    scoped_refptr<ServiceWorkerRegistration> registration =
        context_->GetLiveRegistration(registration_data->registration_id);
    if (registration) {
      infos.push_back(registration->GetInfo());
      continue;
    }

    ServiceWorkerRegistrationInfo info;
    info.scope = registration_data->scope;
    info.key = registration_data->key;
    info.update_via_cache = registration_data->update_via_cache;
    info.registration_id = registration_data->registration_id;
    info.stored_version_size_bytes =
        registration_data->resources_total_size_bytes;
    info.navigation_preload_enabled =
        registration_data->navigation_preload_state->enabled;
    info.navigation_preload_header_length =
        registration_data->navigation_preload_state->header.size();
    if (ServiceWorkerVersion* version =
            context_->GetLiveVersion(registration_data->version_id)) {
      if (registration_data->is_active)
        info.active_version = version->GetInfo();
      else
        info.waiting_version = version->GetInfo();
      infos.push_back(info);
      continue;
    }

    if (registration_data->is_active) {
      info.active_version.status = ServiceWorkerVersion::ACTIVATED;
      info.active_version.script_url = registration_data->script;
      info.active_version.version_id = registration_data->version_id;
      info.active_version.registration_id = registration_data->registration_id;
      info.active_version.script_response_time =
          registration_data->script_response_time;
      info.active_version.fetch_handler_type =
          registration_data->fetch_handler_type;
      info.active_version.navigation_preload_state.enabled =
          registration_data->navigation_preload_state->enabled;
      info.active_version.navigation_preload_state.header =
          registration_data->navigation_preload_state->header;
      if (registration_data->router_rules) {
        info.active_version.router_rules =
            RouterRulesToString(*registration_data->router_rules);
      }
    } else {
      info.waiting_version.status = ServiceWorkerVersion::INSTALLED;
      info.waiting_version.script_url = registration_data->script;
      info.waiting_version.version_id = registration_data->version_id;
      info.waiting_version.registration_id = registration_data->registration_id;
      info.waiting_version.script_response_time =
          registration_data->script_response_time;
      info.waiting_version.fetch_handler_type =
          registration_data->fetch_handler_type;
      info.waiting_version.navigation_preload_state.enabled =
          registration_data->navigation_preload_state->enabled;
      info.waiting_version.navigation_preload_state.header =
          registration_data->navigation_preload_state->header;
      if (registration_data->router_rules) {
        info.waiting_version.router_rules =
            RouterRulesToString(*registration_data->router_rules);
      }
    }
    infos.push_back(info);
  }

  // Add unstored registrations that are being installed.
  for (const auto& registration : installing_registrations_) {
    if (pushed_registrations.insert(registration.first).second)
      infos.push_back(registration.second->GetInfo());
  }

  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk, infos);
}

void ServiceWorkerRegistry::DidGetStorageUsageForStorageKey(
    GetStorageUsageForStorageKeyCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    int64_t usage) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);
  std::move(callback).Run(status, usage);
}

void ServiceWorkerRegistry::DidStoreRegistration(
    int64_t stored_registration_id,
    uint64_t stored_resources_total_size_bytes,
    const GURL& stored_scope,
    const blink::StorageKey& key,
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    uint64_t deleted_resources_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  CheckForClientWriteFailure(quota_manager_proxy_, database_status, key);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(status);
    return;
  }

  auto registration_stored_callback =
      base::BindOnce(&ServiceWorkerRegistry::NotifyRegistrationStored,
                     weak_factory_.GetWeakPtr(), stored_registration_id,
                     stored_resources_total_size_bytes, stored_scope, key,
                     std::move(callback));

  if (quota_manager_proxy_) {
    // Can be nullptr in tests.
    quota_manager_proxy_->NotifyBucketModified(
        storage::QuotaClientType::kServiceWorker,
        storage::BucketLocator::ForDefaultBucket(key),
        stored_resources_total_size_bytes - deleted_resources_size,
        base::Time::Now(), base::SequencedTaskRunner::GetCurrentDefault(),
        std::move(registration_stored_callback));
  } else {
    std::move(registration_stored_callback).Run();
  }
}

void ServiceWorkerRegistry::NotifyRegistrationStored(
    int64_t stored_registration_id,
    uint64_t stored_resources_total_size_bytes,
    const GURL& stored_scope,
    const blink::StorageKey& key,
    StatusCallback callback) {
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(stored_registration_id);
  if (registration) {
    registration->SetStored();
    registration->set_resources_total_size_bytes(
        stored_resources_total_size_bytes);
  }
  context_->NotifyRegistrationStored(stored_registration_id, stored_scope, key);

  auto iter = registration_scope_cache_.Get(key);
  if (iter != registration_scope_cache_.end()) {
    std::set<GURL>& scopes = iter->second;
    scopes.insert(stored_scope);
    if (scopes.size() > storage::kMaxServiceWorkerScopeUrlCountPerStorageKey) {
      registration_scope_cache_.Erase(iter);
    }
  }

  if (storage_policy_observer_) {
    storage_policy_observer_->StartTrackingOrigin(key.origin());
  }

  // For all other blink::ServiceWorkerStatusCode entries,
  // DidStoreRegistration() schedules deletion and starts over, so the only
  // status code that can be called here is the kOk one.
  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
}

void ServiceWorkerRegistry::DidDeleteRegistration(
    int64_t registration_id,
    const GURL& stored_scope,
    const blink::StorageKey& key,
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus database_status,
    uint64_t deleted_resources_size,
    storage::mojom::ServiceWorkerStorageStorageKeyState storage_key_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::ServiceWorkerStatusCode status =
      DatabaseStatusToStatusCode(database_status);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    ScheduleDeleteAndStartOver();
    std::move(callback).Run(status);
    return;
  }

  auto notify_registration_deleted_callback = base::BindOnce(
      &ServiceWorkerRegistry::NotifyRegistrationDeletedForStorageKey,
      weak_factory_.GetWeakPtr(), registration_id, stored_scope, key,
      storage_key_state, std::move(callback));

  if (quota_manager_proxy_) {
    // Can be nullptr in tests.
    quota_manager_proxy_->NotifyBucketModified(
        storage::QuotaClientType::kServiceWorker,
        storage::BucketLocator::ForDefaultBucket(key), -deleted_resources_size,
        base::Time::Now(), base::SequencedTaskRunner::GetCurrentDefault(),
        std::move(notify_registration_deleted_callback));
  } else {
    std::move(notify_registration_deleted_callback).Run();
  }
}

void ServiceWorkerRegistry::NotifyRegistrationDeletedForStorageKey(
    int64_t registration_id,
    const GURL& stored_scope,
    const blink::StorageKey& key,
    storage::mojom::ServiceWorkerStorageStorageKeyState storage_key_state,
    StatusCallback callback) {
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration)
    registration->UnsetStored();

  if (storage_key_state ==
      storage::mojom::ServiceWorkerStorageStorageKeyState::kDelete) {
    context_->NotifyAllRegistrationsDeletedForStorageKey(key);
    if (storage_policy_observer_)
      storage_policy_observer_->StopTrackingOrigin(key.origin());
    auto it = registration_scope_cache_.Peek(key);
    if (it != registration_scope_cache_.end()) {
      registration_scope_cache_.Erase(it);
    }
  } else {
    auto iter = registration_scope_cache_.Peek(key);
    if (iter != registration_scope_cache_.end()) {
      iter->second.erase(stored_scope);
      DCHECK(!iter->second.empty());
    }
  }

  // For all other blink::ServiceWorkerStatusCode entries,
  // DidDeleteRegistration() schedules deletion and starts over, so the only
  // status code that can be called here is the kOk one.
  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
}

void ServiceWorkerRegistry::DidUpdateRegistration(
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      status != storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidUpdateToActiveState(
    const blink::StorageKey& key,
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  CheckForClientWriteFailure(quota_manager_proxy_, status, key);
  DidUpdateRegistration(std::move(callback), status);
}

void ServiceWorkerRegistry::DidWriteUncommittedResourceIds(
    const blink::StorageKey& key,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CheckForClientWriteFailure(quota_manager_proxy_, status, key);
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk)
    ScheduleDeleteAndStartOver();
}

void ServiceWorkerRegistry::DidDoomUncommittedResourceIds(
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk)
    ScheduleDeleteAndStartOver();
}

void ServiceWorkerRegistry::DidGetUserData(
    GetUserDataCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    const std::vector<std::string>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      status != storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(data, DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidGetUserKeysAndData(
    GetUserKeysAndDataCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    const base::flat_map<std::string, std::string>& data_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      status != storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(DatabaseStatusToStatusCode(status), data_map);
}

void ServiceWorkerRegistry::DidStoreUserData(
    StatusCallback callback,
    const blink::StorageKey& key,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CheckForClientWriteFailure(quota_manager_proxy_, status, key);
  // |status| can be NOT_FOUND when the associated registration did not exist in
  // the database. In the case, we don't have to schedule the corruption
  // recovery.
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk &&
      status != storage::mojom::ServiceWorkerDatabaseStatus::kErrorNotFound) {
    ScheduleDeleteAndStartOver();
  }
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidClearUserData(
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk)
    ScheduleDeleteAndStartOver();
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidGetUserDataForAllRegistrations(
    GetUserDataForAllRegistrationsCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    std::vector<storage::mojom::ServiceWorkerUserDataPtr> entries) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(crbug.com/40120038): Update call sites of
  // GetUserDataForAllRegistrations so that we can avoid converting mojo struct
  // to a pair.
  std::vector<std::pair<int64_t, std::string>> user_data;
  if (status != storage::mojom::ServiceWorkerDatabaseStatus::kOk)
    ScheduleDeleteAndStartOver();
  for (auto& entry : entries) {
    user_data.emplace_back(entry->registration_id, entry->value);
  }
  std::move(callback).Run(user_data, DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidGetNewRegistrationId(
    blink::mojom::ServiceWorkerRegistrationOptions options,
    const blink::StorageKey& key,
    blink::mojom::AncestorFrameType ancestor_frame_type,
    NewRegistrationCallback callback,
    int64_t registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId) {
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(ServiceWorkerRegistration::Create(
      std::move(options), key, registration_id, context_->AsWeakPtr(),
      ancestor_frame_type));
}

void ServiceWorkerRegistry::DidGetNewVersionId(
    scoped_refptr<ServiceWorkerRegistration> registration,
    const GURL& script_url,
    blink::mojom::ScriptType script_type,
    NewVersionCallback callback,
    int64_t version_id,
    mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
        version_reference) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (version_id == blink::mojom::kInvalidServiceWorkerVersionId) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto version = base::MakeRefCounted<ServiceWorkerVersion>(
      registration.get(), script_url, script_type, version_id,
      std::move(version_reference), context_->AsWeakPtr());
  std::move(callback).Run(std::move(version));
}

void ServiceWorkerRegistry::ScheduleDeleteAndStartOver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!should_schedule_delete_and_start_over_) {
    // Recovery process has already been scheduled.
    return;
  }

  // Ideally, the corruption recovery should not be scheduled if the error
  // is transient as it can get healed soon (e.g. IO error). However we
  // unconditionally start recovery here for simplicity and low error rates.
  DVLOG(1) << "Schedule to delete the context and start over.";
  context_->ScheduleDeleteAndStartOver();
  // ServiceWorkerContextCore should call PrepareForDeleteAndStartOver().
  DCHECK(!should_schedule_delete_and_start_over_);
  DCHECK(is_storage_disabled_);
}

void ServiceWorkerRegistry::DidDeleteAndStartOver(
    StatusCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  remote_storage_control_.reset();
  std::move(callback).Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerRegistry::DidGetRegisteredStorageKeys(
    GetRegisteredStorageKeysCallback callback,
    const std::vector<blink::StorageKey>& keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(keys);
}

void ServiceWorkerRegistry::DidPerformStorageCleanup(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run();
}

void ServiceWorkerRegistry::DidDisable() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerRegistry::DidApplyPolicyUpdates(
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerRegistry::DidGetRegisteredStorageKeysOnStartup(
    const std::vector<blink::StorageKey>& storage_keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(special_storage_policy_);
  std::vector<url::Origin> origins;
  origins.reserve(storage_keys.size());
  for (const blink::StorageKey& storage_key : storage_keys) {
    origins.push_back(storage_key.origin());
  }
  storage_policy_observer_->StartTrackingOrigins(origins);
}

void ServiceWorkerRegistry::ApplyPolicyUpdates(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_storage_disabled_)
    return;
  if (policy_updates.empty())
    return;

  CreateInvokerAndStartRemoteCall(
      &storage::mojom::ServiceWorkerStorageControl::ApplyPolicyUpdates,
      base::BindOnce(&ServiceWorkerRegistry::DidApplyPolicyUpdates,
                     weak_factory_.GetWeakPtr()),
      std::move(policy_updates));
}

bool ServiceWorkerRegistry::ShouldPurgeOnShutdownForTesting(
    const blink::StorageKey& key) {
  if (!storage_policy_observer_)
    return false;
  return storage_policy_observer_->ShouldPurgeOnShutdownForTesting(  // IN-TEST
      key.origin());
}

mojo::Remote<storage::mojom::ServiceWorkerStorageControl>&
ServiceWorkerRegistry::GetRemoteStorageControl() {
  // TODO(crbug.com/40813186): Replace CHECK with DCHECK_CURRENTLY_ON
  // once the cause is identified.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!(remote_storage_control_.is_bound() &&
           !remote_storage_control_.is_connected()))
      << "Rebinding is not supported yet.";

  if (!remote_storage_control_.is_bound()) {
    context_->wrapper()->BindStorageControl(
        remote_storage_control_.BindNewPipeAndPassReceiver(
            base::FeatureList::IsEnabled(
                kServiceWorkerStorageControlResponseQueue)
                ? GetUIThreadTaskRunner(
                      {BrowserTaskType::kServiceWorkerStorageControlResponse})
                : base::SequencedTaskRunner::GetCurrentDefault()));
    DCHECK(remote_storage_control_.is_bound());
    remote_storage_control_.set_disconnect_handler(
        base::BindOnce(&ServiceWorkerRegistry::OnRemoteStorageDisconnected,
                       weak_factory_.GetWeakPtr()));
  }

  return remote_storage_control_;
}

void ServiceWorkerRegistry::OnRemoteStorageDisconnected() {
  const size_t kMaxRetryCounts = 100;

  // TODO(crbug.com/40813186): Replace CHECK with DCHECK_CURRENTLY_ON
  // once the cause is identified.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  remote_storage_control_.reset();

  if (!context_)
    return;

  if (is_storage_disabled_) {
    // When the storage is disabled a storage error recovery process is ongoing
    // and the storage control will be destroyed soon. Don't try to reconnect
    // storage control remote but flush inflight calls. These calls will check
    // `is_storage_disabled_` and return errors.
    DidRecover();
    return;
  }

  if (connection_state_ == ConnectionState::kRecovering) {
    ++recovery_retry_counts_;
    if (recovery_retry_counts_ > kMaxRetryCounts) {
      RecordRetryCount(kMaxRetryCounts, inflight_calls_.size());
      CHECK(false) << "The Storage Service consistently crashes.";
      return;
    }
  }
  connection_state_ = ConnectionState::kRecovering;

  // Collect live version information to recover resource purging state in the
  // Storage Service.
  std::vector<storage::mojom::ServiceWorkerLiveVersionInfoPtr> versions;
  for (auto& it : context_->GetLiveVersions()) {
    if (!it.second->is_redundant())
      versions.push_back(it.second->RebindStorageReference());
  }

  GetRemoteStorageControl()->Recover(
      std::move(versions), base::BindOnce(&ServiceWorkerRegistry::DidRecover,
                                          weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegistry::DidRecover() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RecordRetryCount(recovery_retry_counts_, inflight_calls_.size());

  recovery_retry_counts_ = 0;
  connection_state_ = ConnectionState::kNormal;

  // Retry inflight calls.
  for (auto& call : inflight_calls_)
    call->Run();
}

void ServiceWorkerRegistry::StartRemoteCall(
    std::unique_ptr<InflightCall> call) {
  auto* raw_call = call.get();
  inflight_calls_.insert(std::move(call));
  if (connection_state_ == ConnectionState::kNormal) {
    raw_call->Run();
  }
}

void ServiceWorkerRegistry::FinishRemoteCall(const InflightCall* call) {
  DCHECK(base::Contains(inflight_calls_, call));
  inflight_calls_.erase(call);
}

namespace {

template <typename T>
using PassingType = std::conditional_t<std::is_scalar<T>::value, T, const T&>;

template <typename T>
struct RequiresCloneTraits {
  static constexpr bool kValue = std::is_copy_constructible<T>::value;
};

// Specialization for vectors with move-only types, since STL does not SFINAE
// disable the copy constructor.
template <typename T>
struct RequiresCloneTraits<std::vector<T>> {
  static constexpr bool kValue = std::is_copy_constructible<T>::value;
};

template <typename T,
          bool is_copy_constructible = RequiresCloneTraits<T>::kValue>
struct CloneTraits;

template <typename T>
struct CloneTraits<T, true> {
  // Simply return a const ref for a copyable type, as the caller can copy if
  // needed.
  static const T& CloneIfNeeded(const T& in) { return in; }
};

template <typename T>
struct CloneTraits<T, false> {
  static T CloneIfNeeded(const T& in) { return mojo::Clone(in); }
};

}  // namespace

template <typename Functor, typename... Args, typename... ReplyArgs>
void ServiceWorkerRegistry::CreateInvokerAndStartRemoteCall(
    Functor&& f,
    base::OnceCallback<void(ReplyArgs...)> reply_callback,
    Args&&... args) {
  using InflightCallWithInvokerType = InflightCallWithInvoker<ReplyArgs...>;

  // This repeating callback (re)issues the Mojo IPC designated by `f`. Note
  // that the original passed in `args` end up being owned by `invoker`, and
  // ownership is never transferred to the called lambda.
  auto invoker = base::BindRepeating(
      [](Functor f, PassingType<std::decay_t<Args>>... args,
         InflightCallWithInvokerType* inflight_call,
         base::OnceCallback<void(ReplyArgs...)> reply_callback) {
        DCHECK(inflight_call->registry()
                   ->GetRemoteStorageControl()
                   .is_connected());
        ((*inflight_call->registry()->GetRemoteStorageControl()).*f)(
            CloneTraits<std::decay_t<Args>>::CloneIfNeeded(args)...,
            std::move(reply_callback));
      },
      std::forward<Functor>(f), std::forward<Args>(args)...);

  auto inflight_call = std::make_unique<InflightCallWithInvokerType>(
      this, std::move(invoker), std::move(reply_callback));

  StartRemoteCall(std::move(inflight_call));
}

}  // namespace content
