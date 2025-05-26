// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_client.h"

#include <set>
#include <variant>

#include "base/check_is_test.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_util.h"
#include "base/uuid.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_running_status_callback.mojom.h"

namespace content {

namespace {

void RunCallbacks(
    std::vector<ServiceWorkerClient::ExecutionReadyCallback> callbacks) {
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

}  // namespace

// RAII helper class for keeping track of versions waiting for an update hint
// from the renderer.
//
// This class is move-only.
class ServiceWorkerClient::PendingUpdateVersion {
 public:
  explicit PendingUpdateVersion(scoped_refptr<ServiceWorkerVersion> version)
      : version_(std::move(version)) {
    version_->IncrementPendingUpdateHintCount();
  }

  PendingUpdateVersion(PendingUpdateVersion&& other) {
    version_ = std::move(other.version_);
  }

  PendingUpdateVersion(const PendingUpdateVersion&) = delete;
  PendingUpdateVersion& operator=(const PendingUpdateVersion&) = delete;

  ~PendingUpdateVersion() {
    if (version_) {
      version_->DecrementPendingUpdateHintCount();
    }
  }

  PendingUpdateVersion& operator=(PendingUpdateVersion&& other) {
    version_ = std::move(other.version_);
    return *this;
  }

  // Needed for base::flat_set.
  bool operator<(const PendingUpdateVersion& other) const {
    return version_ < other.version_;
  }

 private:
  scoped_refptr<ServiceWorkerVersion> version_;
};

// To realize the running status condition in the ServiceWorker static routing
// API, the latest ServiceWorker running status is notified to all renderers
// that execute ServiceWorker subresource loaders and controlled by the
// ServiceWorker.
//
// This is an observer to receive the ServiceWorker running status change,
// and make the update notified to all the renderers via mojo IPC.
//
// See:
// https://w3c.github.io/ServiceWorker/#dom-routercondition-runningstatus
class ServiceWorkerClient::ServiceWorkerRunningStatusObserver final
    : public ServiceWorkerVersion::Observer {
 public:
  void OnRunningStateChanged(ServiceWorkerVersion* version) override {
    Notify(version->running_status());
  }
  void Notify(blink::EmbeddedWorkerStatus status) {
    for (const auto& callback : callbacks_) {
      callback->OnStatusChanged(status);
    }
  }
  void AddCallback(
      mojo::PendingRemote<blink::mojom::ServiceWorkerRunningStatusCallback>
          callback) {
    callbacks_.Add(std::move(callback));
  }

 private:
  mojo::RemoteSet<blink::mojom::ServiceWorkerRunningStatusCallback> callbacks_;
};

ServiceWorkerClient::ServiceWorkerClient(
    base::WeakPtr<ServiceWorkerContextCore> context,
    bool is_parent_frame_secure,
    FrameTreeNodeId ongoing_navigation_frame_tree_node_id)
    : context_(std::move(context)),
      owner_(context_->service_worker_client_owner()),
      create_time_(base::TimeTicks::Now()),
      client_uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      is_parent_frame_secure_(is_parent_frame_secure),
      is_initiated_by_prefetch_(false),
      client_info_(ServiceWorkerClientInfo()),
      process_id_for_worker_client_(ChildProcessHost::kInvalidUniqueID),
      ongoing_navigation_frame_tree_node_id_(
          ongoing_navigation_frame_tree_node_id) {
  DCHECK(context_);
}

ServiceWorkerClient::ServiceWorkerClient(
    base::WeakPtr<ServiceWorkerContextCore> context,
    bool is_parent_frame_secure,
    scoped_refptr<network::SharedURLLoaderFactory>
        network_url_loader_factory_for_prefetch)
    : context_(std::move(context)),
      owner_(context_->service_worker_client_owner()),
      create_time_(base::TimeTicks::Now()),
      client_uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      is_parent_frame_secure_(is_parent_frame_secure),
      is_initiated_by_prefetch_(true),
      client_info_(ServiceWorkerClientInfo()),
      process_id_for_worker_client_(ChildProcessHost::kInvalidUniqueID),
      network_url_loader_factory_for_prefetch_(
          std::move(network_url_loader_factory_for_prefetch)) {
  DCHECK(context_);
}

ServiceWorkerClient::ServiceWorkerClient(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int process_id,
    ServiceWorkerClientInfo client_info)
    : context_(std::move(context)),
      owner_(context_->service_worker_client_owner()),
      create_time_(base::TimeTicks::Now()),
      client_uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      is_parent_frame_secure_(true),
      is_initiated_by_prefetch_(false),
      client_info_(client_info),
      process_id_for_worker_client_(process_id) {
  DCHECK(context_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(process_id_for_worker_client_, ChildProcessHost::kInvalidUniqueID);
}

ServiceWorkerClient::~ServiceWorkerClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsContainerForWindowClient()) {
    auto* rfh = RenderFrameHostImpl::FromID(GetRenderFrameHostId());
    if (rfh) {
      rfh->RemoveServiceWorkerClient(client_uuid());
    }
  }

  if (controller_) {
    controller_->Uncontrol(client_uuid());

    if (running_status_observer_) {
      controller_->RemoveObserver(running_status_observer_.get());
      running_status_observer_.reset();
    }
  }

  // Remove |this| as an observer of ServiceWorkerRegistrations.
  // TODO(falken): Use base::ScopedObservation instead of this explicit call.
  controller_.reset();
  controller_registration_.reset();

  // Ensure callbacks awaiting execution ready are notified.
  RunExecutionReadyCallbacks();

  RemoveAllMatchingRegistrations();
}

void ServiceWorkerClient::HintToUpdateServiceWorker() {
  // The destructors notify the ServiceWorkerVersions to update.
  versions_to_update_.clear();
}

void ServiceWorkerClient::EnsureFileAccess(
    const std::vector<base::FilePath>& file_paths,
    blink::mojom::ServiceWorkerContainerHost::EnsureFileAccessCallback
        callback) {
  ServiceWorkerVersion* version =
      controller_registration_ ? controller_registration_->active_version()
                               : nullptr;

  // The controller might have legitimately been lost due to
  // NotifyControllerLost(), so don't ReportBadMessage() here.
  if (version) {
    int controller_process_id = version->embedded_worker()->process_id();

    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    for (const auto& file : file_paths) {
      if (!policy->CanReadFile(GetProcessId(), file)) {
        mojo::ReportBadMessage(
            "The renderer doesn't have access to the file "
            "but it tried to grant access to the controller.");
        return;
      }

      if (!policy->CanReadFile(controller_process_id, file)) {
        policy->GrantReadFile(controller_process_id, file);
      }
    }
  }

  std::move(callback).Run();
}

void ServiceWorkerClient::OnExecutionReady() {
  // Since `OnExecutionReady()` is a part of `ServiceWorkerContainerHost`,
  // this method is called only if `is_container_ready()` is true.
  CHECK(is_container_ready());

  if (is_execution_ready()) {
    mojo::ReportBadMessage("SWPH_OER_ALREADY_READY");
    return;
  }

  // The controller was sent on navigation commit but we must send it again here
  // because 1) the controller might have changed since navigation commit due to
  // skipWaiting(), and 2) the UseCounter might have changed since navigation
  // commit, in such cases the updated information was prevented being sent due
  // to false is_execution_ready().
  // TODO(leonhsl): Create some layout tests covering the above case 1), in
  // which case we may also need to set |notify_controllerchange| correctly.
  container_host()->SendSetController(false /* notify_controllerchange */);

  SetExecutionReady();
}

void ServiceWorkerClient::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (container_host()) {
    container_host()->OnVersionAttributesChanged(registration,
                                                 std::move(changed_mask));
  }
}

void ServiceWorkerClient::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerClient::OnRegistrationFinishedUninstalling(
    ServiceWorkerRegistration* registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerClient::OnSkippedWaiting(
    ServiceWorkerRegistration* registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (controller_registration_ != registration) {
    return;
  }

#if DCHECK_IS_ON()
  DCHECK(controller_);
  ServiceWorkerVersion* active = controller_registration_->active_version();
  DCHECK(active);
  DCHECK_NE(active, controller_.get());
  DCHECK_EQ(active->status(), ServiceWorkerVersion::ACTIVATING);
#endif  // DCHECK_IS_ON()

  if (IsBackForwardCacheEnabled() && IsInBackForwardCache()) {
    // This ServiceWorkerContainerHost is evicted from BackForwardCache in
    // |ActivateWaitingVersion|, but not deleted yet. This can happen because
    // asynchronous eviction and |OnSkippedWaiting| are in the same task.
    // The controller does not have to be updated because |this| will be evicted
    // from BackForwardCache.
    // TODO(yuzus): Wire registration with ServiceWorkerContainerHost so that we
    // can check on the caller side.
    return;
  }

  UpdateController(true /* notify_controllerchange */);
}

void ServiceWorkerClient::AddMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(blink::ServiceWorkerScopeMatches(registration->scope(),
                                          GetUrlForScopeMatch()));
  DCHECK(registration->key() == key());
  if (!IsEligibleForServiceWorkerController()) {
    return;
  }
  size_t key = registration->scope().spec().size();
  if (base::Contains(matching_registrations_, key)) {
    return;
  }
  registration->AddListener(this);
  matching_registrations_[key] = registration;

  if (container_host()) {
    container_host()->ReturnRegistrationForReadyIfNeeded();
  }
}

void ServiceWorkerClient::RemoveMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(controller_registration_, registration);
#if DCHECK_IS_ON()
  DCHECK(IsMatchingRegistration(registration));
#endif  // DCHECK_IS_ON()

  registration->RemoveListener(this);
  size_t key = registration->scope().spec().size();
  matching_registrations_.erase(key);
}

ServiceWorkerRegistration* ServiceWorkerClient::MatchRegistration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& registration : base::Reversed(matching_registrations_)) {
    if (registration.second->is_uninstalled()) {
      continue;
    }
    if (registration.second->is_uninstalling()) {
      return nullptr;
    }
    return registration.second.get();
  }
  return nullptr;
}

void ServiceWorkerClient::AddServiceWorkerToUpdate(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This is only called for windows now, but it should be called for all
  // clients someday.
  DCHECK(IsContainerForWindowClient());

  versions_to_update_.emplace(std::move(version));
}

void ServiceWorkerClient::CountFeature(blink::mojom::WebFeature feature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `container_` can be used only if ServiceWorkerContainerInfoForClient has
  // been passed to the renderer process. Otherwise, the method call will crash
  // inside the mojo library (See crbug.com/40918057).
  if (!is_container_ready()) {
    buffered_used_features_.insert(feature);
    return;
  }

  // And only when loading finished so the controller is really settled.
  if (!is_execution_ready()) {
    buffered_used_features_.insert(feature);
    return;
  }

  container_host()->CountFeature(feature);
}

void ServiceWorkerClient::NotifyControllerLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsBackForwardCacheEnabled() && IsInBackForwardCache()) {
    // The controller was unregistered, which usually does not happen while it
    // has controllees. Since the document is in the back/forward cache, it does
    // not count as a controllee. However, this means if it were to be restored,
    // the page would be in an unexpected state, so evict the bfcache entry.
    EvictFromBackForwardCache(BackForwardCacheMetrics::NotRestoredReason::
                                  kServiceWorkerUnregistration);
  }

  SetControllerRegistration(nullptr, true /* notify_controllerchange */);
}

void ServiceWorkerClient::ClaimedByRegistration(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(registration->active_version());
  DCHECK(is_execution_ready());

  // TODO(falken): This should just early return, or DCHECK. claim() should have
  // no effect on a page that's already using the registration.
  if (registration == controller_registration_) {
    UpdateController(true /* notify_controllerchange */);
    return;
  }

  SetControllerRegistration(registration, true /* notify_controllerchange */);
}

blink::mojom::ServiceWorkerClientType ServiceWorkerClient::GetClientType()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::visit(
      base::Overloaded(
          [](GlobalRenderFrameHostId render_frame_host_id) {
            return blink::mojom::ServiceWorkerClientType::kWindow;
          },
          [](blink::DedicatedWorkerToken dedicated_worker_token) {
            return blink::mojom::ServiceWorkerClientType::kDedicatedWorker;
          },
          [](blink::SharedWorkerToken shared_worker_token) {
            return blink::mojom::ServiceWorkerClientType::kSharedWorker;
          }),
      client_info_);
}

bool ServiceWorkerClient::IsContainerForWindowClient() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::holds_alternative<GlobalRenderFrameHostId>(client_info_);
}

bool ServiceWorkerClient::IsContainerForWorkerClient() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::holds_alternative<blink::DedicatedWorkerToken>(client_info_) ||
         std::holds_alternative<blink::SharedWorkerToken>(client_info_);
}

ServiceWorkerClientInfo ServiceWorkerClient::GetServiceWorkerClientInfo()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return client_info_;
}

blink::mojom::ServiceWorkerContainerInfoForClientPtr
ServiceWorkerClient::CommitResponse(
    base::PassKey<ScopedServiceWorkerClient>,
    std::optional<GlobalRenderFrameHostId> rfh_id,
    const PolicyContainerPolicies& policy_container_policies,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    mojo::PendingRemote<network::mojom::DocumentIsolationPolicyReporter>
        dip_reporter,
    ukm::SourceId ukm_source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(client_phase_, ClientPhase::kInitial);

  if (IsContainerForWindowClient()) {
    CHECK(coep_reporter);
    CHECK(rfh_id);
    ongoing_navigation_frame_tree_node_id_ = FrameTreeNodeId();
    client_info_ = *rfh_id;

    if (controller_) {
      controller_->UpdateForegroundPriority();
    }

    auto* rfh = RenderFrameHostImpl::FromID(*rfh_id);
    // `rfh` may be null in tests (but it should not happen in production).
    if (rfh) {
      rfh->AddServiceWorkerClient(client_uuid(),
                                  weak_ptr_factory_.GetWeakPtr());
    }
  }

  CHECK(!container_host_);

  auto container_info =
      blink::mojom::ServiceWorkerContainerInfoForClient::New();
  container_host_ = std::make_unique<ServiceWorkerContainerHostForClient>(
      base::PassKey<ServiceWorkerClient>(), AsWeakPtr(), container_info,
      policy_container_policies, std::move(coep_reporter),
      std::move(dip_reporter), std::move(ukm_source_id));

  // `network_url_loader_factory_for_prefetch_` is no longer used after commit.
  network_url_loader_factory_for_prefetch_.reset();

  TransitionToClientPhase(ClientPhase::kResponseCommitted);

  return container_info;
}

void ServiceWorkerClient::OnEndNavigationCommit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForWindowClient());

  DCHECK(!navigation_commit_ended_);
  navigation_commit_ended_ = true;

  if (controller_) {
    controller_->OnControlleeNavigationCommitted(client_uuid_,
                                                 GetRenderFrameHostId());
  }
}

void ServiceWorkerClient::UpdateUrlsInternal(
    const GURL& creation_url,
    const std::optional<url::Origin>& top_frame_origin,
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const GURL url = creation_url.is_valid()
                       ? net::SimplifyUrlForRequest(creation_url)
                       : creation_url;
  GURL previous_url = url_;
  creation_url_ = creation_url;
  // The url_ needs the URL fragment removed, but the creation URL needs to be
  // the original URL including the fragment.
  url_ = url;
  top_frame_origin_ = top_frame_origin;
  key_ = storage_key;
  service_worker_security_utils::CheckOnUpdateUrls(GetUrlForScopeMatch(), key_);

  if (previous_url != url) {
    // Revoke the token on URL change since any service worker holding the token
    // may no longer be the potential controller of this frame and shouldn't
    // have the power to display SSL dialogs for it.
    if (IsContainerForWindowClient()) {
      fetch_request_window_id_ = base::UnguessableToken::Create();
    }
  }

  // Update client id on cross origin redirects. This corresponds to the HTML
  // standard's "process a navigation fetch" algorithm's step for discarding
  // |reservedEnvironment|.
  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#process-a-navigate-fetch
  // "If |reservedEnvironment| is not null and |currentURL|'s origin is not the
  // same as |reservedEnvironment|'s creation URL's origin, then:
  //    1. Run the environment discarding steps for |reservedEnvironment|.
  //    2. Set |reservedEnvironment| to null."
  if (previous_url.is_valid() && !url::IsSameOriginWith(previous_url, url)) {
    // Remove old controller since we know the controller is definitely
    // changed. We need to remove |this| from |controller_|'s controllee before
    // updating UUID since ServiceWorkerVersion has a map from uuid to provider
    // hosts.
    SetControllerRegistration(nullptr, false /* notify_controllerchange */);

    // Set UUID to the new one.
    std::string previous_client_uuid = client_uuid_;
    client_uuid_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
    owner_->UpdateServiceWorkerClientClientID(previous_client_uuid,
                                              client_uuid_);
  }

  SyncMatchingRegistrations();
}

namespace {

// Attempt to get the storage key from |RenderFrameHostImpl|. This correctly
// accounts for extension URLs. The absence of this logic was a potential cause
// for https://crbug.com/1346450.
std::optional<blink::StorageKey> GetStorageKeyFromRenderFrameHost(
    FrameTreeNodeId frame_tree_node_id,
    const url::Origin& origin,
    const base::UnguessableToken* nonce) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node) {
    return std::nullopt;
  }
  RenderFrameHostImpl* frame_host = frame_tree_node->current_frame_host();
  if (!frame_host) {
    return std::nullopt;
  }

  return frame_host->CalculateStorageKey(origin, nonce);
}

// For dedicated/shared worker cases, if a storage key is returned, it will have
// its origin replaced by |origin|. This would mean that the origin of the
// WorkerHost and the origin as used by the service worker code don't match,
// however in cases where these wouldn't match the load will be aborted later
// anyway.
std::optional<blink::StorageKey> GetStorageKeyFromDedicatedWorkerHost(
    content::StoragePartition* storage_partition,
    blink::DedicatedWorkerToken dedicated_worker_token,
    const url::Origin& origin) {
  auto* worker_service = static_cast<DedicatedWorkerServiceImpl*>(
      storage_partition->GetDedicatedWorkerService());
  auto* worker_host =
      worker_service->GetDedicatedWorkerHostFromToken(dedicated_worker_token);
  if (worker_host) {
    return worker_host->GetStorageKey().WithOrigin(origin);
  }
  return std::nullopt;
}

std::optional<blink::StorageKey> GetStorageKeyFromSharedWorkerHost(
    content::StoragePartition* storage_partition,
    blink::SharedWorkerToken shared_worker_token,
    const url::Origin& origin) {
  auto* worker_service = static_cast<SharedWorkerServiceImpl*>(
      storage_partition->GetSharedWorkerService());
  auto* worker_host =
      worker_service->GetSharedWorkerHostFromToken(shared_worker_token);
  if (worker_host) {
    return worker_host->GetStorageKey().WithOrigin(origin);
  }
  return std::nullopt;
}

}  // namespace

blink::StorageKey ServiceWorkerClient::CalculateStorageKeyForUpdateUrls(
    const GURL& url,
    const net::IsolationInfo& isolation_info_from_handle) const {
  CHECK(!is_response_committed());

  const url::Origin origin = url::Origin::Create(url);

  const std::optional<blink::StorageKey> storage_key = std::visit(
      base::Overloaded(
          [&](GlobalRenderFrameHostId render_frame_host_id) {
            if (is_initiated_by_prefetch_) {
              // Falls back to the `CreateFromOriginAndIsolationInfo()` case
              // below.
              // Navigation isn't served by prefetch if the key for prefetch
              // calculated here is wrong/mismatching, checked at
              // `PrefetchURLLoaderInterceptor::OnGetPrefetchComplete()`.
              // https://crbug.com/413207408.
              return std::optional<blink::StorageKey>(std::nullopt);
            }
            // We use `ongoing_navigation_frame_tree_node_id_` instead of
            // `render_frame_host_id` because this method is called before
            // response commit.
            return GetStorageKeyFromRenderFrameHost(
                ongoing_navigation_frame_tree_node_id_, origin,
                base::OptionalToPtr(isolation_info_from_handle.nonce()));
          },
          [&](blink::DedicatedWorkerToken dedicated_worker_token) {
            auto* process = RenderProcessHost::FromID(GetProcessId());
            return process ? GetStorageKeyFromDedicatedWorkerHost(
                                 process->GetStoragePartition(),
                                 dedicated_worker_token, origin)
                           : std::nullopt;
          },
          [&](blink::SharedWorkerToken shared_worker_token) {
            auto* process = RenderProcessHost::FromID(GetProcessId());
            return process ? GetStorageKeyFromSharedWorkerHost(
                                 process->GetStoragePartition(),
                                 shared_worker_token, origin)
                           : std::nullopt;
          }),
      client_info_);

  if (storage_key) {
    return *storage_key;
  }

  // If we're in this case then we couldn't get the StorageKey from the RFH,
  // which means we also can't get the storage partitioning status from
  // RuntimeFeatureState(Read)Context. Using
  // CreateFromOriginAndIsolationInfo() will create a key based on
  // net::features::kThirdPartyStoragePartitioning state.
  return blink::StorageKey::CreateFromOriginAndIsolationInfo(
      origin, isolation_info_from_handle);
}

void ServiceWorkerClient::UpdateUrls(
    const GURL& creation_url,
    const std::optional<url::Origin>& top_frame_origin,
    const blink::StorageKey& storage_key) {
  CHECK(!is_response_committed());
  UpdateUrlsInternal(creation_url, top_frame_origin, storage_key);
}

void ServiceWorkerClient::UpdateUrlsAfterCommitResponseForTesting(
    const GURL& url,
    const std::optional<url::Origin>& top_frame_origin,
    const blink::StorageKey& storage_key) {
  CHECK(is_response_committed());
  UpdateUrlsInternal(url, top_frame_origin, storage_key);
}

void ServiceWorkerClient::SetControllerRegistration(
    scoped_refptr<ServiceWorkerRegistration> controller_registration,
    bool notify_controllerchange) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (controller_registration) {
    CHECK(IsEligibleForServiceWorkerController());
    DCHECK(controller_registration->active_version());
#if DCHECK_IS_ON()
    DCHECK(IsMatchingRegistration(controller_registration.get()));
#endif  // DCHECK_IS_ON()
  }

  controller_registration_ = controller_registration;
  UpdateController(notify_controllerchange);
}

bool ServiceWorkerClient::IsEligibleForServiceWorkerController() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!url_.is_valid()) {
    return false;
  }
  // Pass GetUrlForScopeMatch() instead of `url_` because we cannot take the
  // origin of `url_` when it's a blob URL (see https://crbug.com/1144717). It's
  // guaranteed that the URL returned by GetURLForScopeMatch() has the same
  // logical origin as `url_`.
  // TODO(asamidoi): Add url::Origin member for ServiceWorkerContainerHost and
  // use it as the argument of OriginCanAccessServiceWorkers().
  if (!OriginCanAccessServiceWorkers(GetUrlForScopeMatch())) {
    return false;
  }

  if (is_parent_frame_secure_) {
    return true;
  }

  std::set<std::string> schemes;
  GetContentClient()->browser()->GetSchemesBypassingSecureContextCheckAllowlist(
      &schemes);
  return schemes.find(url_.scheme()) != schemes.end();
}

bool ServiceWorkerClient::is_response_committed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (client_phase_) {
    case ClientPhase::kInitial:
    case ClientPhase::kResponseNotCommitted:
      return false;
    case ClientPhase::kResponseCommitted:
    case ClientPhase::kContainerReady:
    case ClientPhase::kExecutionReady:
      return true;
  }
}

bool ServiceWorkerClient::is_container_ready() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (client_phase_) {
    case ClientPhase::kInitial:
    case ClientPhase::kResponseCommitted:
    case ClientPhase::kResponseNotCommitted:
      return false;
    case ClientPhase::kContainerReady:
    case ClientPhase::kExecutionReady:
      return true;
  }
}

void ServiceWorkerClient::AddExecutionReadyCallback(
    ExecutionReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_execution_ready());
  execution_ready_callbacks_.push_back(std::move(callback));
}

bool ServiceWorkerClient::is_execution_ready() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (client_phase_) {
    case ClientPhase::kInitial:
    case ClientPhase::kResponseCommitted:
    case ClientPhase::kResponseNotCommitted:
    case ClientPhase::kContainerReady:
      return false;
    case ClientPhase::kExecutionReady:
      return true;
  }
}

GlobalRenderFrameHostId ServiceWorkerClient::GetRenderFrameHostId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForWindowClient());
  return std::get<GlobalRenderFrameHostId>(client_info_);
}

int ServiceWorkerClient::GetProcessId() const {
  if (IsContainerForWindowClient()) {
    return GetRenderFrameHostId().child_id;
  }
  DCHECK(IsContainerForWorkerClient());
  return process_id_for_worker_client_;
}

NavigationRequest* ServiceWorkerClient::GetOngoingNavigationRequestBeforeCommit(
    base::PassKey<StoragePartitionImpl>) const {
  DCHECK(IsContainerForWindowClient());
  DCHECK(!GetRenderFrameHostId());

  // For Window clients for prefetch,
  // `GetOngoingNavigationRequestBeforeCommit()` isn't called at all, because
  // prefetching requests don't set `URLLoaderNetworkServiceObserver`.
  CHECK(!is_initiated_by_prefetch_);

  // It is safe to use `ongoing_navigation_frame_tree_node_id_` to obtain the
  // corresponding navigation request without being concerned about the case
  // that a new navigation had started and the old navigation had been deleted,
  // because the owner of this instance will reset the key that can be used to
  // retrieve this instance, which makes the old key stale and cannot locate
  // this instance. This mechanism guarantees that this instance would always be
  // associated with the latest navigation.
  // However, this design requires callers to carefully get the
  // `ServiceWorkerContainerHost` instance from scratch instead of using a
  // stored one, and it would be better to optimize the design when possible.
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(ongoing_navigation_frame_tree_node_id_);
  return frame_tree_node ? frame_tree_node->navigation_request() : nullptr;
}

std::string ServiceWorkerClient::GetFrameTreeNodeTypeStringBeforeCommit()
    const {
  CHECK(!is_response_committed());
  // TODO(https://crbug.com/40947546): If needed, assign a proper metrics name
  // for clients for prefetch where `ongoing_navigation_frame_tree_node_id` is
  // null.
  if (FrameTreeNode* frame_tree_node = FrameTreeNode::GloballyFindByID(
          ongoing_navigation_frame_tree_node_id_)) {
    CHECK(IsContainerForWindowClient());
    return frame_tree_node->IsOutermostMainFrame() ? "OutermostMainFrame"
                                                   : "NotOutermostMainFrame";
  }
  return "Unknown";
}

const std::string& ServiceWorkerClient::client_uuid() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return client_uuid_;
}

std::string ServiceWorkerClient::client_uuid_for_resulting_client_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_initiated_by_prefetch_) {
    return "";
  }
  return client_uuid_;
}

ServiceWorkerVersion* ServiceWorkerClient::controller() const {
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CheckControllerConsistency(false);
#endif  // DCHECK_IS_ON()
  return controller_.get();
}

ServiceWorkerRegistration* ServiceWorkerClient::controller_registration()
    const {
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CheckControllerConsistency(false);
#endif  // DCHECK_IS_ON()
  return controller_registration_.get();
}

bool ServiceWorkerClient::IsInBackForwardCache() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_in_back_forward_cache_;
}

void ServiceWorkerClient::EvictFromBackForwardCache(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsBackForwardCacheEnabled());
  is_in_back_forward_cache_ = false;

  if (!IsContainerForWindowClient()) {
    return;
  }

  auto* rfh = RenderFrameHostImpl::FromID(GetRenderFrameHostId());
  // |rfh| could be evicted before this function is called.
  if (rfh && rfh->IsInBackForwardCache()) {
    rfh->EvictFromBackForwardCacheWithReason(reason);
  }
}

void ServiceWorkerClient::OnEnterBackForwardCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsBackForwardCacheEnabled());
  if (controller_) {
    // TODO(crbug.com/330928087): remove check when this issue resolved.
    SCOPED_CRASH_KEY_NUMBER("SWC_OnEBFC", "client_type",
                            static_cast<int32_t>(GetClientType()));
    SCOPED_CRASH_KEY_BOOL("SWC_OnEBFC", "is_execution_ready",
                          is_execution_ready());
    SCOPED_CRASH_KEY_BOOL("SWC_OnEBFC", "is_blob_or_about_url",
                          url() != GetUrlForScopeMatch());
    SCOPED_CRASH_KEY_BOOL("SWC_OnEBFC", "is_inherited", is_inherited());
    CHECK(!controller_->BFCacheContainsControllee(client_uuid()));
    controller_->MoveControlleeToBackForwardCacheMap(client_uuid());
  }
  is_in_back_forward_cache_ = true;
}

void ServiceWorkerClient::OnRestoreFromBackForwardCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsBackForwardCacheEnabled());
  // TODO(crbug.com/330928087): remove check when this issue resolved.
  SCOPED_CRASH_KEY_BOOL("SWC_OnRFBFC", "is_in_bfcache",
                        is_in_back_forward_cache_);
  SCOPED_CRASH_KEY_BOOL("SWC_OnRFBFC", "is_blob_or_about_url",
                        url() != GetUrlForScopeMatch());
  SCOPED_CRASH_KEY_BOOL("SWC_OnRFBFC", "is_inherited", is_inherited());
  if (controller_) {
    controller_->RestoreControlleeFromBackForwardCacheMap(client_uuid());
  }
  is_in_back_forward_cache_ = false;
}

void ServiceWorkerClient::SyncMatchingRegistrations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!controller_registration_);

  RemoveAllMatchingRegistrations();
  if (!context_) {
    return;
  }
  const auto& registrations = context_->GetLiveRegistrations();
  for (const auto& key_registration : registrations) {
    ServiceWorkerRegistration* registration = key_registration.second;
    if (!registration->is_uninstalled() && registration->key() == key() &&
        blink::ServiceWorkerScopeMatches(registration->scope(),
                                         GetUrlForScopeMatch())) {
      AddMatchingRegistration(registration);
    }
  }
}

#if DCHECK_IS_ON()
bool ServiceWorkerClient::IsMatchingRegistration(
    ServiceWorkerRegistration* registration) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string spec = registration->scope().spec();
  size_t key = spec.size();

  auto iter = matching_registrations_.find(key);
  if (iter == matching_registrations_.end()) {
    return false;
  }
  if (iter->second.get() != registration) {
    return false;
  }
  return true;
}
#endif  // DCHECK_IS_ON()

void ServiceWorkerClient::RemoveAllMatchingRegistrations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!controller_registration_);
  for (const auto& it : matching_registrations_) {
    ServiceWorkerRegistration* registration = it.second.get();
    registration->RemoveListener(this);
  }
  matching_registrations_.clear();
}

void ServiceWorkerClient::SetExecutionReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (client_phase_) {
    case ClientPhase::kInitial:
      // When `CommitResponse()` is not yet called, it should be skipped because
      // the service worker client initialization failed somewhere, and thus
      // ignore the `SetExecutionReady()` call here and transition to
      // `kResponseNotCommitted` to confirm that no further transitions are
      // attempted. See also the comment at `kResponseNotCommitted` in the
      // header file.
      TransitionToClientPhase(ClientPhase::kResponseNotCommitted);
      break;

    case ClientPhase::kContainerReady:
      // Successful case.
      TransitionToClientPhase(ClientPhase::kExecutionReady);
      RunExecutionReadyCallbacks();

      if (context_) {
        context_->NotifyClientIsExecutionReady(*this);
      }

      FlushFeatures();
      break;

    case ClientPhase::kResponseCommitted:
    case ClientPhase::kExecutionReady:
    case ClientPhase::kResponseNotCommitted:
      // Invalid state transition.
      NOTREACHED()
          << "ServiceWorkerClient::SetExecutionReady() called on ClientPhase "
          << static_cast<int>(client_phase_);
  }
}

void ServiceWorkerClient::RunExecutionReadyCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<ExecutionReadyCallback> callbacks;
  execution_ready_callbacks_.swap(callbacks);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RunCallbacks, std::move(callbacks)));
}

void ServiceWorkerClient::TransitionToClientPhase(ClientPhase new_phase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_phase_ == new_phase) {
    return;
  }
  switch (client_phase_) {
    case ClientPhase::kInitial:
      CHECK(new_phase == ClientPhase::kResponseCommitted ||
            new_phase == ClientPhase::kResponseNotCommitted);
      break;
    case ClientPhase::kResponseCommitted:
      CHECK_EQ(new_phase, ClientPhase::kContainerReady);
      break;
    case ClientPhase::kContainerReady:
      CHECK_EQ(new_phase, ClientPhase::kExecutionReady);
      break;
    case ClientPhase::kExecutionReady:
    case ClientPhase::kResponseNotCommitted:
      NOTREACHED() << "Invalid transition from "
                   << static_cast<int>(client_phase_);
  }
  client_phase_ = new_phase;
}

void ServiceWorkerClient::UpdateController(bool notify_controllerchange) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ServiceWorkerVersion* version =
      controller_registration_ ? controller_registration_->active_version()
                               : nullptr;
  CHECK(!version || IsEligibleForServiceWorkerController());
  if (version == controller_.get()) {
    return;
  }

  scoped_refptr<ServiceWorkerVersion> previous_version = controller_;
  controller_ = version;
  if (version) {
    // TODO(crbug.com/330928087): remove check when this issue resolved.
    SCOPED_CRASH_KEY_NUMBER("SWV_RCFBCM", "client_type",
                            static_cast<int32_t>(GetClientType()));
    SCOPED_CRASH_KEY_BOOL("SWV_RCFBCM", "is_execution_ready",
                          is_execution_ready());
    SCOPED_CRASH_KEY_BOOL("SWV_RCFBCM", "is_blob_or_about_url",
                          url() != GetUrlForScopeMatch());
    SCOPED_CRASH_KEY_BOOL("SWV_RCFBCM", "is_inherited", is_inherited());
    CHECK(!version->BFCacheContainsControllee(client_uuid()));
    version->AddControllee(this);
    if (IsBackForwardCacheEnabled() && IsInBackForwardCache()) {
      // |this| was not |version|'s controllee when |OnEnterBackForwardCache|
      // was called.
      version->MoveControlleeToBackForwardCacheMap(client_uuid());
    }
    if (running_status_observer_) {
      version->AddObserver(running_status_observer_.get());
      running_status_observer_->Notify(version->running_status());
    }
  }
  if (previous_version) {
    previous_version->Uncontrol(client_uuid());
    if (running_status_observer_) {
      previous_version->RemoveObserver(running_status_observer_.get());
    }
  }

  // No need to `SetController` if the container is not ready because
  // when the container gets ready, `ControllerServiceWorkerInfoPtr` is also
  // sent in the same IPC call. Moreover, it is harmful to resend the past
  // SetController to the renderer because it moves the controller in the
  // renderer to the past one.
  if (!is_container_ready()) {
    return;
  }

  if (!is_execution_ready()) {
    return;
  }

  container_host()->SendSetController(notify_controllerchange);
}

#if DCHECK_IS_ON()
void ServiceWorkerClient::CheckControllerConsistency(bool should_crash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!controller_) {
    DCHECK(!controller_registration_);
    return;
  }

  DCHECK(controller_registration_);
  DCHECK_EQ(controller_->registration_id(), controller_registration_->id());

  switch (controller_->status()) {
    case ServiceWorkerVersion::NEW:
    case ServiceWorkerVersion::INSTALLING:
    case ServiceWorkerVersion::INSTALLED:
      if (should_crash) {
        ServiceWorkerVersion::Status status = controller_->status();
        base::debug::Alias(&status);
        NOTREACHED() << "Controller service worker has a bad status: "
                     << ServiceWorkerVersion::VersionStatusToString(status);
      }
      break;
    case ServiceWorkerVersion::REDUNDANT: {
      if (should_crash) {
        NOTREACHED();
      }
      break;
    }
    case ServiceWorkerVersion::ACTIVATING:
    case ServiceWorkerVersion::ACTIVATED:
      // Valid status, controller is being activated.
      break;
  }
}
#endif  // DCHECK_IS_ON()

const GURL& ServiceWorkerClient::GetUrlForScopeMatch() const {
  if (!scope_match_url_for_client_.is_empty()) {
    return scope_match_url_for_client_;
  }
  return url_;
}

void ServiceWorkerClient::InheritControllerFrom(
    ServiceWorkerClient& creator_host,
    const GURL& client_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetClientType() ==
             blink::mojom::ServiceWorkerClientType::kDedicatedWorker ||
         (base::FeatureList::IsEnabled(kSharedWorkerBlobURLFix) &&
          GetClientType() ==
              blink::mojom::ServiceWorkerClientType::kSharedWorker) ||
         (base::FeatureList::IsEnabled(features::kServiceWorkerSrcdocSupport) &&
          GetClientType() == blink::mojom::ServiceWorkerClientType::kWindow &&
          client_url.IsAboutSrcdoc()));
  // Only expect srcdoc url or blob url of same origin as creator for
  // client_url.
  DCHECK((client_url.SchemeIsBlob() &&
          url::Origin::Create(client_url)
              .IsSameOriginWith(creator_host.key().origin())) ||
         (base::FeatureList::IsEnabled(features::kServiceWorkerSrcdocSupport) &&
          client_url.IsAboutSrcdoc()));

  // Let `scope_match_url_for_client_` be the creator's url for scope match
  // because a client should be handled by the service worker of its creator.
  // Update it before UpdateUrls so that CheckOnUpdateUrls inside UpdateUrls
  // checks with the updated GetUrlForScopeMatch().
  scope_match_url_for_client_ = creator_host.GetUrlForScopeMatch();

  UpdateUrls(client_url, creator_host.top_frame_origin(), creator_host.key());

  // Inherit the controller of the creator.
  if (creator_host.controller_registration()) {
    AddMatchingRegistration(creator_host.controller_registration());
    // If the creator is in back forward cache, the client should also
    // be in back forward cache. Otherwise, CHECK fail during restoring from
    // back forward cache.
    is_in_back_forward_cache_ = creator_host.is_in_back_forward_cache();
    // TODO(crbug.com/341322515): remove this CHECK.
    // This CHECK is to ensure this path does not cause the crash at
    // ServiceWorkerVersion::RemoveControlleeFromBackForwardCacheMap().
    CHECK(creator_host.controller_registration()->active_version());
    SetControllerRegistration(creator_host.controller_registration(),
                              false /* notify_controllerchange */);
  }
  creator_host.SetInherited();
}

mojo::PendingReceiver<blink::mojom::ServiceWorkerRunningStatusCallback>
ServiceWorkerClient::GetRunningStatusCallbackReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(controller_);
  if (!running_status_observer_) {
    running_status_observer_ =
        absl::make_unique<ServiceWorkerRunningStatusObserver>();
    controller_->AddObserver(running_status_observer_.get());
  }
  mojo::PendingRemote<blink::mojom::ServiceWorkerRunningStatusCallback>
      remote_callback;
  auto receiver = remote_callback.InitWithNewPipeAndPassReceiver();
  running_status_observer_->AddCallback(std::move(remote_callback));
  return receiver;
}

void ServiceWorkerClient::SetContainerReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransitionToClientPhase(ClientPhase::kContainerReady);
  CHECK(container_host()->IsContainerRemoteConnected());

  FlushFeatures();
}

void ServiceWorkerClient::FlushFeatures() {
  std::set<blink::mojom::WebFeature> features;
  features.swap(buffered_used_features_);
  for (const auto& feature : features) {
    CountFeature(feature);
  }
}

void ServiceWorkerClient::SetNetworkURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  CHECK_IS_TEST();
  network_url_loader_factory_override_for_testing_ = url_loader_factory;
}

scoped_refptr<network::SharedURLLoaderFactory>
ServiceWorkerClient::CreateNetworkURLLoaderFactory(
    CreateNetworkURLLoaderFactoryType type,
    StoragePartitionImpl* storage_partition,
    const network::ResourceRequest& resource_request) {
  CHECK(!is_response_committed());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (network_url_loader_factory_override_for_testing_) {
    CHECK_IS_TEST();
    return network_url_loader_factory_override_for_testing_;
  }

  if (is_initiated_by_prefetch_) {
    // We skip `WillCreateURLLoaderFactory` below, because it is already
    // included in `network_url_loader_factory_for_prefetch_` (see
    // `PrefetchNetworkContext::CreateNewURLLoaderFactory()`).
    // We also skip `CreateURLLoaderHandlerForServiceWorkerNavigationPreload`,
    // because this is a prefetch request and don't have to consult with search
    // prefetch cache via
    // `CreateURLLoaderHandlerForServiceWorkerNavigationPreload`.
    return network_url_loader_factory_for_prefetch_;
  }

  switch (type) {
    case CreateNetworkURLLoaderFactoryType::kNavigationPreload:
      // Allow the embedder to intercept the URLLoader request if necessary.
      // This must be a synchronous decision by the embedder. In the future, we
      // may wish to support asynchronous decisions using
      // |URLLoaderRequestInterceptor| in the same fashion that they are used
      // for navigation requests.
      if (ContentBrowserClient::URLLoaderRequestHandler
              embedder_url_loader_handler =
                  GetContentClient()
                      ->browser()
                      ->CreateURLLoaderHandlerForServiceWorkerNavigationPreload(
                          ongoing_navigation_frame_tree_node_id_,
                          resource_request)) {
        return base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
            std::move(embedder_url_loader_handler));
      }
      break;
    case CreateNetworkURLLoaderFactoryType::kRaceNetworkRequest:
    case CreateNetworkURLLoaderFactoryType::kSyntheticNetworkRequest:
      break;
  }

  // For worker clients, `ongoing_navigation_frame_tree_node_id_` is null.
  // TODO(falken): Can `navigation_request` check be a DCHECK now that the
  // caller does not post a task to this function?
  auto* frame_tree_node =
      FrameTreeNode::GloballyFindByID(ongoing_navigation_frame_tree_node_id_);
  if (!frame_tree_node || !storage_partition ||
      !frame_tree_node->navigation_request()) {
    // The navigation was cancelled. Just drop the request. Otherwise, we might
    // go to network without consulting the embedder first, which would break
    // guarantees.
    //
    // TODO(https://crbug.com/40947546): Clients for prefetch (where
    // `ongoing_navigation_frame_tree_node_id` is null) also fall into this case
    // and thus don't support navigationPreload and race network requests. Fix
    // this.
    mojo::PendingRemote<network::mojom::URLLoaderFactory> network_factory;
    return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
        std::move(network_factory));
  }

  // We ignore the value of |bypass_redirect_checks_unused| since a redirect is
  // just relayed to the service worker where preloadResponse is resolved as
  // redirect.
  bool bypass_redirect_checks_unused;

  // Consult the embedder.
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  network::URLLoaderFactoryBuilder factory_builder;
  // Here we give nullptr for |factory_override|, because CORS is no-op
  // for navigations.
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      storage_partition->browser_context(),
      frame_tree_node->current_frame_host(),
      frame_tree_node->current_frame_host()->GetProcess()->GetDeprecatedID(),
      ContentBrowserClient::URLLoaderFactoryType::kNavigation, url::Origin(),
      net::IsolationInfo(),
      frame_tree_node->navigation_request()->GetNavigationId(),
      ukm::SourceIdObj::FromInt64(
          frame_tree_node->navigation_request()->GetNextPageUkmSourceId()),
      factory_builder, &header_client, &bypass_redirect_checks_unused,
      /*disable_secure_dns=*/nullptr, /*factory_override=*/nullptr,
      GetUIThreadTaskRunner({BrowserTaskType::kNavigationNetworkResponse}));

  // Make the network factory.
  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      NavigationURLLoaderImpl::CreateURLLoaderFactoryWithHeaderClient(
          std::move(header_client), std::move(factory_builder),
          storage_partition,
          // TODO(crbug.com/390003764): Consider whether/how to apply devtools
          // cookies setting overrides for a service worker.
          /*devtools_cookie_overrides=*/std::nullopt,
          /*cookie_overrides=*/std::nullopt));
}

// If a blob URL is used for a SharedWorker script's URL, a controller will be
// inherited.
BASE_FEATURE(kSharedWorkerBlobURLFix,
             "SharedWorkerBlobURLFix",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace content
