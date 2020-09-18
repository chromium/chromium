// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_container_host.h"

#include <set>
#include <utility>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/web_contents/frame_tree_node_id_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"

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
  DISALLOW_COPY_AND_ASSIGN(PendingUpdateVersion);
};

ServiceWorkerContainerHost::ServiceWorkerContainerHost(
    base::WeakPtr<ServiceWorkerContextCore> context)
    : context_(std::move(context)), create_time_(base::TimeTicks::Now()) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
      client_uuid_(base::GenerateGUID()),
      is_parent_frame_secure_(is_parent_frame_secure),
      container_(std::move(container_remote)),
      client_info_(frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
      client_uuid_(base::GenerateGUID()),
      process_id_(process_id),
      container_(std::move(container_remote)),
      client_info_(client_info) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForWorkerClient());
  DCHECK(context_);
  DCHECK_NE(process_id_, ChildProcessHost::kInvalidUniqueID);
  DCHECK(container_.is_bound());
}

ServiceWorkerContainerHost::~ServiceWorkerContainerHost() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (IsBackForwardCacheEnabled() && IsContainerForClient()) {
    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
        base::BindOnce(
            [](int process_id, int frame_id, const std::string& uuid) {
              auto* rfh = RenderFrameHostImpl::FromID(process_id, frame_id);
              if (rfh)
                rfh->RemoveServiceWorkerContainerHost(uuid);
            },
            process_id(), frame_id(), client_uuid()));
  }

  if (fetch_request_window_id_)
    FrameTreeNodeIdRegistry::GetInstance()->Remove(fetch_request_window_id_);

  if (IsContainerForClient() && controller_)
    controller_->OnControlleeDestroyed(client_uuid());

  // Remove |this| as an observer of ServiceWorkerRegistrations.
  // TODO(falken): Use ScopedObserver instead of this explicit call.
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

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
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageImproperOrigins);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }

  int64_t trace_id = base::TimeTicks::Now().since_origin().InMicroseconds();
  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "ServiceWorkerContainerHost::Register", trace_id,
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
  context_->RegisterServiceWorker(
      script_url, *options, std::move(outside_fetch_client_settings_object),
      base::BindOnce(&ServiceWorkerContainerHost::RegistrationComplete,
                     weak_factory_.GetWeakPtr(), GURL(script_url),
                     GURL(options->scope), std::move(wrapped_callback),
                     trace_id, mojo::GetBadMessageCallback()));
}

void ServiceWorkerContainerHost::GetRegistration(
    const GURL& client_url,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

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
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerContainerHost::GetRegistration",
                           trace_id, "Client URL", client_url.spec());
  context_->registry()->FindRegistrationForClientUrl(
      client_url,
      base::AdaptCallbackForRepeating(base::BindOnce(
          &ServiceWorkerContainerHost::GetRegistrationComplete,
          weak_factory_.GetWeakPtr(), std::move(callback), trace_id)));
}

void ServiceWorkerContainerHost::GetRegistrations(
    GetRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (!CanServeContainerHostMethods(
          &callback, url_, GURL(),
          ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix,
          base::nullopt)) {
    return;
  }

  std::string error_message;
  if (!IsValidGetRegistrationsMessage(&error_message)) {
    mojo::ReportBadMessage(error_message);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), base::nullopt);
    return;
  }

  int64_t trace_id = base::TimeTicks::Now().since_origin().InMicroseconds();
  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "ServiceWorkerContainerHost::GetRegistrations",
                           trace_id);
  context_->registry()->GetRegistrationsForOrigin(
      url::Origin::Create(url_),
      base::AdaptCallbackForRepeating(base::BindOnce(
          &ServiceWorkerContainerHost::GetRegistrationsComplete,
          weak_factory_.GetWeakPtr(), std::move(callback), trace_id)));
}

void ServiceWorkerContainerHost::GetRegistrationForReady(
    GetRegistrationForReadyCallback callback) {
  std::string error_message;
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (!IsValidGetRegistrationForReadyMessage(&error_message)) {
    mojo::ReportBadMessage(error_message);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(nullptr);
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistrationForReady",
      this);
  DCHECK(!get_ready_callback_);
  get_ready_callback_ =
      std::make_unique<GetRegistrationForReadyCallback>(std::move(callback));
  ReturnRegistrationForReadyIfNeeded();
}

void ServiceWorkerContainerHost::EnsureControllerServiceWorker(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  additional_receivers_.Add(this, std::move(receiver));
}

void ServiceWorkerContainerHost::HintToUpdateServiceWorker() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
      if (!policy->CanReadFile(process_id_, file))
        mojo::ReportBadMessage(
            "The renderer doesn't have access to the file "
            "but it tried to grant access to the controller.");

      if (!policy->CanReadFile(controller_process_id, file))
        policy->GrantReadFile(controller_process_id, file);
    }
  }

  std::move(callback).Run();
}

void ServiceWorkerContainerHost::OnExecutionReady() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerContainerHost::OnRegistrationFinishedUninstalling(
    ServiceWorkerRegistration* registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerContainerHost::OnSkippedWaiting(
    ServiceWorkerRegistration* registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(blink::ServiceWorkerScopeMatches(registration->scope(), url_));
  if (!IsContextSecureForServiceWorker())
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  auto it = matching_registrations_.rbegin();
  for (; it != matching_registrations_.rend(); ++it) {
    if (it->second->is_uninstalled())
      continue;
    if (it->second->is_uninstalling())
      return nullptr;
    return it->second.get();
  }
  return nullptr;
}

void ServiceWorkerContainerHost::AddServiceWorkerToUpdate(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // This is only called for windows now, but it should be called for all
  // clients someday.
  DCHECK(IsContainerForWindowClient());

  versions_to_update_.emplace(std::move(version));
}

void ServiceWorkerContainerHost::PostMessageToClient(
    ServiceWorkerVersion* version,
    blink::TransferableMessage message) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // CountFeature is a message about the client's controller. It should be sent
  // only for clients.
  DCHECK(IsContainerForClient());

  // And only when loading finished so the controller is really settled.
  if (!is_execution_ready())
    return;

  container_->CountFeature(feature);
}

void ServiceWorkerContainerHost::SendSetControllerServiceWorker(
    bool notify_controllerchange) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
        base::make_optional(fetch_request_window_id_);
  }

  if (!controller_) {
    container_->SetController(std::move(controller_info),
                              notify_controllerchange);
    return;
  }

  DCHECK(controller_registration());
  DCHECK_EQ(controller_registration_->active_version(), controller_.get());

  controller_info->mode = GetControllerMode();

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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  SetControllerRegistration(nullptr, true /* notify_controllerchange */);
}

void ServiceWorkerContainerHost::ClaimedByRegistration(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(base::Contains(registration_object_hosts_, registration_id));
  registration_object_hosts_.erase(registration_id);
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerContainerHost::CreateServiceWorkerObjectInfoToSend(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  int64_t version_id = version->version_id();
  auto existing_object_host = service_worker_object_hosts_.find(version_id);
  if (existing_object_host != service_worker_object_hosts_.end()) {
    return existing_object_host->second->CreateCompleteObjectInfoToSend();
  }
  service_worker_object_hosts_[version_id] =
      std::make_unique<ServiceWorkerObjectHost>(context_, this,
                                                std::move(version));
  return service_worker_object_hosts_[version_id]
      ->CreateCompleteObjectInfoToSend();
}

base::WeakPtr<ServiceWorkerObjectHost>
ServiceWorkerContainerHost::GetOrCreateServiceWorkerObjectHost(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (!context_ || !version)
    return nullptr;

  const int64_t version_id = version->version_id();
  auto existing_object_host = service_worker_object_hosts_.find(version_id);
  if (existing_object_host != service_worker_object_hosts_.end())
    return existing_object_host->second->AsWeakPtr();

  service_worker_object_hosts_[version_id] =
      std::make_unique<ServiceWorkerObjectHost>(context_, this,
                                                std::move(version));
  return service_worker_object_hosts_[version_id]->AsWeakPtr();
}

void ServiceWorkerContainerHost::RemoveServiceWorkerObjectHost(
    int64_t version_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  return client_info_ == base::nullopt;
}

bool ServiceWorkerContainerHost::IsContainerForClient() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  return client_info_ != base::nullopt;
}

blink::mojom::ServiceWorkerClientType
ServiceWorkerContainerHost::GetClientType() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(client_info_);
  return client_info_->type();
}

bool ServiceWorkerContainerHost::IsContainerForWindowClient() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  return client_info_ &&
         client_info_->type() == blink::mojom::ServiceWorkerClientType::kWindow;
}

bool ServiceWorkerContainerHost::IsContainerForWorkerClient() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  using blink::mojom::ServiceWorkerClientType;
  if (!client_info_)
    return false;

  return client_info_->type() == ServiceWorkerClientType::kDedicatedWorker ||
         client_info_->type() == ServiceWorkerClientType::kSharedWorker;
}

ServiceWorkerClientInfo ServiceWorkerContainerHost::GetServiceWorkerClientInfo()
    const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForClient());

  return *client_info_;
}

void ServiceWorkerContainerHost::OnBeginNavigationCommit(
    int container_process_id,
    int container_frame_id,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForWindowClient());

  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, process_id_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, container_process_id);
  process_id_ = container_process_id;
  if (controller_)
    controller_->UpdateForegroundPriority();

  DCHECK_EQ(MSG_ROUTING_NONE, frame_id_);
  DCHECK_NE(MSG_ROUTING_NONE, container_frame_id);
  frame_id_ = container_frame_id;

  DCHECK(!cross_origin_embedder_policy_.has_value());
  cross_origin_embedder_policy_ = cross_origin_embedder_policy;
  coep_reporter_.Bind(std::move(coep_reporter));

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_to_be_passed;
  coep_reporter_->Clone(
      coep_reporter_to_be_passed.InitWithNewPipeAndPassReceiver());

  if (controller_ && controller_->fetch_handler_existence() ==
                         ServiceWorkerVersion::FetchHandlerExistence::EXISTS) {
    DCHECK(pending_controller_receiver_);
    controller_->controller()->Clone(std::move(pending_controller_receiver_),
                                     cross_origin_embedder_policy_.value(),
                                     std::move(coep_reporter_to_be_passed));
  }

  if (IsBackForwardCacheEnabled()) {
    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
        base::BindOnce(
            [](int process_id, int frame_id, const std::string& uuid,
               base::WeakPtr<ServiceWorkerContainerHost> self) {
              auto* rfh = RenderFrameHostImpl::FromID(process_id, frame_id);
              // |rfh| may be null in tests (but it should not happen in
              // production).
              if (rfh)
                rfh->AddServiceWorkerContainerHost(uuid, self);
            },
            container_process_id, frame_id_, client_uuid(), GetWeakPtr()));
  }

  TransitionToClientPhase(ClientPhase::kResponseCommitted);
}

void ServiceWorkerContainerHost::OnEndNavigationCommit() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForWindowClient());

  DCHECK(!navigation_commit_ended_);
  navigation_commit_ended_ = true;

  if (controller_) {
    controller_->OnControlleeNavigationCommitted(client_uuid_, process_id_,
                                                 frame_id_);
  }
}

void ServiceWorkerContainerHost::CompleteWebWorkerPreparation(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForWorkerClient());

  DCHECK(!cross_origin_embedder_policy_.has_value());
  cross_origin_embedder_policy_ = cross_origin_embedder_policy;
  if (controller_ && controller_->fetch_handler_existence() ==
                         ServiceWorkerVersion::FetchHandlerExistence::EXISTS) {
    DCHECK(pending_controller_receiver_);
    // TODO(https://crbug.com/999049): Plumb the COEP reporter.
    controller_->controller()->Clone(std::move(pending_controller_receiver_),
                                     cross_origin_embedder_policy_.value(),
                                     mojo::NullRemote());
  }

  TransitionToClientPhase(ClientPhase::kResponseCommitted);
  SetExecutionReady();
}

void ServiceWorkerContainerHost::UpdateUrls(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GURL previous_url = url_;

  DCHECK(!url.has_ref());
  url_ = url;
  site_for_cookies_ = site_for_cookies;
  top_frame_origin_ = top_frame_origin;

  // The remaining parts of this function don't make sense for service worker
  // execution contexts. Return early.
  if (IsContainerForServiceWorker())
    return;

  if (previous_url != url) {
    // Revoke the token on URL change since any service worker holding the token
    // may no longer be the potential controller of this frame and shouldn't
    // have the power to display SSL dialogs for it.
    if (IsContainerForWindowClient()) {
      auto* registry = FrameTreeNodeIdRegistry::GetInstance();
      registry->Remove(fetch_request_window_id_);
      fetch_request_window_id_ = base::UnguessableToken::Create();
      registry->Add(fetch_request_window_id_,
                    client_info_->GetFrameTreeNodeId());
    }
  }

  auto previous_origin = url::Origin::Create(previous_url);
  auto new_origin = url::Origin::Create(url);
  // Update client id on cross origin redirects. This corresponds to the HTML
  // standard's "process a navigation fetch" algorithm's step for discarding
  // |reservedEnvironment|.
  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#process-a-navigate-fetch
  // "If |reservedEnvironment| is not null and |currentURL|'s origin is not the
  // same as |reservedEnvironment|'s creation URL's origin, then:
  //    1. Run the environment discarding steps for |reservedEnvironment|.
  //    2. Set |reservedEnvironment| to null."
  if (previous_url.is_valid() &&
      !new_origin.IsSameOriginWith(previous_origin)) {
    // Remove old controller since we know the controller is definitely
    // changed. We need to remove |this| from |controller_|'s controllee before
    // updating UUID since ServiceWorkerVersion has a map from uuid to provider
    // hosts.
    SetControllerRegistration(nullptr, false /* notify_controllerchange */);

    // Set UUID to the new one.
    std::string previous_client_uuid = client_uuid_;
    client_uuid_ = base::GenerateGUID();
    if (context_)
      context_->UpdateContainerHostClientID(previous_client_uuid, client_uuid_);
  }

  SyncMatchingRegistrations();
}

void ServiceWorkerContainerHost::SetControllerRegistration(
    scoped_refptr<ServiceWorkerRegistration> controller_registration,
    bool notify_controllerchange) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForClient());

  if (controller_registration) {
    CHECK(IsContextSecureForServiceWorker());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
        cross_origin_embedder_policy_.value(),
        std::move(coep_reporter_to_be_passed));
  }
  return remote_controller;
}

namespace {

void ReportServiceWorkerAccess(int process_id,
                               int frame_id,
                               const GURL& scope,
                               AllowServiceWorkerResult allowed) {
  RenderFrameHost* rfh = RenderFrameHost::FromID(process_id, frame_id);
  if (!rfh)
    return;
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(rfh));
  web_contents->OnServiceWorkerAccessed(rfh, scope, allowed);
}

}  // namespace

bool ServiceWorkerContainerHost::AllowServiceWorker(const GURL& scope,
                                                    const GURL& script_url) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(context_);
  AllowServiceWorkerResult allowed = AllowServiceWorkerResult::No();
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    allowed = GetContentClient()->browser()->AllowServiceWorkerOnUI(
        scope, site_for_cookies().RepresentativeUrl(), top_frame_origin(),
        script_url, context_->wrapper()->browser_context());
  } else {
    allowed = GetContentClient()->browser()->AllowServiceWorkerOnIO(
        scope, site_for_cookies().RepresentativeUrl(), top_frame_origin(),
        script_url, context_->wrapper()->resource_context());
  }
  RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                        base::BindOnce(&ReportServiceWorkerAccess, process_id_,
                                       frame_id_, scope, allowed));
  return allowed;
}

bool ServiceWorkerContainerHost::IsContextSecureForServiceWorker() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForClient());

  if (!url_.is_valid())
    return false;
  if (!OriginCanAccessServiceWorkers(url_))
    return false;

  if (is_parent_frame_secure_)
    return true;

  std::set<std::string> schemes;
  GetContentClient()->browser()->GetSchemesBypassingSecureContextCheckAllowlist(
      &schemes);
  return schemes.find(url_.scheme()) != schemes.end();
}

bool ServiceWorkerContainerHost::is_response_committed() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForClient());

  DCHECK(!is_execution_ready());
  execution_ready_callbacks_.push_back(std::move(callback));
}

bool ServiceWorkerContainerHost::is_execution_ready() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForClient());

  return client_phase_ == ClientPhase::kExecutionReady;
}

const std::string& ServiceWorkerContainerHost::client_uuid() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForClient());
  return client_uuid_;
}

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerContainerHost::GetControllerMode() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  CheckControllerConsistency(false);
#endif  // DCHECK_IS_ON()
  return controller_.get();
}

ServiceWorkerRegistration* ServiceWorkerContainerHost::controller_registration()
    const {
#if DCHECK_IS_ON()
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForServiceWorker());
  return service_worker_host_;
}

bool ServiceWorkerContainerHost::IsInBackForwardCache() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  return is_in_back_forward_cache_;
}

void ServiceWorkerContainerHost::EvictFromBackForwardCache(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(IsContainerForWindowClient());
  is_in_back_forward_cache_ = false;
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          [](int process_id, int frame_id,
             BackForwardCacheMetrics::NotRestoredReason reason) {
            auto* rfh = RenderFrameHostImpl::FromID(process_id, frame_id);
            // |rfh| could be evicted before this function is called.
            if (!rfh || !rfh->IsInBackForwardCache())
              return;
            rfh->EvictFromBackForwardCacheWithReason(reason);
          },
          process_id_, frame_id_, reason));
}

void ServiceWorkerContainerHost::OnEnterBackForwardCache() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(IsContainerForWindowClient());
  if (controller_)
    controller_->MoveControlleeToBackForwardCacheMap(client_uuid());
  is_in_back_forward_cache_ = true;
}

void ServiceWorkerContainerHost::OnRestoreFromBackForwardCache() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(IsContainerForWindowClient());
  if (controller_)
    controller_->RestoreControlleeFromBackForwardCacheMap(client_uuid());
  is_in_back_forward_cache_ = false;
}

base::WeakPtr<ServiceWorkerContainerHost>
ServiceWorkerContainerHost::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  return weak_factory_.GetWeakPtr();
}

void ServiceWorkerContainerHost::SyncMatchingRegistrations() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!controller_registration_);

  RemoveAllMatchingRegistrations();
  if (!context_)
    return;
  const auto& registrations = context_->GetLiveRegistrations();
  for (const auto& key_registration : registrations) {
    ServiceWorkerRegistration* registration = key_registration.second;
    if (!registration->is_uninstalled() &&
        blink::ServiceWorkerScopeMatches(registration->scope(), url_)) {
      AddMatchingRegistration(registration);
    }
  }
}

#if DCHECK_IS_ON()
bool ServiceWorkerContainerHost::IsMatchingRegistration(
    ServiceWorkerRegistration* registration) const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!controller_registration_);
  for (const auto& it : matching_registrations_) {
    ServiceWorkerRegistration* registration = it.second.get();
    registration->RemoveListener(this);
  }
  matching_registrations_.clear();
}

void ServiceWorkerContainerHost::ReturnRegistrationForReadyIfNeeded() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (!get_ready_callback_ || get_ready_callback_->is_null())
    return;
  ServiceWorkerRegistration* registration = MatchRegistration();
  if (!registration || !registration->active_version())
    return;
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerContainerHost::GetRegistrationForReady",
                         this, "Registration ID", registration->id());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!is_execution_ready());
  TransitionToClientPhase(ClientPhase::kExecutionReady);
  RunExecutionReadyCallbacks();
}

void ServiceWorkerContainerHost::RunExecutionReadyCallbacks() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsContainerForClient());

  std::vector<ExecutionReadyCallback> callbacks;
  execution_ready_callbacks_.swap(callbacks);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&RunCallbacks, std::move(callbacks)));
}

void ServiceWorkerContainerHost::TransitionToClientPhase(
    ClientPhase new_phase) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  ServiceWorkerVersion* version =
      controller_registration_ ? controller_registration_->active_version()
                               : nullptr;
  CHECK(!version || IsContextSecureForServiceWorker());
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
    previous_version->RemoveControllee(client_uuid());

  // SetController message should be sent only for clients.
  DCHECK(IsContainerForClient());

  if (!is_execution_ready())
    return;

  SendSetControllerServiceWorker(notify_controllerchange);
}

#if DCHECK_IS_ON()
void ServiceWorkerContainerHost::CheckControllerConsistency(
    bool should_crash) const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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

    controller_->controller()->Clone(std::move(receiver),
                                     cross_origin_embedder_policy_.value(),
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  TRACE_EVENT_ASYNC_END2("ServiceWorker",
                         "ServiceWorkerContainerHost::Register", trace_id,
                         "Status", blink::ServiceWorkerStatusToString(status),
                         "Registration ID", registration_id);

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

  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id);
  // ServiceWorkerRegisterJob calls its completion callback, which results in
  // this function being called, while the registration is live.
  DCHECK(registration);

  std::move(callback).Run(
      blink::mojom::ServiceWorkerErrorType::kNone, base::nullopt,
      CreateServiceWorkerRegistrationObjectInfo(
          scoped_refptr<ServiceWorkerRegistration>(registration)));
}

void ServiceWorkerContainerHost::GetRegistrationComplete(
    GetRegistrationCallback callback,
    int64_t trace_id,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistration", trace_id,
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
                          base::nullopt, std::move(info));
}

void ServiceWorkerContainerHost::GetRegistrationsComplete(
    GetRegistrationsCallback callback,
    int64_t trace_id,
    blink::ServiceWorkerStatusCode status,
    const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
        registrations) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  TRACE_EVENT_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistrations", trace_id,
      "Status", blink::ServiceWorkerStatusToString(status));

  if (!context_) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        std::string(
            ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        base::nullopt);
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
        base::nullopt);
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
                          base::nullopt, std::move(object_infos));
}

bool ServiceWorkerContainerHost::IsValidGetRegistrationMessage(
    const GURL& client_url,
    std::string* out_error) const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (!IsContainerForWindowClient()) {
    *out_error = ServiceWorkerConsts::kBadMessageFromNonWindow;
    return false;
  }
  if (!client_url.is_valid()) {
    *out_error = ServiceWorkerConsts::kBadMessageInvalidURL;
    return false;
  }
  std::vector<GURL> urls = {url_, client_url};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    *out_error = ServiceWorkerConsts::kBadMessageImproperOrigins;
    return false;
  }

  return true;
}

bool ServiceWorkerContainerHost::IsValidGetRegistrationsMessage(
    std::string* out_error) const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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

}  // namespace content
