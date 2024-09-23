// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_client.h"

#include <set>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/functional/overloaded.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/remote_set.h"
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
    FrameTreeNodeId frame_tree_node_id)
    : context_(std::move(context)),
      owner_(context_->service_worker_client_owner()),
      create_time_(base::TimeTicks::Now()),
      client_uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      is_parent_frame_secure_(is_parent_frame_secure),
      client_info_(ServiceWorkerClientInfo()),
      process_id_for_worker_client_(ChildProcessHost::kInvalidUniqueID),
      ongoing_navigation_frame_tree_node_id_(frame_tree_node_id) {
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
  DCHECK(client_info_);
  return absl::visit(
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
      *client_info_);
}

bool ServiceWorkerClient::IsContainerForWindowClient() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return client_info_ &&
         absl::holds_alternative<GlobalRenderFrameHostId>(*client_info_);
}

bool ServiceWorkerClient::IsContainerForWorkerClient() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  using blink::mojom::ServiceWorkerClientType;
  if (!client_info_) {
    return false;
  }

  return absl::holds_alternative<blink::DedicatedWorkerToken>(*client_info_) ||
         absl::holds_alternative<blink::SharedWorkerToken>(*client_info_);
}

ServiceWorkerClientInfo ServiceWorkerClient::GetServiceWorkerClientInfo()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *client_info_;
}

blink::mojom::ServiceWorkerContainerInfoForClientPtr
ServiceWorkerClient::CommitResponse(
    base::PassKey<ScopedServiceWorkerClient>,
    std::optional<GlobalRenderFrameHostId> rfh_id,
    const PolicyContainerPolicies& policy_container_policies,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
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
      std::move(ukm_source_id));

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
    const GURL& url,
    const std::optional<url::Origin>& top_frame_origin,
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!url.has_ref());

  GURL previous_url = url_;
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

void ServiceWorkerClient::UpdateUrls(
    const GURL& url,
    const std::optional<url::Origin>& top_frame_origin,
    const blink::StorageKey& storage_key) {
  CHECK(!is_response_committed());
  UpdateUrlsInternal(url, top_frame_origin, storage_key);
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
  return absl::get<GlobalRenderFrameHostId>(*client_info_);
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
  DCHECK(ongoing_navigation_frame_tree_node_id_);
  DCHECK(!GetRenderFrameHostId());

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

const std::string& ServiceWorkerClient::client_uuid() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    SCOPED_CRASH_KEY_BOOL("SWC_OnEBFC", "is_blob_url",
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
  SCOPED_CRASH_KEY_BOOL("SWC_OnRFBFC", "is_blob_url",
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
    SCOPED_CRASH_KEY_BOOL("SWV_RCFBCM", "is_blob_url",
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
        CHECK(false) << "Controller service worker has a bad status: "
                     << ServiceWorkerVersion::VersionStatusToString(status);
      }
      break;
    case ServiceWorkerVersion::REDUNDANT: {
      if (should_crash) {
        CHECK(false);
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
  if (!scope_match_url_for_blob_client_.is_empty()) {
    return scope_match_url_for_blob_client_;
  }
  return url_;
}

void ServiceWorkerClient::InheritControllerFrom(
    ServiceWorkerClient& creator_host,
    const GURL& blob_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(kSharedWorkerBlobURLFix) ||
         blink::mojom::ServiceWorkerClientType::kDedicatedWorker ==
             GetClientType());
  DCHECK(blob_url.SchemeIsBlob());

  UpdateUrls(blob_url, creator_host.top_frame_origin(), creator_host.key());

  // Let `scope_match_url_for_blob_client_` be the creator's url for scope match
  // because a client should be handled by the service worker of its creator.
  scope_match_url_for_blob_client_ = creator_host.GetUrlForScopeMatch();

  // Inherit the controller of the creator.
  if (creator_host.controller_registration()) {
    AddMatchingRegistration(creator_host.controller_registration());
    // If the creator is in back forward cache, the client should also
    // be in back forward cache. Otherwise, CHECK fail during restoring from
    // back forward cache.
    is_in_back_forward_cache_ = creator_host.is_in_back_forward_cache();
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

// If a blob URL is used for a SharedWorker script's URL, a controller will be
// inherited.
BASE_FEATURE(kSharedWorkerBlobURLFix,
             "SharedWorkerBlobURLFix",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace content
