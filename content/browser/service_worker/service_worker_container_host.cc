// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_container_host.h"

#include <set>
#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace content {

namespace {

void RunCallbacks(
    std::vector<ServiceWorkerContainerHost::ExecutionReadyCallback> callbacks) {
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

ServiceWorkerMetrics::EventType PurposeToEventType(
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  switch (purpose) {
    case blink::mojom::ControllerServiceWorkerPurpose::FETCH_SUB_RESOURCE:
      return ServiceWorkerMetrics::EventType::FETCH_SUB_RESOURCE;
  }
  NOTREACHED();
  return ServiceWorkerMetrics::EventType::UNKNOWN;
}

}  // namespace

// RAII helper class for keeping track of versions waiting for an update hint
// from the renderer.
//
// This class is move-only.
class ServiceWorkerContainerHost::PendingUpdateVersion {
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
    if (version_)
      version_->DecrementPendingUpdateHintCount();
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

ServiceWorkerContainerHost::ServiceWorkerContainerHost(
    base::WeakPtr<ServiceWorkerContextCore> context)
    : context_(std::move(context)), create_time_(base::TimeTicks::Now()) {
  DCHECK(IsContainerForServiceWorker());
  DCHECK(context_);
}

ServiceWorkerContainerHost::ServiceWorkerContainerHost(
    base::WeakPtr<ServiceWorkerContextCore> context,
    bool is_parent_frame_secure,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote,
    int frame_tree_node_id)
    : context_(std::move(context)),
      create_time_(base::TimeTicks::Now()),
      client_uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      is_parent_frame_secure_(is_parent_frame_secure),
      container_(std::move(container_remote)),
      client_info_(ServiceWorkerClientInfo()),
      ongoing_navigation_frame_tree_node_id_(frame_tree_node_id) {
  DCHECK(IsContainerForWindowClient());
  DCHECK(context_);
  DCHECK(container_.is_bound());
}

ServiceWorkerContainerHost::ServiceWorkerContainerHost(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int process_id,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote,
    ServiceWorkerClientInfo client_info)
    : context_(std::move(context)),
      create_time_(base::TimeTicks::Now()),
      client_uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      container_(std::move(container_remote)),
      client_info_(client_info),
      process_id_for_worker_client_(process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForWorkerClient());
  DCHECK(context_);
  DCHECK_NE(process_id_for_worker_client_, ChildProcessHost::kInvalidUniqueID);
  DCHECK(container_.is_bound());
}

ServiceWorkerContainerHost::~ServiceWorkerContainerHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsContainerForWindowClient()) {
    auto* rfh = RenderFrameHostImpl::FromID(GetRenderFrameHostId());
    if (rfh)
      rfh->RemoveServiceWorkerContainerHost(client_uuid());
  }

  if (controller_) {
    DCHECK(IsContainerForClient());
    controller_->Uncontrol(client_uuid());
  }

  // Remove |this| as an observer of ServiceWorkerRegistrations.
  // TODO(falken): Use base::ScopedObservation instead of this explicit call.
  controller_.reset();
  controller_registration_.reset();

  // Ensure callbacks awaiting execution ready are notified.
  if (IsContainerForClient())
    RunExecutionReadyCallbacks();

  RemoveAllMatchingRegistrations();
}

void ServiceWorkerContainerHost::Register(
    const GURL& script_url,
    blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    RegisterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanServeContainerHostMethods(
          &callback, options->scope, script_url,
          base::StringPrintf(
              ServiceWorkerConsts::kServiceWorkerRegisterErrorPrefix,
              options->scope.spec().c_str(), script_url.spec().c_str())
              .c_str(),
          nullptr)) {
    return;
  }

  if (!IsContainerForWindowClient()) {
    mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageFromNonWindow);
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }

  std::vector<GURL> urls = {url_, options->scope, script_url};
  if (!service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          urls)) {
    mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageImproperOrigins);
    // ReportBadMessage() will terminate the renderer process, but Mojo
    // complains if the callback is not run. Just run it with nonsense
    // arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }

  if (!service_worker_security_utils::
          OriginCanRegisterServiceWorkerFromJavascript(url_)) {
    mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageImproperOrigins);
    // ReportBadMessage() will terminate the renderer process, but Mojo
    // complains if the callback is not run. Just run it with nonsense
    // arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }

  int64_t trace_id = base::TimeTicks::Now().since_origin().InMicroseconds();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      "ServiceWorker", "ServiceWorkerContainerHost::Register",
      TRACE_ID_WITH_SCOPE("ServiceWorkerContainerHost::Register", trace_id),
      "Scope", options->scope.spec(), "Script URL", script_url.spec());

  // Wrap the callback with default invoke before passing it, since
  // RegisterServiceWorker() can drop the callback on service worker
  // context core shutdown (i.e., browser session shutdown or
  // DeleteAndStartOver()) and a DCHECK would happen.
  // TODO(crbug.com/1002776): Remove this wrapper and have the Mojo connections
  // drop during shutdown, so the callback can be dropped without crash. Note
  // that we currently would need to add this WrapCallback to *ALL* Mojo
  // callbacks that go through ServiceWorkerContextCore or its members like
  // ServiceWorkerStorage. We're only adding it to Register() now because a
  // browser test fails without it.
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), blink::mojom::ServiceWorkerErrorType::kUnknown,
      std::string(), nullptr);

  // We pass the requesting frame host id, so that we can use this context for
  // things like printing console error if the service worker does not have a
  // process yet. This must be after commit so it should be populated, while
  // it's possible the RenderFrameHost has already been destroyed due to IPC
  // ordering.
  GlobalRenderFrameHostId global_frame_id = GetRenderFrameHostId();
  DCHECK_NE(global_frame_id.child_id, ChildProcessHost::kInvalidUniqueID);
  DCHECK_NE(global_frame_id.frame_routing_id, MSG_ROUTING_NONE);

  // Registrations could come from different origins when "disable-web-security"
  // is active, we need to make sure we get the correct key.
  const blink::StorageKey& key =
      GetCorrectStorageKeyForWebSecurityState(options->scope);

  context_->RegisterServiceWorker(
      script_url, key, *options,
      std::move(outside_fetch_client_settings_object),
      base::BindOnce(&ServiceWorkerContainerHost::RegistrationComplete,
                     weak_factory_.GetWeakPtr(), GURL(script_url),
                     GURL(options->scope), std::move(wrapped_callback),
                     trace_id, mojo::GetBadMessageCallback()),
      global_frame_id, policy_container_policies_.value());
}

void ServiceWorkerContainerHost::GetRegistration(
    const GURL& client_url,
    GetRegistrationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanServeContainerHostMethods(
          &callback, url_, GURL(),
          ServiceWorkerConsts::kServiceWorkerGetRegistrationErrorPrefix,
          nullptr)) {
    return;
  }

  std::string error_message;
  if (!IsValidGetRegistrationMessage(client_url, &error_message)) {
    mojo::ReportBadMessage(error_message);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }

  int64_t trace_id = base::TimeTicks::Now().since_origin().InMicroseconds();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistration",
      TRACE_ID_WITH_SCOPE("ServiceWorkerContainerHost::GetRegistration",
                          trace_id),
      "Client URL", client_url.spec());

  // The client_url may be cross-origin if "disable-web-security" is active,
  // make sure we get the correct key.
  const blink::StorageKey& key =
      GetCorrectStorageKeyForWebSecurityState(client_url);

  context_->registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation, client_url, key,
      base::BindOnce(&ServiceWorkerContainerHost::GetRegistrationComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     trace_id));
}

void ServiceWorkerContainerHost::GetRegistrations(
    GetRegistrationsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanServeContainerHostMethods(
          &callback, url_, GURL(),
          ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix,
          absl::nullopt)) {
    return;
  }

  std::string error_message;
  if (!IsValidGetRegistrationsMessage(&error_message)) {
    mojo::ReportBadMessage(error_message);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), absl::nullopt);
    return;
  }

  int64_t trace_id = base::TimeTicks::Now().since_origin().InMicroseconds();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistrations",
      TRACE_ID_WITH_SCOPE("ServiceWorkerContainerHost::GetRegistrations",
                          trace_id));
  context_->registry()->GetRegistrationsForStorageKey(
      key_, base::BindOnce(
                &ServiceWorkerContainerHost::GetRegistrationsComplete,
                weak_factory_.GetWeakPtr(), std::move(callback), trace_id));
}

void ServiceWorkerContainerHost::GetRegistrationForReady(
    GetRegistrationForReadyCallback callback) {
  std::string error_message;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsValidGetRegistrationForReadyMessage(&error_message)) {
    mojo::ReportBadMessage(error_message);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(nullptr);
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistrationForReady",
      TRACE_ID_LOCAL(this));
  DCHECK(!get_ready_callback_);
  get_ready_callback_ =
      std::make_unique<GetRegistrationForReadyCallback>(std::move(callback));
  ReturnRegistrationForReadyIfNeeded();
}

void ServiceWorkerContainerHost::EnsureControllerServiceWorker(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(kinuko): Log the reasons we drop the request.
  if (!context_ || !controller_)
    return;

  controller_->RunAfterStartWorker(
      PurposeToEventType(purpose),
      base::BindOnce(&ServiceWorkerContainerHost::StartControllerComplete,
                     weak_factory_.GetWeakPtr(), std::move(receiver)));
}

void ServiceWorkerContainerHost::CloneContainerHost(
    mojo::PendingReceiver<blink::mojom::ServiceWorkerContainerHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  additional_receivers_.Add(this, std::move(receiver));
}

void ServiceWorkerContainerHost::HintToUpdateServiceWorker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsContainerForClient()) {
    mojo::ReportBadMessage("SWPH_HTUSW_NOT_CLIENT");
    return;
  }

  // The destructors notify the ServiceWorkerVersions to update.
  versions_to_update_.clear();
}

void ServiceWorkerContainerHost::EnsureFileAccess(
    const std::vector<base::FilePath>& file_paths,
    EnsureFileAccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

      if (!policy->CanReadFile(controller_process_id, file))
        policy->GrantReadFile(controller_process_id, file);
    }
  }

  std::move(callback).Run();
}

void ServiceWorkerContainerHost::OnExecutionReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsContainerForClient()) {
    mojo::ReportBadMessage("SWPH_OER_NOT_CLIENT");
    return;
  }

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
  SendSetControllerServiceWorker(false /* notify_controllerchange */);

  SetExecutionReady();
}

void ServiceWorkerContainerHost::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!get_ready_callback_ || get_ready_callback_->is_null())
    return;
  if (changed_mask->active && registration->active_version()) {
    // Wait until the state change so we don't send the get for ready
    // registration complete message before set version attributes message.
    registration->active_version()->RegisterStatusChangeCallback(base::BindOnce(
        &ServiceWorkerContainerHost::ReturnRegistrationForReadyIfNeeded,
        weak_factory_.GetWeakPtr()));
  }
}

void ServiceWorkerContainerHost::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerContainerHost::OnRegistrationFinishedUninstalling(
    ServiceWorkerRegistration* registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerContainerHost::OnSkippedWaiting(
    ServiceWorkerRegistration* registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (controller_registration_ != registration)
    return;

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

void ServiceWorkerContainerHost::AddMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(blink::ServiceWorkerScopeMatches(registration->scope(),
                                          GetUrlForScopeMatch()));
  DCHECK(registration->key() == key());
  if (!IsEligibleForServiceWorkerController())
    return;
  size_t key = registration->scope().spec().size();
  if (base::Contains(matching_registrations_, key))
    return;
  registration->AddListener(this);
  matching_registrations_[key] = registration;
  ReturnRegistrationForReadyIfNeeded();
}

void ServiceWorkerContainerHost::RemoveMatchingRegistration(
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

ServiceWorkerRegistration* ServiceWorkerContainerHost::MatchRegistration()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& registration : base::Reversed(matching_registrations_)) {
    if (registration.second->is_uninstalled())
      continue;
    if (registration.second->is_uninstalling())
      return nullptr;
    return registration.second.get();
  }
  return nullptr;
}

void ServiceWorkerContainerHost::AddServiceWorkerToUpdate(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This is only called for windows now, but it should be called for all
  // clients someday.
  DCHECK(IsContainerForWindowClient());

  versions_to_update_.emplace(std::move(version));
}

void ServiceWorkerContainerHost::PostMessageToClient(
    ServiceWorkerVersion* version,
    blink::TransferableMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());

  blink::mojom::ServiceWorkerObjectInfoPtr info;
  base::WeakPtr<ServiceWorkerObjectHost> object_host =
      GetOrCreateServiceWorkerObjectHost(version);
  if (object_host)
    info = object_host->CreateCompleteObjectInfoToSend();
  container_->PostMessageToClient(std::move(info), std::move(message));
}

void ServiceWorkerContainerHost::CountFeature(
    blink::mojom::WebFeature feature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // CountFeature is a message about the client's controller. It should be sent
  // only for clients.
  DCHECK(IsContainerForClient());

  // And only when loading finished so the controller is really settled.
  if (!is_execution_ready())
    return;

  // `container_` shouldn't be disconnected during the lifetime of `this` but
  // there seems a situation where `container_` is disconnected.
  // TODO(crbug.com/1136843): Figure out the cause and remove this check.
  if (!container_.is_connected())
    return;

  container_->CountFeature(feature);
}

void ServiceWorkerContainerHost::SendSetControllerServiceWorker(
    bool notify_controllerchange) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());

  auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
  controller_info->client_id = client_uuid();
  // Set |fetch_request_window_id| only when |controller_| is available.
  // Setting |fetch_request_window_id| should not affect correctness, however,
  // we have the extensions bug, https://crbug.com/963748, which we don't yet
  // understand.  That is why we don't set |fetch_request_window_id| if there
  // is no controller, at least, until we can fix the extension bug.
  if (controller_ && fetch_request_window_id_) {
    controller_info->fetch_request_window_id =
        absl::make_optional(fetch_request_window_id_);
  }

  if (!controller_) {
    container_->SetController(std::move(controller_info),
                              notify_controllerchange);
    return;
  }

  DCHECK(controller_registration());
  DCHECK_EQ(controller_registration_->active_version(), controller_.get());

  controller_info->mode = GetControllerMode();
  controller_info->fetch_handler_type = controller()->fetch_handler_type();
  controller_info->effective_fetch_handler_type =
      controller()->EffectiveFetchHandlerType();
  controller_info->fetch_handler_bypass_option =
      controller()->fetch_handler_bypass_option();
  controller_info->sha256_script_checksum =
      controller()->sha256_script_checksum();
  if (controller()->router_evaluator()) {
    controller_info->router_rules = controller()->router_evaluator()->rules();
  }

  // Pass an endpoint for the client to talk to this controller.
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote =
      GetRemoteControllerServiceWorker();
  if (remote.is_bound()) {
    controller_info->remote_controller = remote.Unbind();
  }

  // Set the info for the JavaScript ServiceWorkerContainer#controller object.
  base::WeakPtr<ServiceWorkerObjectHost> object_host =
      GetOrCreateServiceWorkerObjectHost(controller_);
  if (object_host) {
    controller_info->object_info =
        object_host->CreateCompleteObjectInfoToSend();
  }

  // Populate used features for UseCounter purposes.
  for (const blink::mojom::WebFeature feature : controller_->used_features())
    controller_info->used_features.push_back(feature);

  container_->SetController(std::move(controller_info),
                            notify_controllerchange);
}

void ServiceWorkerContainerHost::NotifyControllerLost() {
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

void ServiceWorkerContainerHost::ClaimedByRegistration(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());
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

blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
ServiceWorkerContainerHost::CreateServiceWorkerRegistrationObjectInfo(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t registration_id = registration->id();
  auto existing_host = registration_object_hosts_.find(registration_id);
  if (existing_host != registration_object_hosts_.end()) {
    return existing_host->second->CreateObjectInfo();
  }
  registration_object_hosts_[registration_id] =
      std::make_unique<ServiceWorkerRegistrationObjectHost>(
          context_, this, std::move(registration));
  return registration_object_hosts_[registration_id]->CreateObjectInfo();
}

void ServiceWorkerContainerHost::RemoveServiceWorkerRegistrationObjectHost(
    int64_t registration_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(registration_object_hosts_, registration_id));
  // This is a workaround for a really unfavorable ownership structure of
  // service worker content code. This boils down to the following ownership
  // cycle:
  // 1. This class owns ServiceWorkerRegistrationObjectHost via std::unique_ptr
  //    in registration_object_hosts_.
  // 2. The ServiceWorkerRegistrationObjectHost has a
  //    scoped_refptr<ServiceWorkerRegistration> registration_ member.
  // 3. The ServiceWorkerRegistration has multiple
  //    scoped_refptr<ServiceWorkerVersion> members.
  // 4. ServiceWorkerVersion has a std::unique_ptr<ServiceWorkerHost>
  //    worker_host_ member.
  // 5. ServiceWorkerHost in turn owns an instance of this class via
  //    its worker_host_ member.
  // What this all means is that erasing the registration_id here can actually
  // lead to "this" ending up being destroyed after we exit from the erase
  // call. This might not seem fatal, but is when using libstdc++. Apparently
  // the C++ standard does not define when the destructor of the value from the
  // map gets called. In libcxx its called after the key has been removed from
  // the map, while in libstdc++ the destructor gets called first and then
  // the key is removed before the erase call returns. This means that in
  // case of libstdc++ the value we're removing from the map in the erase call
  // can be deleted a second time when registration_object_hosts_ destructor
  // gets called in ~ServiceWorkerContainerHost.
  auto to_be_deleted = std::move(registration_object_hosts_[registration_id]);
  registration_object_hosts_.erase(registration_id);
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerContainerHost::CreateServiceWorkerObjectInfoToSend(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t version_id = version->version_id();
  auto existing_object_host = service_worker_object_hosts_.find(version_id);
  if (existing_object_host != service_worker_object_hosts_.end()) {
    return existing_object_host->second->CreateCompleteObjectInfoToSend();
  }
  service_worker_object_hosts_[version_id] =
      std::make_unique<ServiceWorkerObjectHost>(context_, GetWeakPtr(),
                                                std::move(version));
  return service_worker_object_hosts_[version_id]
      ->CreateCompleteObjectInfoToSend();
}

base::WeakPtr<ServiceWorkerObjectHost>
ServiceWorkerContainerHost::GetOrCreateServiceWorkerObjectHost(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context_ || !version)
    return nullptr;

  const int64_t version_id = version->version_id();
  auto existing_object_host = service_worker_object_hosts_.find(version_id);
  if (existing_object_host != service_worker_object_hosts_.end())
    return existing_object_host->second->AsWeakPtr();

  service_worker_object_hosts_[version_id] =
      std::make_unique<ServiceWorkerObjectHost>(context_, GetWeakPtr(),
                                                std::move(version));
  return service_worker_object_hosts_[version_id]->AsWeakPtr();
}

void ServiceWorkerContainerHost::RemoveServiceWorkerObjectHost(
    int64_t version_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(service_worker_object_hosts_, version_id));

  // ServiceWorkerObjectHost to be deleted may have the last reference to
  // ServiceWorkerVersion that indirectly owns this ServiceWorkerContainerHost.
  // If we erase the object host directly from the map, |this| could be deleted
  // during the map operation and may crash. To avoid the case, we take the
  // ownership of the object host from the map first, and then erase the entry
  // from the map. See https://crbug.com/1056598 for details.
  std::unique_ptr<ServiceWorkerObjectHost> to_be_deleted =
      std::move(service_worker_object_hosts_[version_id]);
  DCHECK(to_be_deleted);
  service_worker_object_hosts_.erase(version_id);
}

bool ServiceWorkerContainerHost::IsContainerForServiceWorker() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return client_info_ == absl::nullopt;
}

bool ServiceWorkerContainerHost::IsContainerForClient() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return client_info_ != absl::nullopt;
}

blink::mojom::ServiceWorkerClientType
ServiceWorkerContainerHost::GetClientType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_info_);
  return client_info_->type();
}

bool ServiceWorkerContainerHost::IsContainerForWindowClient() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return client_info_ &&
         client_info_->type() == blink::mojom::ServiceWorkerClientType::kWindow;
}

bool ServiceWorkerContainerHost::IsContainerForWorkerClient() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  using blink::mojom::ServiceWorkerClientType;
  if (!client_info_)
    return false;

  return client_info_->type() == ServiceWorkerClientType::kDedicatedWorker ||
         client_info_->type() == ServiceWorkerClientType::kSharedWorker;
}

ServiceWorkerClientInfo ServiceWorkerContainerHost::GetServiceWorkerClientInfo()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());

  return *client_info_;
}

void ServiceWorkerContainerHost::OnBeginNavigationCommit(
    const GlobalRenderFrameHostId& rfh_id,
    const PolicyContainerPolicies& policy_container_policies,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    ukm::SourceId document_ukm_source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForWindowClient());

  ongoing_navigation_frame_tree_node_id_ = RenderFrameHost::kNoFrameTreeNodeId;
  client_info_->SetRenderFrameHostId(rfh_id);

  if (controller_)
    controller_->UpdateForegroundPriority();

  DCHECK(!policy_container_policies_.has_value());
  policy_container_policies_ = policy_container_policies.Clone();

  coep_reporter_.Bind(std::move(coep_reporter));

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_to_be_passed;
  coep_reporter_->Clone(
      coep_reporter_to_be_passed.InitWithNewPipeAndPassReceiver());

  if (controller_ && controller_->fetch_handler_existence() ==
                         ServiceWorkerVersion::FetchHandlerExistence::EXISTS) {
    DCHECK(pending_controller_receiver_);
    controller_->controller()->Clone(
        std::move(pending_controller_receiver_),
        policy_container_policies_->cross_origin_embedder_policy,
        std::move(coep_reporter_to_be_passed));
  }

  auto* rfh = RenderFrameHostImpl::FromID(rfh_id);
  // `rfh` may be null in tests (but it should not happen in production).
  if (rfh)
    rfh->AddServiceWorkerContainerHost(client_uuid(), GetWeakPtr());

  DCHECK_EQ(ukm_source_id_, ukm::kInvalidSourceId);
  ukm_source_id_ = document_ukm_source_id;

  TransitionToClientPhase(ClientPhase::kResponseCommitted);
}

void ServiceWorkerContainerHost::OnEndNavigationCommit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForWindowClient());

  DCHECK(!navigation_commit_ended_);
  navigation_commit_ended_ = true;

  if (controller_) {
    controller_->OnControlleeNavigationCommitted(client_uuid_,
                                                 GetRenderFrameHostId());
  }
}

void ServiceWorkerContainerHost::CompleteWebWorkerPreparation(
    const PolicyContainerPolicies& policy_container_policies,
    ukm::SourceId worker_ukm_source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForWorkerClient());

  DCHECK(!policy_container_policies_);
  policy_container_policies_ = policy_container_policies.Clone();
  if (controller_ && controller_->fetch_handler_existence() ==
                         ServiceWorkerVersion::FetchHandlerExistence::EXISTS) {
    DCHECK(pending_controller_receiver_);
    // TODO(https://crbug.com/999049): Plumb the COEP reporter.
    controller_->controller()->Clone(
        std::move(pending_controller_receiver_),
        policy_container_policies_->cross_origin_embedder_policy,
        mojo::NullRemote());
  }

  DCHECK_EQ(ukm_source_id_, ukm::kInvalidSourceId);
  ukm_source_id_ = worker_ukm_source_id;

  TransitionToClientPhase(ClientPhase::kResponseCommitted);
  SetExecutionReady();
}

void ServiceWorkerContainerHost::UpdateUrls(
    const GURL& url,
    const absl::optional<url::Origin>& top_frame_origin,
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GURL previous_url = url_;

  DCHECK(!url.has_ref());
  url_ = url;
  top_frame_origin_ = top_frame_origin;
  key_ = storage_key;

#if DCHECK_IS_ON()
  const url::Origin origin_to_dcheck =
      IsContainerForClient() ? url::Origin::Create(GetUrlForScopeMatch())
                             : url::Origin::Create(url);
  DCHECK((origin_to_dcheck.opaque() && key_.origin().opaque()) ||
         origin_to_dcheck.IsSameOriginWith(key_.origin()))
      << origin_to_dcheck << " and " << key_.origin() << " should be equal.";
  // TODO(crbug.com/1402965): verify that `top_frame_origin` matches the
  // `top_level_site` of `storage_key`, in most cases.
  //
  // This is currently not the case if:
  //  - The storage key is not for the "real" top-level site, such as when the
  //    top-level site is actually an extension.
  //  - The storage key has a nonce, in which case its `top_level_site` will be
  //    for the frame that introduced the nonce (such as a fenced frame) and not
  //    the same as `top_level_site`.
  //  - The storage key was generated without third-party storage partitioning.
  //    This may be the case even when 3PSP is enabled, due to enterprise policy
  //    or deprecation trials.
  //
  // Consider adding a DHCECK here once the last of those conditions is
  // resolved. See
  // https://chromium-review.googlesource.com/c/chromium/src/+/4378900/4.
#endif

  // The remaining parts of this function don't make sense for service worker
  // execution contexts. Return early.
  if (IsContainerForServiceWorker())
    return;

  if (previous_url != url) {
    // Revoke the token on URL change since any service worker holding the token
    // may no longer be the potential controller of this frame and shouldn't
    // have the power to display SSL dialogs for it.
    if (IsContainerForWindowClient())
      fetch_request_window_id_ = base::UnguessableToken::Create();
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
    if (context_)
      context_->UpdateContainerHostClientID(previous_client_uuid, client_uuid_);
  }

  SyncMatchingRegistrations();
}

void ServiceWorkerContainerHost::SetControllerRegistration(
    scoped_refptr<ServiceWorkerRegistration> controller_registration,
    bool notify_controllerchange) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());

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

mojo::Remote<blink::mojom::ControllerServiceWorker>
ServiceWorkerContainerHost::GetRemoteControllerServiceWorker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());

  DCHECK(controller_);
  if (controller_->fetch_handler_existence() ==
      ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST) {
    return mojo::Remote<blink::mojom::ControllerServiceWorker>();
  }

  mojo::Remote<blink::mojom::ControllerServiceWorker> remote_controller;
  if (!is_response_committed()) {
    // The receiver will be connected to the controller in
    // OnBeginNavigationCommit() or CompleteWebWorkerPreparation(). The pair of
    // Mojo endpoints is created on each main resource response including
    // redirect. The final Mojo endpoint which is corresponding to the OK
    // response will be sent to the service worker.
    pending_controller_receiver_ =
        remote_controller.BindNewPipeAndPassReceiver();
  } else {
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter_to_be_passed;
    if (coep_reporter_) {
      DCHECK(IsContainerForWindowClient());
      coep_reporter_->Clone(
          coep_reporter_to_be_passed.InitWithNewPipeAndPassReceiver());
    } else {
      // TODO(https://crbug.com/999049): Implement DedicatedWorker and
      // SharedWorker cases.
      DCHECK(IsContainerForWorkerClient());
    }

    controller_->controller()->Clone(
        remote_controller.BindNewPipeAndPassReceiver(),
        policy_container_policies_->cross_origin_embedder_policy,
        std::move(coep_reporter_to_be_passed));
  }
  return remote_controller;
}

namespace {

}  // namespace

bool ServiceWorkerContainerHost::AllowServiceWorker(const GURL& scope,
                                                    const GURL& script_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(context_);
  auto* browser_context = context_->wrapper()->browser_context();
  // Check that the browser context is not nullptr.  It becomes nullptr
  // when the service worker process manager is being shutdown.
  if (!browser_context) {
    return false;
  }
  AllowServiceWorkerResult allowed =
      GetContentClient()->browser()->AllowServiceWorker(
          scope, site_for_cookies(), top_frame_origin(), script_url,
          browser_context);
  if (IsContainerForWindowClient()) {
    auto* rfh = RenderFrameHostImpl::FromID(GetRenderFrameHostId());
    auto* web_contents =
        static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(rfh));
    if (web_contents)
      web_contents->OnServiceWorkerAccessed(rfh, scope, allowed);
  }
  return allowed;
}

bool ServiceWorkerContainerHost::IsEligibleForServiceWorkerController() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());

  if (!url_.is_valid())
    return false;
  // Pass GetUrlForScopeMatch() instead of `url_` because we cannot take the
  // origin of `url_` when it's a blob URL (see https://crbug.com/1144717). It's
  // guaranteed that the URL returned by GetURLForScopeMatch() has the same
  // logical origin as `url_`.
  // TODO(asamidoi): Add url::Origin member for ServiceWorkerContainerHost and
  // use it as the argument of OriginCanAccessServiceWorkers().
  if (!OriginCanAccessServiceWorkers(GetUrlForScopeMatch()))
    return false;

  if (is_parent_frame_secure_)
    return true;

  std::set<std::string> schemes;
  GetContentClient()->browser()->GetSchemesBypassingSecureContextCheckAllowlist(
      &schemes);
  return schemes.find(url_.scheme()) != schemes.end();
}

bool ServiceWorkerContainerHost::is_response_committed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());
  switch (client_phase_) {
    case ClientPhase::kInitial:
      return false;
    case ClientPhase::kResponseCommitted:
    case ClientPhase::kExecutionReady:
      return true;
  }
  NOTREACHED();
  return false;
}

void ServiceWorkerContainerHost::AddExecutionReadyCallback(
    ExecutionReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());

  DCHECK(!is_execution_ready());
  execution_ready_callbacks_.push_back(std::move(callback));
}

bool ServiceWorkerContainerHost::is_execution_ready() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());

  return client_phase_ == ClientPhase::kExecutionReady;
}

GlobalRenderFrameHostId ServiceWorkerContainerHost::GetRenderFrameHostId()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForWindowClient());
  return client_info_->GetRenderFrameHostId();
}

int ServiceWorkerContainerHost::GetProcessId() const {
  if (IsContainerForWindowClient()) {
    return GetRenderFrameHostId().child_id;
  }
  DCHECK(IsContainerForWorkerClient());
  return process_id_for_worker_client_;
}

NavigationRequest*
ServiceWorkerContainerHost::GetOngoingNavigationRequestBeforeCommit(
    base::PassKey<StoragePartitionImpl>) const {
  DCHECK(IsContainerForWindowClient());
  DCHECK_NE(ongoing_navigation_frame_tree_node_id_,
            RenderFrameHost::kNoFrameTreeNodeId);
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

const std::string& ServiceWorkerContainerHost::client_uuid() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());
  return client_uuid_;
}

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerContainerHost::GetControllerMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());
  if (!controller_)
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  switch (controller_->fetch_handler_existence()) {
    case ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST:
      return blink::mojom::ControllerServiceWorkerMode::kNoFetchEventHandler;
    case ServiceWorkerVersion::FetchHandlerExistence::EXISTS:
      return blink::mojom::ControllerServiceWorkerMode::kControlled;
    case ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN:
      // UNKNOWN means the controller is still installing. It's not possible to
      // have a controller that hasn't finished installing.
      NOTREACHED();
  }
  NOTREACHED();
  return blink::mojom::ControllerServiceWorkerMode::kNoController;
}

ServiceWorkerVersion* ServiceWorkerContainerHost::controller() const {
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CheckControllerConsistency(false);
#endif  // DCHECK_IS_ON()
  return controller_.get();
}

ServiceWorkerRegistration* ServiceWorkerContainerHost::controller_registration()
    const {
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CheckControllerConsistency(false);
#endif  // DCHECK_IS_ON()
  return controller_registration_.get();
}

void ServiceWorkerContainerHost::set_service_worker_host(
    ServiceWorkerHost* service_worker_host) {
  DCHECK(IsContainerForServiceWorker());
  DCHECK(!service_worker_host_);
  DCHECK(service_worker_host);
  service_worker_host_ = service_worker_host;
}

ServiceWorkerHost* ServiceWorkerContainerHost::service_worker_host() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForServiceWorker());
  return service_worker_host_;
}

bool ServiceWorkerContainerHost::IsInBackForwardCache() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_in_back_forward_cache_;
}

void ServiceWorkerContainerHost::EvictFromBackForwardCache(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(IsContainerForClient());
  is_in_back_forward_cache_ = false;

  if (!IsContainerForWindowClient())
    return;

  auto* rfh = RenderFrameHostImpl::FromID(GetRenderFrameHostId());
  // |rfh| could be evicted before this function is called.
  if (rfh && rfh->IsInBackForwardCache())
    rfh->EvictFromBackForwardCacheWithReason(reason);
}

void ServiceWorkerContainerHost::OnEnterBackForwardCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(IsContainerForClient());
  if (controller_)
    controller_->MoveControlleeToBackForwardCacheMap(client_uuid());
  is_in_back_forward_cache_ = true;
}

void ServiceWorkerContainerHost::OnRestoreFromBackForwardCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(IsContainerForClient());
  if (controller_)
    controller_->RestoreControlleeFromBackForwardCacheMap(client_uuid());
  is_in_back_forward_cache_ = false;
}

base::WeakPtr<ServiceWorkerContainerHost>
ServiceWorkerContainerHost::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ServiceWorkerContainerHost::SyncMatchingRegistrations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!controller_registration_);

  RemoveAllMatchingRegistrations();
  if (!context_)
    return;
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
bool ServiceWorkerContainerHost::IsMatchingRegistration(
    ServiceWorkerRegistration* registration) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string spec = registration->scope().spec();
  size_t key = spec.size();

  auto iter = matching_registrations_.find(key);
  if (iter == matching_registrations_.end())
    return false;
  if (iter->second.get() != registration)
    return false;
  return true;
}
#endif  // DCHECK_IS_ON()

void ServiceWorkerContainerHost::RemoveAllMatchingRegistrations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!controller_registration_);
  for (const auto& it : matching_registrations_) {
    ServiceWorkerRegistration* registration = it.second.get();
    registration->RemoveListener(this);
  }
  matching_registrations_.clear();
}

void ServiceWorkerContainerHost::ReturnRegistrationForReadyIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!get_ready_callback_ || get_ready_callback_->is_null())
    return;
  ServiceWorkerRegistration* registration = MatchRegistration();
  if (!registration || !registration->active_version())
    return;
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistrationForReady",
      TRACE_ID_LOCAL(this), "Registration ID", registration->id());
  if (!context_) {
    // Here no need to run or destroy |get_ready_callback_|, which will destroy
    // together with |receiver_| when |this| destroys.
    return;
  }

  std::move(*get_ready_callback_)
      .Run(CreateServiceWorkerRegistrationObjectInfo(
          scoped_refptr<ServiceWorkerRegistration>(registration)));
}

void ServiceWorkerContainerHost::SetExecutionReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_execution_ready());
  TransitionToClientPhase(ClientPhase::kExecutionReady);
  RunExecutionReadyCallbacks();

  if (context_)
    context_->NotifyClientIsExecutionReady(*this);
}

void ServiceWorkerContainerHost::RunExecutionReadyCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());

  std::vector<ExecutionReadyCallback> callbacks;
  execution_ready_callbacks_.swap(callbacks);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RunCallbacks, std::move(callbacks)));
}

void ServiceWorkerContainerHost::TransitionToClientPhase(
    ClientPhase new_phase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_phase_ == new_phase)
    return;
  switch (client_phase_) {
    case ClientPhase::kInitial:
      DCHECK_EQ(new_phase, ClientPhase::kResponseCommitted);
      break;
    case ClientPhase::kResponseCommitted:
      DCHECK_EQ(new_phase, ClientPhase::kExecutionReady);
      break;
    case ClientPhase::kExecutionReady:
      NOTREACHED();
      break;
  }
  client_phase_ = new_phase;
}

void ServiceWorkerContainerHost::UpdateController(
    bool notify_controllerchange) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ServiceWorkerVersion* version =
      controller_registration_ ? controller_registration_->active_version()
                               : nullptr;
  CHECK(!version || IsEligibleForServiceWorkerController());
  if (version == controller_.get())
    return;

  scoped_refptr<ServiceWorkerVersion> previous_version = controller_;
  controller_ = version;
  if (version) {
    version->AddControllee(this);
    if (IsBackForwardCacheEnabled() && IsInBackForwardCache()) {
      // |this| was not |version|'s controllee when |OnEnterBackForwardCache|
      // was called.
      version->MoveControlleeToBackForwardCacheMap(client_uuid());
    }
  }
  if (previous_version)
    previous_version->Uncontrol(client_uuid());

  // SetController message should be sent only for clients.
  DCHECK(IsContainerForClient());

  if (!is_execution_ready())
    return;

  SendSetControllerServiceWorker(notify_controllerchange);
}

#if DCHECK_IS_ON()
void ServiceWorkerContainerHost::CheckControllerConsistency(
    bool should_crash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!controller_) {
    DCHECK(!controller_registration_);
    return;
  }

  DCHECK(IsContainerForClient());
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

void ServiceWorkerContainerHost::StartControllerComplete(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());

  if (status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(is_response_committed());

    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter_to_be_passed;
    if (coep_reporter_) {
      DCHECK(IsContainerForWindowClient());
      coep_reporter_->Clone(
          coep_reporter_to_be_passed.InitWithNewPipeAndPassReceiver());
    } else {
      // TODO(https://crbug.com/999049): Implement DedicatedWorker and
      // SharedWorker cases.
      DCHECK(IsContainerForWorkerClient());
    }

    controller_->controller()->Clone(
        std::move(receiver),
        policy_container_policies_->cross_origin_embedder_policy,
        std::move(coep_reporter_to_be_passed));
  }
}

void ServiceWorkerContainerHost::RegistrationComplete(
    const GURL& script_url,
    const GURL& scope,
    RegisterCallback callback,
    int64_t trace_id,
    mojo::ReportBadMessageCallback bad_message_callback,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "ServiceWorker", "ServiceWorkerContainerHost::Register",
      TRACE_ID_WITH_SCOPE("ServiceWorkerContainerHost::Register", trace_id),
      "Status", blink::ServiceWorkerStatusToString(status), "Registration ID",
      registration_id);

  // kErrorInvalidArguments means the renderer gave unexpectedly bad arguments,
  // so terminate it.
  if (status == blink::ServiceWorkerStatusCode::kErrorInvalidArguments) {
    std::move(bad_message_callback).Run(status_message);
    // |bad_message_callback| will kill the renderer process, but Mojo complains
    // if the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }

  if (!context_) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        base::StringPrintf(
            ServiceWorkerConsts::kServiceWorkerRegisterErrorPrefix,
            scope.spec().c_str(), script_url.spec().c_str()) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        nullptr);
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, status_message,
                                             &error_type, &error_message);
    std::move(callback).Run(
        error_type,
        base::StringPrintf(
            ServiceWorkerConsts::kServiceWorkerRegisterErrorPrefix,
            scope.spec().c_str(), script_url.spec().c_str()) +
            error_message,
        nullptr);
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  // ServiceWorkerRegisterJob calls its completion callback, which results in
  // this function being called, while the registration is live.
  DCHECK(registration);

  std::move(callback).Run(
      blink::mojom::ServiceWorkerErrorType::kNone, absl::nullopt,
      CreateServiceWorkerRegistrationObjectInfo(
          scoped_refptr<ServiceWorkerRegistration>(registration)));
}

void ServiceWorkerContainerHost::GetRegistrationComplete(
    GetRegistrationCallback callback,
    int64_t trace_id,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistration",
      TRACE_ID_WITH_SCOPE("ServiceWorkerContainerHost::GetRegistration",
                          trace_id),
      "Status", blink::ServiceWorkerStatusToString(status), "Registration ID",
      registration ? registration->id()
                   : blink::mojom::kInvalidServiceWorkerRegistrationId);

  if (!context_) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        std::string(
            ServiceWorkerConsts::kServiceWorkerGetRegistrationErrorPrefix) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        nullptr);
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk &&
      status != blink::ServiceWorkerStatusCode::kErrorNotFound) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, std::string(), &error_type,
                                             &error_message);
    std::move(callback).Run(
        error_type,
        ServiceWorkerConsts::kServiceWorkerGetRegistrationErrorPrefix +
            error_message,
        nullptr);
    return;
  }

  DCHECK(status != blink::ServiceWorkerStatusCode::kOk || registration);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
  if (status == blink::ServiceWorkerStatusCode::kOk &&
      !registration->is_uninstalling()) {
    info = CreateServiceWorkerRegistrationObjectInfo(std::move(registration));
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          absl::nullopt, std::move(info));
}

void ServiceWorkerContainerHost::GetRegistrationsComplete(
    GetRegistrationsCallback callback,
    int64_t trace_id,
    blink::ServiceWorkerStatusCode status,
    const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
        registrations) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistrations",
      TRACE_ID_WITH_SCOPE("ServiceWorkerContainerHost::GetRegistrations",
                          trace_id),
      "Status", blink::ServiceWorkerStatusToString(status));

  if (!context_) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        std::string(
            ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        absl::nullopt);
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, std::string(), &error_type,
                                             &error_message);
    std::move(callback).Run(
        error_type,
        ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix +
            error_message,
        absl::nullopt);
    return;
  }

  std::vector<blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>
      object_infos;

  for (const auto& registration : registrations) {
    DCHECK(registration.get());
    if (!registration->is_uninstalling()) {
      object_infos.push_back(
          CreateServiceWorkerRegistrationObjectInfo(std::move(registration)));
    }
  }

  // Sort by Insertion order. Detail discussion can be found in:
  // https://github.com/w3c/ServiceWorker/issues/1465
  std::sort(
      object_infos.begin(), object_infos.end(),
      [](const blink::mojom::ServiceWorkerRegistrationObjectInfoPtr& ptr1,
         const blink::mojom::ServiceWorkerRegistrationObjectInfoPtr& ptr2) {
        return ptr1->registration_id < ptr2->registration_id;
      });

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          absl::nullopt, std::move(object_infos));
}

bool ServiceWorkerContainerHost::IsValidGetRegistrationMessage(
    const GURL& client_url,
    std::string* out_error) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsContainerForWindowClient()) {
    *out_error = ServiceWorkerConsts::kBadMessageFromNonWindow;
    return false;
  }
  if (!client_url.is_valid()) {
    *out_error = ServiceWorkerConsts::kBadMessageInvalidURL;
    return false;
  }
  std::vector<GURL> urls = {url_, client_url};
  if (!service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          urls)) {
    *out_error = ServiceWorkerConsts::kBadMessageImproperOrigins;
    return false;
  }

  return true;
}

bool ServiceWorkerContainerHost::IsValidGetRegistrationsMessage(
    std::string* out_error) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsContainerForWindowClient()) {
    *out_error = ServiceWorkerConsts::kBadMessageFromNonWindow;
    return false;
  }
  if (!OriginCanAccessServiceWorkers(url_)) {
    *out_error = ServiceWorkerConsts::kBadMessageImproperOrigins;
    return false;
  }

  return true;
}

bool ServiceWorkerContainerHost::IsValidGetRegistrationForReadyMessage(
    std::string* out_error) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsContainerForWindowClient()) {
    *out_error = ServiceWorkerConsts::kBadMessageFromNonWindow;
    return false;
  }

  if (get_ready_callback_) {
    *out_error =
        ServiceWorkerConsts::kBadMessageGetRegistrationForReadyDuplicated;
    return false;
  }

  return true;
}

template <typename CallbackType, typename... Args>
bool ServiceWorkerContainerHost::CanServeContainerHostMethods(
    CallbackType* callback,
    const GURL& scope,
    const GURL& script_url,
    const char* error_prefix,
    Args... args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context_) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        std::string(error_prefix) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        args...);
    return false;
  }

  // TODO(falken): This check can be removed once crbug.com/439697 is fixed.
  // (Also see crbug.com/776408)
  if (url_.is_empty()) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kSecurity,
        std::string(error_prefix) +
            std::string(ServiceWorkerConsts::kNoDocumentURLErrorMessage),
        args...);
    return false;
  }

  if (!AllowServiceWorker(scope, script_url)) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kDisabled,
        std::string(error_prefix) +
            std::string(ServiceWorkerConsts::kUserDeniedPermissionMessage),
        args...);
    return false;
  }

  return true;
}

blink::StorageKey
ServiceWorkerContainerHost::GetCorrectStorageKeyForWebSecurityState(
    const GURL& url) const {
  if (service_worker_security_utils::IsWebSecurityDisabled()) {
    url::Origin other_origin = url::Origin::Create(url);

    if (key_.origin() != other_origin)
      return blink::StorageKey::CreateFirstParty(other_origin);
  }

  return key_;
}

const GURL& ServiceWorkerContainerHost::GetUrlForScopeMatch() const {
  DCHECK(IsContainerForClient());
  if (!scope_match_url_for_blob_client_.is_empty())
    return scope_match_url_for_blob_client_;
  return url_;
}

void ServiceWorkerContainerHost::InheritControllerFrom(
    ServiceWorkerContainerHost& creator_host,
    const GURL& blob_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsContainerForClient());
  DCHECK_EQ(blink::mojom::ServiceWorkerClientType::kDedicatedWorker,
            GetClientType());
  DCHECK(blob_url.SchemeIsBlob());

  UpdateUrls(blob_url, creator_host.top_frame_origin(), creator_host.key());

  // Let `scope_match_url_for_blob_client_` be the creator's url for scope match
  // because a client should be handled by the service worker of its creator.
  scope_match_url_for_blob_client_ = creator_host.GetUrlForScopeMatch();

  // Inherit the controller of the creator.
  if (creator_host.controller_registration()) {
    AddMatchingRegistration(creator_host.controller_registration());
    SetControllerRegistration(creator_host.controller_registration(),
                              false /* notify_controllerchange */);
  }
}

}  // namespace content
