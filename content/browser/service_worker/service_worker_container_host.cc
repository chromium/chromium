// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_container_host.h"

#include <set>
#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_running_status_callback.mojom.h"

namespace content {

namespace {

void RunCallbacks(
    std::vector<ServiceWorkerClient::ExecutionReadyCallback> callbacks) {
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/40918057): remove this metrics if we confirm that
// kContainerNotReady prevents calling the CountFeature IPC.
enum class CountFeatureDropOutReason {
  kOk = 0,
  kContainerNotReady = 1,
  kExecutionNotReady = 2,
  kNotBoundOrNotConnected = 3,
  kMaxValue = kNotBoundOrNotConnected,
};

// Max number of messages that can be sent before |container_| gets ready.
// I believe messages may not be sent in that situation for regular way, but
// we technically do not prevent finding a client and send a message in that
// phase.
// 128 is picked randomly. We may need to run a experiment to decide the precise
// number.
constexpr size_t kMaxBufferedMessageSize = 128;

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

ServiceWorkerContainerHost::ServiceWorkerContainerHost() = default;

ServiceWorkerContainerHostForServiceWorker::
    ServiceWorkerContainerHostForServiceWorker(
        base::WeakPtr<ServiceWorkerContextCore> context,
        ServiceWorkerHost* service_worker_host,
        const GURL& url,
        const blink::StorageKey& storage_key)
    : service_worker_host_(service_worker_host),
      context_(std::move(context)),
      url_(url),
      key_(storage_key),
      top_frame_origin_(url::Origin::Create(key_.top_level_site().GetURL())) {
  DCHECK(context_);
  CHECK(!url_.has_ref());
  service_worker_security_utils::CheckOnUpdateUrls(url_, key_);
}

ServiceWorkerClient::ServiceWorkerClient(
    base::WeakPtr<ServiceWorkerContextCore> context,
    bool is_parent_frame_secure,
    int frame_tree_node_id)
    : context_(std::move(context)),
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

ServiceWorkerContainerHost::~ServiceWorkerContainerHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}
ServiceWorkerContainerHostForClient::~ServiceWorkerContainerHostForClient() =
    default;
ServiceWorkerContainerHostForServiceWorker::
    ~ServiceWorkerContainerHostForServiceWorker() = default;

ServiceWorkerContainerHostForClient::ServiceWorkerContainerHostForClient(
    base::WeakPtr<ServiceWorkerClient> service_worker_client,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote)
    : service_worker_client_(std::move(service_worker_client)),
      container_(std::move(container_remote)) {
  CHECK(service_worker_client_);
  DCHECK(container_.is_bound());
}

void ServiceWorkerContainerHostForClient::Create(
    base::WeakPtr<ServiceWorkerClient> service_worker_client,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote) {
  service_worker_client->set_container_host(
      std::make_unique<ServiceWorkerContainerHostForClient>(
          service_worker_client, std::move(container_remote)));
}

void ServiceWorkerClient::set_container_host(
    std::unique_ptr<ServiceWorkerContainerHostForClient> container_host) {
  CHECK(!container_host_);
  CHECK(container_host);
  container_host_ = std::move(container_host);
}

void ServiceWorkerContainerHostForClient::Register(
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

  if (!service_worker_client().IsContainerForWindowClient()) {
    mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageFromNonWindow);
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }

  std::vector<GURL> urls = {url(), options->scope, script_url};
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
          OriginCanRegisterServiceWorkerFromJavascript(url())) {
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
  // TODO(crbug.com/40646828): Remove this wrapper and have the Mojo connections
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
  GlobalRenderFrameHostId global_frame_id =
      service_worker_client().GetRenderFrameHostId();
  DCHECK_NE(global_frame_id.child_id, ChildProcessHost::kInvalidUniqueID);
  DCHECK_NE(global_frame_id.frame_routing_id, MSG_ROUTING_NONE);

  // Registrations could come from different origins when "disable-web-security"
  // is active, we need to make sure we get the correct key.
  const blink::StorageKey key =
      service_worker_security_utils::GetCorrectStorageKeyForWebSecurityState(
          service_worker_client().key(), options->scope);

  context()->RegisterServiceWorker(
      script_url, key, *options,
      std::move(outside_fetch_client_settings_object),
      base::BindOnce(&ServiceWorkerContainerHostForClient::RegistrationComplete,
                     base::AsWeakPtr(this), GURL(script_url),
                     GURL(options->scope), std::move(wrapped_callback),
                     trace_id, mojo::GetBadMessageCallback()),
      global_frame_id,
      service_worker_client().policy_container_policies().value());
}

void ServiceWorkerContainerHostForClient::GetRegistration(
    const GURL& client_url,
    GetRegistrationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanServeContainerHostMethods(
          &callback, url(), GURL(),
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
  const blink::StorageKey key =
      service_worker_security_utils::GetCorrectStorageKeyForWebSecurityState(
          service_worker_client().key(), client_url);

  context()->registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation, client_url, key,
      base::BindOnce(
          &ServiceWorkerContainerHostForClient::GetRegistrationComplete,
          base::AsWeakPtr(this), std::move(callback), trace_id));
}

void ServiceWorkerContainerHostForClient::GetRegistrations(
    GetRegistrationsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanServeContainerHostMethods(
          &callback, url(), GURL(),
          ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix,
          std::nullopt)) {
    return;
  }

  std::string error_message;
  if (!IsValidGetRegistrationsMessage(&error_message)) {
    mojo::ReportBadMessage(error_message);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), std::nullopt);
    return;
  }

  int64_t trace_id = base::TimeTicks::Now().since_origin().InMicroseconds();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistrations",
      TRACE_ID_WITH_SCOPE("ServiceWorkerContainerHost::GetRegistrations",
                          trace_id));
  context()->registry()->GetRegistrationsForStorageKey(
      service_worker_client().key(),
      base::BindOnce(
          &ServiceWorkerContainerHostForClient::GetRegistrationsComplete,
          base::AsWeakPtr(this), std::move(callback), trace_id));
}

void ServiceWorkerContainerHostForClient::GetRegistrationForReady(
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

void ServiceWorkerContainerHostForClient::EnsureControllerServiceWorker(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_worker_client().EnsureControllerServiceWorker(std::move(receiver),
                                                        purpose);
}

void ServiceWorkerClient::EnsureControllerServiceWorker(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  // TODO(kinuko): Log the reasons we drop the request.
  if (!context_ || !controller_)
    return;

  controller_->RunAfterStartWorker(
      PurposeToEventType(purpose),
      base::BindOnce(&ServiceWorkerClient::StartControllerComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
}

void ServiceWorkerContainerHost::CloneContainerHost(
    mojo::PendingReceiver<blink::mojom::ServiceWorkerContainerHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  additional_receivers_.Add(this, std::move(receiver));
}

void ServiceWorkerContainerHostForClient::HintToUpdateServiceWorker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_worker_client().HintToUpdateServiceWorker();
}

void ServiceWorkerClient::HintToUpdateServiceWorker() {
  // The destructors notify the ServiceWorkerVersions to update.
  versions_to_update_.clear();
}

void ServiceWorkerContainerHostForClient::EnsureFileAccess(
    const std::vector<base::FilePath>& file_paths,
    EnsureFileAccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_worker_client().EnsureFileAccess(file_paths, std::move(callback));
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

      if (!policy->CanReadFile(controller_process_id, file))
        policy->GrantReadFile(controller_process_id, file);
    }
  }

  std::move(callback).Run();
}

void ServiceWorkerContainerHostForClient::OnExecutionReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_worker_client().OnExecutionReady();
}

void ServiceWorkerClient::OnExecutionReady() {
  // Since `OnExecutionReady()` is a part of `ServiceWorkerContainerHost`,
  // this method is called only if `is_container_ready_` is true.
  CHECK(is_container_ready_);

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

void ServiceWorkerContainerHostForServiceWorker::Register(
    const GURL& script_url,
    blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    RegisterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageFromNonWindow);
  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                          std::string(), nullptr);
}

void ServiceWorkerContainerHostForServiceWorker::GetRegistration(
    const GURL& client_url,
    GetRegistrationCallback callback) {
  mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageFromNonWindow);
  // ReportBadMessage() will kill the renderer process, but Mojo complains if
  // the callback is not run. Just run it with nonsense arguments.
  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                          std::string(), nullptr);
}

void ServiceWorkerContainerHostForServiceWorker::GetRegistrations(
    GetRegistrationsCallback callback) {
  mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageFromNonWindow);
  // ReportBadMessage() will kill the renderer process, but Mojo complains if
  // the callback is not run. Just run it with nonsense arguments.
  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                          std::string(), std::nullopt);
}

void ServiceWorkerContainerHostForServiceWorker::GetRegistrationForReady(
    GetRegistrationForReadyCallback callback) {
  std::string error_message;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageFromNonWindow);
  // ReportBadMessage() will kill the renderer process, but Mojo complains if
  // the callback is not run. Just run it with nonsense arguments.
  std::move(callback).Run(nullptr);
}

void ServiceWorkerContainerHostForServiceWorker::EnsureControllerServiceWorker(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ServiceWorkerContainerHostForServiceWorker::HintToUpdateServiceWorker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::ReportBadMessage("SWPH_HTUSW_NOT_CLIENT");
}

void ServiceWorkerContainerHostForServiceWorker::EnsureFileAccess(
    const std::vector<base::FilePath>& file_paths,
    EnsureFileAccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void ServiceWorkerContainerHostForServiceWorker::OnExecutionReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::ReportBadMessage("SWPH_OER_NOT_CLIENT");
}

void ServiceWorkerClient::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  container_host().OnVersionAttributesChanged(registration,
                                              std::move(changed_mask));
}

void ServiceWorkerContainerHostForClient::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!get_ready_callback_ || get_ready_callback_->is_null())
    return;
  if (changed_mask->active && registration->active_version()) {
    // Wait until the state change so we don't send the get for ready
    // registration complete message before set version attributes message.
    registration->active_version()->RegisterStatusChangeCallback(
        base::BindOnce(&ServiceWorkerContainerHostForClient::
                           ReturnRegistrationForReadyIfNeeded,
                       base::AsWeakPtr(this)));
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

void ServiceWorkerClient::AddMatchingRegistration(
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

  container_host().ReturnRegistrationForReadyIfNeeded();
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
    if (registration.second->is_uninstalled())
      continue;
    if (registration.second->is_uninstalling())
      return nullptr;
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

void ServiceWorkerClient::PostMessageToClient(
    ServiceWorkerVersion* version,
    blink::TransferableMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::WeakPtr<ServiceWorkerObjectHost> object_host =
      container_host().version_object_manager().GetOrCreateHost(version);
  if (!is_container_ready_) {
    if (buffered_messages_.size() < kMaxBufferedMessageSize) {
      buffered_messages_.emplace_back(object_host, std::move(message));
    }
    return;
  }

  container_host().PostMessageToClient(std::move(object_host),
                                       std::move(message));
}

void ServiceWorkerContainerHostForClient::PostMessageToClient(
    base::WeakPtr<ServiceWorkerObjectHost> object_host,
    blink::TransferableMessage message) {
  blink::mojom::ServiceWorkerObjectInfoPtr info;
  if (object_host)
    info = object_host->CreateCompleteObjectInfoToSend();
  container_->PostMessageToClient(std::move(info), std::move(message));
}

void ServiceWorkerClient::CountFeature(blink::mojom::WebFeature feature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_CRASH_KEY_NUMBER("SWCH_CF", "feature", static_cast<int32_t>(feature));
  SCOPED_CRASH_KEY_NUMBER("SWCH_CF", "client_type",
                          static_cast<int32_t>(GetClientType()));

  constexpr char kDropOutMetrics[] = "ServiceWorker.CountFeature.DropOut";

  // `container_` can be used only if ServiceWorkerContainerInfoForClient has
  // been passed to the renderer process. Otherwise, the method call will crash
  // inside the mojo library (See crbug.com/40918057).
  if (!is_container_ready_) {
    base::UmaHistogramEnumeration(
        kDropOutMetrics, CountFeatureDropOutReason::kContainerNotReady);
    buffered_used_features_.insert(feature);
    return;
  }

  // And only when loading finished so the controller is really settled.
  if (!is_execution_ready()) {
    base::UmaHistogramEnumeration(
        kDropOutMetrics, CountFeatureDropOutReason::kExecutionNotReady);
    buffered_used_features_.insert(feature);
    return;
  }

  container_host().CountFeature(feature);
}

void ServiceWorkerContainerHostForClient::CountFeature(
    blink::mojom::WebFeature feature) {
  constexpr char kDropOutMetrics[] = "ServiceWorker.CountFeature.DropOut";

  // `container_` shouldn't be disconnected during the lifetime of `this` but
  // there seems a situation where `container_` is disconnected or unbound.
  // TODO(crbug.com/1136843, crbug.com/40918057): Figure out the cause and
  // remove this check.
  if (!container_.is_bound() || !container_.is_connected()) {
    base::UmaHistogramEnumeration(
        kDropOutMetrics, CountFeatureDropOutReason::kNotBoundOrNotConnected);
    return;
  }

  base::UmaHistogramEnumeration(kDropOutMetrics,
                                CountFeatureDropOutReason::kOk);
  container_->CountFeature(feature);
}

blink::mojom::ControllerServiceWorkerInfoPtr
ServiceWorkerClient::CreateControllerServiceWorkerInfo() {
  CHECK(controller());

  auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
  controller_info->client_id = client_uuid();
  controller_info->mode = controller()->GetControllerMode();
  controller_info->fetch_handler_type = controller()->fetch_handler_type();
  controller_info->fetch_handler_bypass_option =
      controller()->fetch_handler_bypass_option();
  controller_info->sha256_script_checksum =
      controller()->sha256_script_checksum();
  controller_info->need_router_evaluate = controller()->NeedRouterEvaluate();

  if (controller()->router_evaluator()) {
    controller_info->router_data = blink::mojom::ServiceWorkerRouterData::New();
    controller_info->router_data->router_rules =
        controller()->router_evaluator()->rules();
    // Pass an endpoint for the cache storage.
    mojo::PendingRemote<blink::mojom::CacheStorage> remote_cache_storage =
        controller()->GetRemoteCacheStorage();
    if (remote_cache_storage) {
      controller_info->router_data->remote_cache_storage =
          std::move(remote_cache_storage);
    }
    if (controller()->router_evaluator()->need_running_status()) {
      controller_info->router_data->running_status_receiver =
          GetRunningStatusCallbackReceiver();
      controller_info->router_data->initial_running_status =
          controller()->running_status();
    }
  }

  // Note that |controller_info->remote_controller| is null if the controller
  // has no fetch event handler. In that case the renderer frame won't get the
  // controller pointer upon the navigation commit, and subresource loading will
  // not be intercepted. (It might get intercepted later if the controller
  // changes due to skipWaiting() so SetController is sent.)
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote =
      GetRemoteControllerServiceWorker();
  if (remote.is_bound()) {
    controller_info->remote_controller = remote.Unbind();
  }

  if (fetch_request_window_id()) {
    controller_info->fetch_request_window_id =
        std::make_optional(fetch_request_window_id());
  }
  // Populate used features for UseCounter purposes.
  for (const auto feature : controller()->used_features()) {
    controller_info->used_features.push_back(feature);
  }
  return controller_info;
}

void ServiceWorkerClient::SendSetControllerServiceWorker(
    bool notify_controllerchange) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_container_ready_);

  if (!controller_ || !context_) {
    // Do not set |fetch_request_window_id| when |controller_| is not available.
    // Setting |fetch_request_window_id| should not affect correctness, however,
    // we have the extensions bug, https://crbug.com/963748, which we don't yet
    // understand.  That is why we don't set |fetch_request_window_id| if there
    // is no controller, at least, until we can fix the extension bug.
    //
    // Also check if |context_| is not null. This is a speculative fix for
    // crbug.com/324559079. When |controller_info->fetch_request_window_id|
    // is set, the renderer expects that |controller_info->object_info| is also
    // set as a controller. |controller_info->object_info| is set in
    // `version_object_manager().GetOrCreateHost()`, but that may return null if
    // |context_| does not exist. To avoid the potential inconsistency with the
    // renderer side, setController as no-controller.
    auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
    controller_info->client_id = client_uuid();
    container_host().SendSetController(std::move(controller_info),
                                       notify_controllerchange);
    return;
  }

  DCHECK(controller_registration());
  DCHECK_EQ(controller_registration_->active_version(), controller_.get());

  auto controller_info = CreateControllerServiceWorkerInfo();

  // Set the info for the JavaScript ServiceWorkerContainer#controller object.
  if (base::WeakPtr<ServiceWorkerObjectHost> object_host =
          container_host().version_object_manager().GetOrCreateHost(
              controller())) {
    controller_info->object_info =
        object_host->CreateCompleteObjectInfoToSend();
  }

  // TODO(crbug.com/331279951): Remove these crash keys after investigation.
  SCOPED_CRASH_KEY_NUMBER("SWCH_SC", "client_type",
                          static_cast<int32_t>(GetClientType()));
  SCOPED_CRASH_KEY_BOOL("SWCH_SC", "is_execution_ready", is_execution_ready());
  SCOPED_CRASH_KEY_BOOL("SWCH_SC", "is_container_ready", is_container_ready_);

  container_host().SendSetController(std::move(controller_info),
                                     notify_controllerchange);
}

void ServiceWorkerContainerHostForClient::SendSetController(
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    bool notify_controllerchange) {
  // TODO(crbug.com/331279951): Remove these crash keys after investigation.
  SCOPED_CRASH_KEY_BOOL("SWCH_SC", "is_bound", container_.is_bound());
  SCOPED_CRASH_KEY_BOOL("SWCH_SC", "is_connected",
                        container_.is_bound() && container_.is_connected());
  SCOPED_CRASH_KEY_BOOL("SWCH_SC", "notify_controllerchange",
                        notify_controllerchange);

  container_->SetController(std::move(controller_info),
                            notify_controllerchange);
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

ServiceWorkerRegistrationObjectManager::ServiceWorkerRegistrationObjectManager(
    ServiceWorkerContainerHost* container_host)
    : container_host_(*container_host) {}
ServiceWorkerRegistrationObjectManager::
    ~ServiceWorkerRegistrationObjectManager() = default;

blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
ServiceWorkerRegistrationObjectManager::CreateInfo(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t registration_id = registration->id();
  auto existing_host = registration_object_hosts_.find(registration_id);
  if (existing_host != registration_object_hosts_.end()) {
    return existing_host->second->CreateObjectInfo();
  }
  registration_object_hosts_[registration_id] =
      std::make_unique<ServiceWorkerRegistrationObjectHost>(
          container_host_->context(), &container_host_.get(),
          std::move(registration));
  return registration_object_hosts_[registration_id]->CreateObjectInfo();
}

void ServiceWorkerRegistrationObjectManager::RemoveHost(
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

ServiceWorkerObjectManager::ServiceWorkerObjectManager(
    ServiceWorkerContainerHost* container_host)
    : container_host_(*container_host) {}
ServiceWorkerObjectManager::~ServiceWorkerObjectManager() = default;

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerObjectManager::CreateInfoToSend(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t version_id = version->version_id();
  auto existing_object_host = service_worker_object_hosts_.find(version_id);
  if (existing_object_host != service_worker_object_hosts_.end()) {
    return existing_object_host->second->CreateCompleteObjectInfoToSend();
  }
  service_worker_object_hosts_[version_id] =
      std::make_unique<ServiceWorkerObjectHost>(container_host_->context(),
                                                container_host_->AsWeakPtr(),
                                                std::move(version));
  return service_worker_object_hosts_[version_id]
      ->CreateCompleteObjectInfoToSend();
}

base::WeakPtr<ServiceWorkerObjectHost>
ServiceWorkerObjectManager::GetOrCreateHost(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!container_host_->context() || !version) {
    return nullptr;
  }

  const int64_t version_id = version->version_id();
  auto existing_object_host = service_worker_object_hosts_.find(version_id);
  if (existing_object_host != service_worker_object_hosts_.end())
    return existing_object_host->second->AsWeakPtr();

  service_worker_object_hosts_[version_id] =
      std::make_unique<ServiceWorkerObjectHost>(container_host_->context(),
                                                container_host_->AsWeakPtr(),
                                                std::move(version));
  return service_worker_object_hosts_[version_id]->AsWeakPtr();
}

void ServiceWorkerObjectManager::RemoveHost(int64_t version_id) {
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
  if (!client_info_)
    return false;

  return absl::holds_alternative<blink::DedicatedWorkerToken>(*client_info_) ||
         absl::holds_alternative<blink::SharedWorkerToken>(*client_info_);
}

ServiceWorkerClientInfo ServiceWorkerClient::GetServiceWorkerClientInfo()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *client_info_;
}

void ServiceWorkerClient::CommitResponse(
    std::optional<GlobalRenderFrameHostId> rfh_id,
    const PolicyContainerPolicies& policy_container_policies,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    ukm::SourceId ukm_source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsContainerForWindowClient()) {
    CHECK(coep_reporter);
    CHECK(rfh_id);
    ongoing_navigation_frame_tree_node_id_ =
        RenderFrameHost::kNoFrameTreeNodeId;
    client_info_ = *rfh_id;

    if (controller_) {
      controller_->UpdateForegroundPriority();
    }
  }

  DCHECK(!policy_container_policies_.has_value());
  policy_container_policies_ = policy_container_policies.Clone();

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_to_be_passed;
  if (coep_reporter) {
    coep_reporter_.Bind(std::move(coep_reporter));
    coep_reporter_->Clone(
        coep_reporter_to_be_passed.InitWithNewPipeAndPassReceiver());
  }

  if (controller_ && controller_->fetch_handler_existence() ==
                         ServiceWorkerVersion::FetchHandlerExistence::EXISTS) {
    DCHECK(pending_controller_receiver_);
    controller_->controller()->Clone(
        std::move(pending_controller_receiver_),
        policy_container_policies_->cross_origin_embedder_policy,
        std::move(coep_reporter_to_be_passed));
  }

  if (IsContainerForWindowClient()) {
    auto* rfh = RenderFrameHostImpl::FromID(*rfh_id);
    // `rfh` may be null in tests (but it should not happen in production).
    if (rfh) {
      rfh->AddServiceWorkerClient(client_uuid(),
                                  weak_ptr_factory_.GetWeakPtr());
    }
  }

  DCHECK_EQ(ukm_source_id_, ukm::kInvalidSourceId);
  ukm_source_id_ = ukm_source_id;

  TransitionToClientPhase(ClientPhase::kResponseCommitted);
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

void ServiceWorkerClient::UpdateUrls(
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
      context_->UpdateServiceWorkerClientClientID(previous_client_uuid,
                                                  client_uuid_);
  }

  SyncMatchingRegistrations();
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

mojo::Remote<blink::mojom::ControllerServiceWorker>
ServiceWorkerClient::GetRemoteControllerServiceWorker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
      // TODO(crbug.com/41478971): Implement DedicatedWorker and
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

bool ServiceWorkerContainerHostForClient::AllowServiceWorker(
    const GURL& scope,
    const GURL& script_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(context());
  auto* browser_context = context()->wrapper()->browser_context();
  // Check that the browser context is not nullptr.  It becomes nullptr
  // when the service worker process manager is being shutdown.
  if (!browser_context) {
    return false;
  }
  AllowServiceWorkerResult allowed =
      GetContentClient()->browser()->AllowServiceWorker(
          scope,
          service_worker_security_utils::site_for_cookies(
              service_worker_client().key()),
          service_worker_client().top_frame_origin(), script_url,
          browser_context);
  if (service_worker_client().IsContainerForWindowClient()) {
    auto* rfh = RenderFrameHostImpl::FromID(
        service_worker_client().GetRenderFrameHostId());
    auto* web_contents =
        static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(rfh));
    if (web_contents) {
      web_contents->OnServiceWorkerAccessed(rfh, scope, allowed);
    }
  }
  return allowed;
}

bool ServiceWorkerContainerHostForServiceWorker::AllowServiceWorker(
    const GURL& scope,
    const GURL& script_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(context());
  auto* browser_context = context()->wrapper()->browser_context();
  // Check that the browser context is not nullptr.  It becomes nullptr
  // when the service worker process manager is being shutdown.
  if (!browser_context) {
    return false;
  }
  return GetContentClient()->browser()->AllowServiceWorker(
      scope, service_worker_security_utils::site_for_cookies(key_),
      top_frame_origin(), script_url, browser_context);
}

bool ServiceWorkerClient::IsEligibleForServiceWorkerController() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

bool ServiceWorkerClient::is_response_committed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (client_phase_) {
    case ClientPhase::kInitial:
      return false;
    case ClientPhase::kResponseCommitted:
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
  return client_phase_ == ClientPhase::kExecutionReady;
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

const base::WeakPtr<ServiceWorkerContextCore>&
ServiceWorkerContainerHostForClient::context() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return service_worker_client().context();
}

const GURL& ServiceWorkerContainerHostForClient::url() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return service_worker_client().url();
}

const base::WeakPtr<ServiceWorkerContextCore>&
ServiceWorkerContainerHostForServiceWorker::context() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context_;
}

const GURL& ServiceWorkerContainerHostForServiceWorker::url() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_;
}

ServiceWorkerHost*
ServiceWorkerContainerHostForServiceWorker::service_worker_host() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return service_worker_host_;
}

const blink::StorageKey& ServiceWorkerContainerHostForServiceWorker::key()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return key_;
}

const url::Origin&
ServiceWorkerContainerHostForServiceWorker::top_frame_origin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return top_frame_origin_;
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

  if (!IsContainerForWindowClient())
    return;

  auto* rfh = RenderFrameHostImpl::FromID(GetRenderFrameHostId());
  // |rfh| could be evicted before this function is called.
  if (rfh && rfh->IsInBackForwardCache())
    rfh->EvictFromBackForwardCacheWithReason(reason);
}

void ServiceWorkerClient::OnEnterBackForwardCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsBackForwardCacheEnabled());
  if (controller_)
    controller_->MoveControlleeToBackForwardCacheMap(client_uuid());
  is_in_back_forward_cache_ = true;
}

void ServiceWorkerClient::OnRestoreFromBackForwardCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsBackForwardCacheEnabled());
  if (controller_)
    controller_->RestoreControlleeFromBackForwardCacheMap(client_uuid());
  is_in_back_forward_cache_ = false;
}

void ServiceWorkerClient::SyncMatchingRegistrations() {
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
bool ServiceWorkerClient::IsMatchingRegistration(
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

void ServiceWorkerClient::RemoveAllMatchingRegistrations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!controller_registration_);
  for (const auto& it : matching_registrations_) {
    ServiceWorkerRegistration* registration = it.second.get();
    registration->RemoveListener(this);
  }
  matching_registrations_.clear();
}

void ServiceWorkerContainerHostForClient::ReturnRegistrationForReadyIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!get_ready_callback_ || get_ready_callback_->is_null())
    return;
  ServiceWorkerRegistration* registration =
      service_worker_client().MatchRegistration();
  if (!registration || !registration->active_version())
    return;
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerContainerHost::GetRegistrationForReady",
      TRACE_ID_LOCAL(this), "Registration ID", registration->id());
  if (!context()) {
    // Here no need to run or destroy |get_ready_callback_|, which will destroy
    // together with |receiver_| when |this| destroys.
    return;
  }

  std::move(*get_ready_callback_)
      .Run(registration_object_manager().CreateInfo(
          scoped_refptr<ServiceWorkerRegistration>(registration)));
}

void ServiceWorkerClient::SetExecutionReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_execution_ready());
  TransitionToClientPhase(ClientPhase::kExecutionReady);
  RunExecutionReadyCallbacks();

  if (context_)
    context_->NotifyClientIsExecutionReady(*this);

  FlushFeatures();
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

void ServiceWorkerClient::UpdateController(bool notify_controllerchange) {
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
  if (!is_container_ready_) {
    return;
  }

  if (!is_execution_ready()) {
    return;
  }

  SendSetControllerServiceWorker(notify_controllerchange);
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

void ServiceWorkerClient::StartControllerComplete(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(is_response_committed());

    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter_to_be_passed;
    if (coep_reporter_) {
      DCHECK(IsContainerForWindowClient());
      coep_reporter_->Clone(
          coep_reporter_to_be_passed.InitWithNewPipeAndPassReceiver());
    } else {
      // TODO(crbug.com/41478971): Implement DedicatedWorker and
      // SharedWorker cases.
      DCHECK(IsContainerForWorkerClient());
    }

    controller_->controller()->Clone(
        std::move(receiver),
        policy_container_policies_->cross_origin_embedder_policy,
        std::move(coep_reporter_to_be_passed));
  }
}

void ServiceWorkerContainerHostForClient::RegistrationComplete(
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

  if (!context()) {
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
      context()->GetLiveRegistration(registration_id);
  // ServiceWorkerRegisterJob calls its completion callback, which results in
  // this function being called, while the registration is live.
  DCHECK(registration);

  std::move(callback).Run(
      blink::mojom::ServiceWorkerErrorType::kNone, std::nullopt,
      registration_object_manager().CreateInfo(
          scoped_refptr<ServiceWorkerRegistration>(registration)));
}

void ServiceWorkerContainerHostForClient::GetRegistrationComplete(
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

  if (!context()) {
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
    info = registration_object_manager().CreateInfo(std::move(registration));
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          std::nullopt, std::move(info));
}

void ServiceWorkerContainerHostForClient::GetRegistrationsComplete(
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

  if (!context()) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        std::string(
            ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        std::nullopt);
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
        std::nullopt);
    return;
  }

  std::vector<blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>
      object_infos;

  for (const auto& registration : registrations) {
    DCHECK(registration.get());
    if (!registration->is_uninstalling()) {
      object_infos.push_back(
          registration_object_manager().CreateInfo(std::move(registration)));
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
                          std::nullopt, std::move(object_infos));
}

bool ServiceWorkerContainerHostForClient::IsValidGetRegistrationMessage(
    const GURL& client_url,
    std::string* out_error) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!service_worker_client().IsContainerForWindowClient()) {
    *out_error = ServiceWorkerConsts::kBadMessageFromNonWindow;
    return false;
  }
  if (!client_url.is_valid()) {
    *out_error = ServiceWorkerConsts::kBadMessageInvalidURL;
    return false;
  }
  std::vector<GURL> urls = {url(), client_url};
  if (!service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          urls)) {
    *out_error = ServiceWorkerConsts::kBadMessageImproperOrigins;
    return false;
  }

  return true;
}

bool ServiceWorkerContainerHostForClient::IsValidGetRegistrationsMessage(
    std::string* out_error) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!service_worker_client().IsContainerForWindowClient()) {
    *out_error = ServiceWorkerConsts::kBadMessageFromNonWindow;
    return false;
  }
  if (!OriginCanAccessServiceWorkers(url())) {
    *out_error = ServiceWorkerConsts::kBadMessageImproperOrigins;
    return false;
  }

  return true;
}

bool ServiceWorkerContainerHostForClient::IsValidGetRegistrationForReadyMessage(
    std::string* out_error) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!service_worker_client().IsContainerForWindowClient()) {
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
bool ServiceWorkerContainerHostForClient::CanServeContainerHostMethods(
    CallbackType* callback,
    const GURL& scope,
    const GURL& script_url,
    const char* error_prefix,
    Args... args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context()) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        std::string(error_prefix) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        args...);
    return false;
  }

  // TODO(falken): This check can be removed once crbug.com/439697 is fixed.
  // (Also see crbug.com/776408)
  if (url().is_empty()) {
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

const GURL& ServiceWorkerClient::GetUrlForScopeMatch() const {
  if (!scope_match_url_for_blob_client_.is_empty())
    return scope_match_url_for_blob_client_;
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

SubresourceLoaderParams ServiceWorkerClient::MaybeCreateSubresourceLoaderParams(
    base::WeakPtr<ServiceWorkerClient> service_worker_client) {
  // We didn't find a matching service worker for this request, and
  // ServiceWorkerContainerHost::SetControllerRegistration() was not called.
  if (!service_worker_client || !service_worker_client->controller()) {
    return {};
  }

  // Otherwise let's send the controller service worker information along
  // with the navigation commit.
  SubresourceLoaderParams params;
  params.controller_service_worker_info =
      service_worker_client->CreateControllerServiceWorkerInfo();
  if (base::WeakPtr<ServiceWorkerObjectHost> object_host =
          service_worker_client->container_host()
              .version_object_manager()
              .GetOrCreateHost(service_worker_client->controller())) {
    params.controller_service_worker_object_host = object_host;
    params.controller_service_worker_info->object_info =
        object_host->CreateIncompleteObjectInfo();
  }
  params.service_worker_client = service_worker_client;

  return params;
}

void ServiceWorkerClient::SetContainerReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_container_ready_ = true;
  std::vector<std::tuple<base::WeakPtr<ServiceWorkerObjectHost>,
                         blink::TransferableMessage>>
      messages;

  messages.swap(buffered_messages_);
  base::UmaHistogramCounts1000("ServiceWorker.PostMessage.QueueSize",
                               messages.size());
  for (auto& [object_host, message] : messages) {
    container_host().PostMessageToClient(std::move(object_host),
                                         std::move(message));
  }
  CHECK(buffered_messages_.empty());

  FlushFeatures();
}

void ServiceWorkerClient::FlushFeatures() {
  std::set<blink::mojom::WebFeature> features;
  features.swap(buffered_used_features_);
  for (const auto& feature : features) {
    CountFeature(feature);
  }
}

namespace {

using StatusCallback = base::OnceCallback<void(blink::ServiceWorkerStatusCode)>;
using PrepareExtendableMessageEventCallback =
    base::OnceCallback<bool(blink::mojom::ExtendableMessageEventPtr*)>;

void DispatchExtendableMessageEventAfterStartWorker(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const std::optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    PrepareExtendableMessageEventCallback prepare_callback,
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(start_worker_status);
    return;
  }

  blink::mojom::ExtendableMessageEventPtr event =
      blink::mojom::ExtendableMessageEvent::New();
  event->message = std::move(message);
  event->source_origin = source_origin;
  if (!std::move(prepare_callback).Run(&event)) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    return;
  }

  int request_id;
  if (timeout) {
    request_id = worker->StartRequestWithCustomTimeout(
        ServiceWorkerMetrics::EventType::MESSAGE, std::move(callback), *timeout,
        ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
  } else {
    request_id = worker->StartRequest(ServiceWorkerMetrics::EventType::MESSAGE,
                                      std::move(callback));
  }
  worker->endpoint()->DispatchExtendableMessageEvent(
      std::move(event), worker->CreateSimpleEventCallback(request_id));
}

void StartWorkerToDispatchExtendableMessageEvent(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const std::optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    PrepareExtendableMessageEventCallback prepare_callback) {
  // If not enough time is left to actually process the event don't even
  // bother starting the worker and sending the event.
  if (timeout && *timeout < base::Milliseconds(100)) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorTimeout);
    return;
  }

  // Abort if redundant. This is not strictly needed since RunAfterStartWorker
  // does the same, but avoids logging UMA about failed startups.
  if (worker->is_redundant()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorRedundant);
    return;
  }

  // As we don't track tasks between workers and renderers, we can nullify the
  // message's parent task ID.
  message.parent_task_id = std::nullopt;

  worker->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::MESSAGE,
      base::BindOnce(&DispatchExtendableMessageEventAfterStartWorker, worker,
                     std::move(message), source_origin, timeout,
                     std::move(callback), std::move(prepare_callback)));
}

bool PrepareExtendableMessageEventFromClient(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int64_t registration_id,
    blink::mojom::ServiceWorkerClientInfoPtr source_client_info,
    blink::mojom::ExtendableMessageEventPtr* event) {
  if (!context) {
    return false;
  }
  DCHECK(source_client_info && !source_client_info->client_uuid.empty());
  (*event)->source_info_for_client = std::move(source_client_info);
  // Hide the client url if the client has a unique origin.
  if ((*event)->source_origin.opaque()) {
    (*event)->source_info_for_client->url = GURL();
  }

  // Reset |registration->self_update_delay| iff postMessage is coming from a
  // client, to prevent workers from postMessage to another version to reset
  // the delay (https://crbug.com/805496).
  scoped_refptr<ServiceWorkerRegistration> registration =
      context->GetLiveRegistration(registration_id);
  DCHECK(registration) << "running workers should have a live registration";
  registration->set_self_update_delay(base::TimeDelta());

  return true;
}

// The output |event| must be sent over Mojo immediately after this function
// returns. See ServiceWorkerObjectHost::CreateCompleteObjectInfoToSend() for
// details.
bool PrepareExtendableMessageEventFromServiceWorker(
    scoped_refptr<ServiceWorkerVersion> worker,
    base::WeakPtr<ServiceWorkerContainerHostForServiceWorker>
        source_container_host,
    blink::mojom::ExtendableMessageEventPtr* event) {
  // The service worker execution context may have been destroyed by the time we
  // get here.
  if (!source_container_host) {
    return false;
  }

  blink::mojom::ServiceWorkerObjectInfoPtr source_worker_info;
  base::WeakPtr<ServiceWorkerObjectHost> service_worker_object_host =
      worker->worker_host()
          ->container_host()
          ->version_object_manager()
          .GetOrCreateHost(
              source_container_host->service_worker_host()->version());
  if (service_worker_object_host) {
    // CreateCompleteObjectInfoToSend() is safe because |source_worker_info|
    // will be sent immediately by the caller of this function.
    source_worker_info =
        service_worker_object_host->CreateCompleteObjectInfoToSend();
  }

  (*event)->source_info_for_service_worker = std::move(source_worker_info);
  // Hide the service worker url if the service worker has a unique origin.
  if ((*event)->source_origin.opaque()) {
    (*event)->source_info_for_service_worker->url = GURL();
  }
  return true;
}

void DispatchExtendableMessageEventFromClient(
    base::WeakPtr<ServiceWorkerContextCore> context,
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    StatusCallback callback,
    blink::mojom::ServiceWorkerClientInfoPtr source_client_info) {
  if (!context) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  // |source_client_info| may be null if a client sent the message but its
  // info could not be retrieved.
  if (!source_client_info) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    return;
  }

  StartWorkerToDispatchExtendableMessageEvent(
      worker, std::move(message), source_origin, std::nullopt /* timeout */,
      std::move(callback),
      base::BindOnce(&PrepareExtendableMessageEventFromClient, context,
                     worker->registration_id(), std::move(source_client_info)));
}

void DispatchExtendableMessageEventFromServiceWorker(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const std::optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    base::WeakPtr<ServiceWorkerContainerHostForServiceWorker>
        source_container_host) {
  if (!source_container_host) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    return;
  }

  StartWorkerToDispatchExtendableMessageEvent(
      worker, std::move(message), source_origin, timeout, std::move(callback),
      base::BindOnce(&PrepareExtendableMessageEventFromServiceWorker, worker,
                     std::move(source_container_host)));
}

}  // namespace

void ServiceWorkerContainerHostForServiceWorker::DispatchExtendableMessageEvent(
    scoped_refptr<ServiceWorkerVersion> version,
    ::blink::TransferableMessage message,
    StatusCallback callback) {
  // Clamp timeout to the sending worker's remaining timeout, to prevent
  // postMessage from keeping workers alive forever.
  base::TimeDelta timeout =
      service_worker_host()->version()->remaining_timeout();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DispatchExtendableMessageEventFromServiceWorker,
                     std::move(version), std::move(message),
                     url::Origin::Create(url()), std::make_optional(timeout),
                     std::move(callback), base::AsWeakPtr(this)));
}

void ServiceWorkerContainerHostForClient::DispatchExtendableMessageEvent(
    scoped_refptr<ServiceWorkerVersion> version,
    ::blink::TransferableMessage message,
    StatusCallback callback) {
  if (service_worker_client().IsContainerForWindowClient()) {
    service_worker_client_utils::GetClient(
        &service_worker_client(),
        base::BindOnce(&DispatchExtendableMessageEventFromClient, context(),
                       std::move(version), std::move(message),
                       url::Origin::Create(url()), std::move(callback)));
  } else {
    DCHECK(service_worker_client().IsContainerForWorkerClient());

    // Web workers don't yet have access to ServiceWorker objects, so they
    // can't postMessage to one (https://crbug.com/371690).
    NOTREACHED();
  }
}

void ServiceWorkerContainerHostForClient::Update(
    scoped_refptr<ServiceWorkerRegistration> registration,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback
        callback) {
  // Don't delay update() if called by non-ServiceWorkers.
  registration->ExecuteUpdate(std::move(outside_fetch_client_settings_object),
                              std::move(callback));
}

void ServiceWorkerContainerHostForServiceWorker::Update(
    scoped_refptr<ServiceWorkerRegistration> registration,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback
        callback) {
  ServiceWorkerVersion* version = service_worker_host()->version();
  DCHECK(version);
  registration->DelayUpdate(*version,
                            std::move(outside_fetch_client_settings_object),
                            std::move(callback));
}

// If a blob URL is used for a SharedWorker script's URL, a controller will be
// inherited.
BASE_FEATURE(kSharedWorkerBlobURLFix,
             "SharedWorkerBlobURLFix",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace content
