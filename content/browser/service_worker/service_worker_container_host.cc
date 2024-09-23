// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_container_host.h"

#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace content {

namespace {

ServiceWorkerMetrics::EventType PurposeToEventType(
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  switch (purpose) {
    case blink::mojom::ControllerServiceWorkerPurpose::FETCH_SUB_RESOURCE:
      return ServiceWorkerMetrics::EventType::FETCH_SUB_RESOURCE;
  }
  NOTREACHED_IN_MIGRATION();
  return ServiceWorkerMetrics::EventType::UNKNOWN;
}

}  // namespace

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

ServiceWorkerContainerHost::~ServiceWorkerContainerHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}
ServiceWorkerContainerHostForClient::~ServiceWorkerContainerHostForClient() =
    default;
ServiceWorkerContainerHostForServiceWorker::
    ~ServiceWorkerContainerHostForServiceWorker() = default;

ServiceWorkerContainerHostForClient::ServiceWorkerContainerHostForClient(
    base::PassKey<ServiceWorkerClient>,
    base::WeakPtr<ServiceWorkerClient> service_worker_client,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr& container_info,
    const PolicyContainerPolicies& policy_container_policies,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    ukm::SourceId ukm_source_id)
    : service_worker_client_(std::move(service_worker_client)),
      container_(
          container_info->client_receiver.InitWithNewEndpointAndPassRemote()),
      ukm_source_id_(std::move(ukm_source_id)),
      policy_container_policies_(policy_container_policies.Clone()),
      coep_reporter_(std::move(coep_reporter)) {
  CHECK(container_.is_bound());
  CHECK(service_worker_client_);
  CHECK(!service_worker_client_->is_response_committed());

  service_worker_client_->owner().BindHost(
      *this, container_info->host_remote.InitWithNewEndpointAndPassReceiver());
}

bool ServiceWorkerContainerHostForClient::IsContainerRemoteConnected() const {
  return container_.is_connected();
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
                     weak_ptr_factory_.GetWeakPtr(), GURL(script_url),
                     GURL(options->scope), std::move(wrapped_callback),
                     trace_id, mojo::GetBadMessageCallback()),
      global_frame_id, policy_container_policies_);
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
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), trace_id));
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
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), trace_id));
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

  // TODO(kinuko): Log the reasons we drop the request.
  if (!context() || !controller()) {
    return;
  }

  controller()->RunAfterStartWorker(
      PurposeToEventType(purpose),
      base::BindOnce(
          &ServiceWorkerContainerHostForClient::StartControllerComplete,
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

void ServiceWorkerContainerHostForClient::EnsureFileAccess(
    const std::vector<base::FilePath>& file_paths,
    EnsureFileAccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_worker_client().EnsureFileAccess(file_paths, std::move(callback));
}

void ServiceWorkerContainerHostForClient::OnExecutionReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_worker_client().OnExecutionReady();
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
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ServiceWorkerContainerHostForClient::PostMessageToClient(
    ServiceWorkerVersion& version,
    blink::TransferableMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(service_worker_client().is_execution_ready());

  blink::mojom::ServiceWorkerObjectInfoPtr info;
  if (base::WeakPtr<ServiceWorkerObjectHost> object_host =
          version_object_manager().GetOrCreateHost(&version)) {
    info = object_host->CreateCompleteObjectInfoToSend();
  }
  container_->PostMessageToClient(std::move(info), std::move(message));
}

void ServiceWorkerContainerHostForClient::CountFeature(
    blink::mojom::WebFeature feature) {
  CHECK(container_.is_bound());
  CHECK(container_.is_connected());
  container_->CountFeature(feature);
}

blink::mojom::ControllerServiceWorkerInfoPtr
ServiceWorkerContainerHostForClient::CreateControllerServiceWorkerInfo() {
  CHECK(controller());

  auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
  controller_info->client_id = service_worker_client().client_uuid();
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
          service_worker_client().GetRunningStatusCallbackReceiver();
      controller_info->router_data->initial_running_status =
          controller()->running_status();
    }
  }

  // Note that |controller_info->remote_controller| is null if the controller
  // has no fetch event handler. In that case the renderer frame won't get the
  // controller pointer upon the navigation commit, and subresource loading will
  // not be intercepted. (It might get intercepted later if the controller
  // changes due to skipWaiting() so SetController is sent.)
  controller_info->remote_controller = GetRemoteControllerServiceWorker();

  if (service_worker_client().fetch_request_window_id()) {
    controller_info->fetch_request_window_id =
        std::make_optional(service_worker_client().fetch_request_window_id());
  }
  // Populate used features for UseCounter purposes.
  for (const auto feature : controller()->used_features()) {
    controller_info->used_features.push_back(feature);
  }

  // Set the info for the JavaScript ServiceWorkerContainer#controller object.
  if (base::WeakPtr<ServiceWorkerObjectHost> object_host =
          version_object_manager().GetOrCreateHost(controller())) {
    controller_info->object_info =
        object_host->CreateCompleteObjectInfoToSend();
  }

  return controller_info;
}

void ServiceWorkerContainerHostForClient::SendSetController(
    bool notify_controllerchange) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(service_worker_client().is_container_ready());

  if (!context() || !controller()) {
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
    controller_info->client_id = service_worker_client().client_uuid();
    container_->SetController(std::move(controller_info),
                              notify_controllerchange);
    return;
  }

  DCHECK(service_worker_client().controller_registration());
  DCHECK_EQ(service_worker_client().controller_registration()->active_version(),
            controller());

  // TODO(crbug.com/331279951): Remove these crash keys after investigation.
  SCOPED_CRASH_KEY_NUMBER(
      "SWCH_SC", "client_type",
      static_cast<int32_t>(service_worker_client().GetClientType()));
  SCOPED_CRASH_KEY_BOOL("SWCH_SC", "is_execution_ready",
                        service_worker_client().is_execution_ready());
  SCOPED_CRASH_KEY_BOOL("SWCH_SC", "is_bound", container_.is_bound());
  SCOPED_CRASH_KEY_BOOL("SWCH_SC", "is_connected",
                        container_.is_bound() && container_.is_connected());
  SCOPED_CRASH_KEY_BOOL("SWCH_SC", "notify_controllerchange",
                        notify_controllerchange);

  auto controller_info = CreateControllerServiceWorkerInfo();
  if (controller_info->fetch_request_window_id &&
      !controller_info->object_info) {
    // TODO(crbug.com/348109482) Remove these crash keys after fixing the issue.
    // When `controller_info` has `fetch_request_window_id` but doesn't have
    // `object_info`, it will hit CHECK crash in the renderer.
    static bool has_dumped_without_crashing = false;
    if (!has_dumped_without_crashing) {
      has_dumped_without_crashing = true;
      SCOPED_CRASH_KEY_STRING256(
          "SWController", "fetch_request_window_id",
          controller_info->fetch_request_window_id->ToString());
      SCOPED_CRASH_KEY_BOOL("SWController", "has_context_core", !!context());
      SCOPED_CRASH_KEY_NUMBER("SWController", "mode",
                              static_cast<int>(controller_info->mode));
      SCOPED_CRASH_KEY_NUMBER(
          "SWController", "client_type",
          static_cast<int>(service_worker_client().GetClientType()));
      SCOPED_CRASH_KEY_BOOL(
          "SWController", "PlzDedicatedWorker",
          base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
      base::debug::DumpWithoutCrashing();
    }
  }

  container_->SetController(std::move(controller_info),
                            notify_controllerchange);
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

mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
ServiceWorkerContainerHostForClient::GetRemoteControllerServiceWorker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(controller());
  CHECK(service_worker_client().is_response_committed());
  if (controller()->fetch_handler_existence() ==
      ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST) {
    return mojo::PendingRemote<blink::mojom::ControllerServiceWorker>();
  }

  mojo::PendingRemote<blink::mojom::ControllerServiceWorker> remote_controller;
  CloneControllerServiceWorker(
      remote_controller.InitWithNewPipeAndPassReceiver());
  return remote_controller;
}

void ServiceWorkerContainerHostForClient::CloneControllerServiceWorker(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver) {
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_to_be_passed;
  if (coep_reporter_) {
    DCHECK(service_worker_client().IsContainerForWindowClient());
    coep_reporter_->Clone(
        coep_reporter_to_be_passed.InitWithNewPipeAndPassReceiver());
  } else {
    // TODO(crbug.com/41478971): Implement DedicatedWorker and
    // SharedWorker cases.
    DCHECK(service_worker_client().IsContainerForWorkerClient());
  }

  controller()->controller()->Clone(
      std::move(receiver),
      policy_container_policies_.cross_origin_embedder_policy,
      std::move(coep_reporter_to_be_passed));
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

const base::WeakPtr<ServiceWorkerContextCore>&
ServiceWorkerContainerHostForClient::context() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return service_worker_client().context();
}

base::WeakPtr<ServiceWorkerContainerHost>
ServiceWorkerContainerHostForClient::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
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

base::WeakPtr<ServiceWorkerContainerHost>
ServiceWorkerContainerHostForServiceWorker::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
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

void ServiceWorkerContainerHostForClient::StartControllerComplete(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(service_worker_client().is_response_committed());
    CloneControllerServiceWorker(std::move(receiver));
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
                     std::move(callback), weak_ptr_factory_.GetWeakPtr()));
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
    NOTREACHED_IN_MIGRATION();
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

ServiceWorkerVersion* ServiceWorkerContainerHostForClient::controller() const {
  return service_worker_client().controller();
}

}  // namespace content
