// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include <stddef.h>

#include <limits>
#include <map>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/services/storage/public/mojom/service_worker_database.mojom-forward.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/renderer_host/private_network_access_util.h"
#include "content/browser/service_worker/payment_handler_support.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_hid_delegate_observer.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_installed_scripts_sender.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/browser/service_worker/service_worker_usb_delegate_observer.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/result_codes.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "ipc/ipc_message.h"
#include "mojo/public/c/system/types.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"

namespace content {
namespace {

// Timeout for an installed worker to start.
constexpr base::TimeDelta kStartInstalledWorkerTimeout = base::Seconds(60);

const char kClaimClientsStateErrorMesage[] =
    "Only the active worker can claim clients.";

const char kClaimClientsShutdownErrorMesage[] =
    "Failed to claim clients due to Service Worker system shutdown.";

const char kNotRespondingErrorMesage[] = "Service Worker is not responding.";
const char kForceUpdateInfoMessage[] =
    "Service Worker was updated because \"Update on reload\" was "
    "checked in the DevTools Application panel.";

void RunSoon(base::OnceClosure callback) {
  if (callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
}

// An adapter to run a |callback| after StartWorker.
void RunCallbackAfterStartWorker(base::WeakPtr<ServiceWorkerVersion> version,
                                 ServiceWorkerVersion::StatusCallback callback,
                                 blink::ServiceWorkerStatusCode status) {
  if (status == blink::ServiceWorkerStatusCode::kOk &&
      version->running_status() != blink::EmbeddedWorkerStatus::kRunning) {
    // We've tried to start the worker (and it has succeeded), but
    // it looks it's not running yet.
    NOTREACHED_IN_MIGRATION()
        << "The worker's not running after successful StartWorker";
    std::move(callback).Run(
        blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed);
    return;
  }
  std::move(callback).Run(status);
}

void ClearTick(base::TimeTicks* time) {
  *time = base::TimeTicks();
}

const int kInvalidTraceId = -1;

int NextTraceId() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static int trace_id = 0;
  if (trace_id == std::numeric_limits<int>::max())
    trace_id = 0;
  else
    ++trace_id;
  DCHECK_NE(kInvalidTraceId, trace_id);
  return trace_id;
}

void OnConnectionError(base::WeakPtr<EmbeddedWorkerInstance> embedded_worker) {
  if (!embedded_worker)
    return;

  switch (embedded_worker->status()) {
    case blink::EmbeddedWorkerStatus::kStarting:
    case blink::EmbeddedWorkerStatus::kRunning:
      // In this case the disconnection might be happening because of sudden
      // renderer shutdown like crash.
      embedded_worker->Detach();
      break;
    case blink::EmbeddedWorkerStatus::kStopping:
    case blink::EmbeddedWorkerStatus::kStopped:
      // Do nothing
      break;
  }
}

void OnOpenWindowFinished(
    blink::mojom::ServiceWorkerHost::OpenNewTabCallback callback,
    blink::ServiceWorkerStatusCode status,
    blink::mojom::ServiceWorkerClientInfoPtr client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const bool success = (status == blink::ServiceWorkerStatusCode::kOk);
  std::optional<std::string> error_msg;
  if (!success) {
    DCHECK(!client_info);
    error_msg.emplace("Something went wrong while trying to open the window.");
  }
  std::move(callback).Run(success, std::move(client_info), error_msg);
}

void DidShowPaymentHandlerWindow(
    const GURL& url,
    const blink::StorageKey& key,
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    blink::mojom::ServiceWorkerHost::OpenPaymentHandlerWindowCallback callback,
    bool success,
    int render_process_id,
    int render_frame_id) {
  if (success) {
    service_worker_client_utils::DidNavigate(
        context, url, key,
        base::BindOnce(&OnOpenWindowFinished, std::move(callback)),
        GlobalRenderFrameHostId(render_process_id, render_frame_id));
  } else {
    OnOpenWindowFinished(std::move(callback),
                         blink::ServiceWorkerStatusCode::kErrorFailed,
                         nullptr /* client_info */);
  }
}

void DidNavigateClient(
    blink::mojom::ServiceWorkerHost::NavigateClientCallback callback,
    const GURL& url,
    blink::ServiceWorkerStatusCode status,
    blink::mojom::ServiceWorkerClientInfoPtr client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const bool success = (status == blink::ServiceWorkerStatusCode::kOk);
  std::optional<std::string> error_msg;
  if (!success) {
    DCHECK(!client);
    error_msg.emplace("Cannot navigate to URL: " + url.spec());
  }
  std::move(callback).Run(success, std::move(client), error_msg);
}

const char* FetchHandlerTypeToSuffix(
    ServiceWorkerVersion::FetchHandlerType type) {
  switch (type) {
    case ServiceWorkerVersion::FetchHandlerType::kNoHandler:
      return "_NO_HANDLER";
    case ServiceWorkerVersion::FetchHandlerType::kNotSkippable:
      return "_NOT_SKIPPABLE";
    case ServiceWorkerVersion::FetchHandlerType::kEmptyFetchHandler:
      return "_EMPTY_FETCH_HANDLER";
  }
}

// This function merges SHA256 checksum hash strings in
// ServiceWokrerResourceRecord and return a single hash string.
std::optional<std::string> MergeResourceRecordSHA256ScriptChecksum(
    const GURL& main_script_url,
    const ServiceWorkerScriptCacheMap& script_cache_map,
    std::optional<blink::mojom::ServiceWorkerFetchHandlerType>
        fetch_handler_type) {
  const std::unique_ptr<crypto::SecureHash> checksum =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources =
      script_cache_map.GetResources();
  // Sort |resources| by |sha256_checksum| value not to make the merged value
  // inconsistent based on the script order.
  std::sort(resources.begin(), resources.end(),
            [](const storage::mojom::ServiceWorkerResourceRecordPtr& record1,
               const storage::mojom::ServiceWorkerResourceRecordPtr& record2) {
              if (record1->sha256_checksum && record2->sha256_checksum) {
                return *record1->sha256_checksum < *record2->sha256_checksum;
              }
              return record1->sha256_checksum.has_value();
            });

  for (auto& resource : resources) {
    if (!resource->sha256_checksum) {
      return std::nullopt;
    }
    // This may not be the case because we use the fixed length string, but
    // insert a delimiter here to distinguish following cases to avoid hash
    // value collisions: ab,cdef vs abcd,ef
    const std::string checksum_with_delimiter =
        *resource->sha256_checksum + "|";
    checksum->Update(checksum_with_delimiter.data(),
                     checksum_with_delimiter.size());
  }

  uint8_t result[crypto::kSHA256Length];
  checksum->Finish(result, crypto::kSHA256Length);
  const std::string encoded = base::HexEncode(result);

  if (fetch_handler_type) {
    DVLOG(3) << "Updated ServiceWorker script checksum. script_url:"
             << main_script_url.spec() << ", checksum:" << encoded
             << ", fetch_handler_type:" << fetch_handler_type.value();
  }

  return encoded;
}

storage::mojom::CacheStorageControl* GetCacheStorageControl(
    ServiceWorkerVersion& version) {
  if (!version.context()) {
    return nullptr;
  }
  auto* storage_partition = version.context()->wrapper()->storage_partition();
  if (!storage_partition) {
    return nullptr;
  }
  auto* control = storage_partition->GetCacheStorageControl();
  if (!control) {
    return nullptr;
  }
  return control;
}

}  // namespace

constexpr base::TimeDelta ServiceWorkerVersion::kTimeoutTimerDelay;
constexpr base::TimeDelta ServiceWorkerVersion::kStartNewWorkerTimeout;
constexpr base::TimeDelta ServiceWorkerVersion::kStopWorkerTimeout;

ServiceWorkerVersion::MainScriptResponse::MainScriptResponse(
    const network::mojom::URLResponseHead& response_head) {
  response_time = response_head.response_time;
  if (response_head.headers) {
    std::optional<base::Time> value =
        response_head.headers->GetLastModifiedValue();
    if (value) {
      last_modified = value.value();
    }
  }
  headers = response_head.headers;
  if (response_head.ssl_info.has_value())
    ssl_info = response_head.ssl_info.value();
}

ServiceWorkerVersion::MainScriptResponse::~MainScriptResponse() = default;

void ServiceWorkerVersion::RestartTick(base::TimeTicks* time) const {
  *time = tick_clock_->NowTicks();
}

bool ServiceWorkerVersion::RequestExpired(
    const base::TimeTicks& expiration_time) const {
  if (expiration_time.is_null()) {
    return false;
  }
  return tick_clock_->NowTicks() >= expiration_time;
}

base::TimeDelta ServiceWorkerVersion::GetTickDuration(
    const base::TimeTicks& time) const {
  if (time.is_null())
    return base::TimeDelta();
  return tick_clock_->NowTicks() - time;
}

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    const GURL& script_url,
    blink::mojom::ScriptType script_type,
    int64_t version_id,
    mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
        remote_reference,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : version_id_(version_id),
      registration_id_(registration->id()),
      script_url_(script_url),
      key_(registration->key()),
      scope_(registration->scope()),
      script_type_(script_type),
      registration_status_(registration->status()),
      ancestor_frame_type_(registration->ancestor_frame_type()),
      context_(context),
      script_cache_map_(this, context),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      clock_(base::DefaultClock::GetInstance()),
      ping_controller_(this),
      remote_reference_(std::move(remote_reference)),
      ukm_source_id_(ukm::ConvertToSourceId(ukm::AssignNewSourceId(),
                                            ukm::SourceIdType::WORKER_ID)),
      reporting_source_(base::UnguessableToken::Create()) {
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId, version_id);
  DCHECK(context_);
  DCHECK(registration);
  DCHECK(script_url_.is_valid());
  embedded_worker_ = std::make_unique<EmbeddedWorkerInstance>(this);
  embedded_worker_->AddObserver(this);
  context_->AddLiveVersion(this);
}

ServiceWorkerVersion::~ServiceWorkerVersion() {
  // TODO(falken): Investigate whether this can be removed. The destructor used
  // to be more complicated and could result in various methods being called.
  in_dtor_ = true;

  // One way we get here is if the user closed the tab before the SW
  // could start up.
  std::vector<StatusCallback> start_callbacks;
  start_callbacks.swap(start_callbacks_);
  for (auto& callback : start_callbacks) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
  }

  // One way we get here is if the user closed the tab before the SW
  // could warm up.
  std::vector<StatusCallback> warm_up_callbacks;
  warm_up_callbacks.swap(warm_up_callbacks_);
  for (auto& callback : warm_up_callbacks) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
  }

  if (context_)
    context_->RemoveLiveVersion(version_id_);

  embedded_worker_->RemoveObserver(this);
}

void ServiceWorkerVersion::SetNavigationPreloadState(
    const blink::mojom::NavigationPreloadState& state) {
  navigation_preload_state_ = state;
}

void ServiceWorkerVersion::SetRegistrationStatus(
    ServiceWorkerRegistration::Status registration_status) {
  registration_status_ = registration_status;
}

void ServiceWorkerVersion::SetStatus(Status status) {
  if (status_ == status)
    return;

  TRACE_EVENT2("ServiceWorker", "ServiceWorkerVersion::SetStatus", "Script URL",
               script_url_.spec(), "New Status", VersionStatusToString(status));

  // |fetch_handler_type_| must be set before setting the status to
  // INSTALLED,
  // ACTIVATING or ACTIVATED.
  DCHECK(fetch_handler_type_ ||
         !(status == INSTALLED || status == ACTIVATING || status == ACTIVATED));

  status_ = status;
  if (skip_waiting_) {
    switch (status_) {
      case NEW:
        // |skip_waiting_| should not be set before the version is NEW.
        NOTREACHED_IN_MIGRATION();
        return;
      case INSTALLING:
        // Do nothing until INSTALLED time.
        break;
      case INSTALLED:
        // Start recording the time when the version is trying to skip waiting.
        RestartTick(&skip_waiting_time_);
        break;
      case ACTIVATING:
        // Do nothing until ACTIVATED time.
        break;
      case ACTIVATED:
        // Resolve skip waiting promises.
        ClearTick(&skip_waiting_time_);
        for (SkipWaitingCallback& callback : pending_skip_waiting_requests_) {
          std::move(callback).Run(true);
        }
        pending_skip_waiting_requests_.clear();
        break;
      case REDUNDANT:
        // Fail any pending skip waiting requests since this version is dead.
        for (SkipWaitingCallback& callback : pending_skip_waiting_requests_) {
          std::move(callback).Run(false);
        }
        pending_skip_waiting_requests_.clear();
        break;
    }
  }

  // OnVersionStateChanged() invokes updates of the status using state
  // change IPC at ServiceWorkerObjectHost (for JS-land on renderer process) and
  // ServiceWorkerContextCore (for devtools and serviceworker-internals).
  // This should be done before using the new status by
  // |status_change_callbacks_| which sends the IPC for resolving the .ready
  // property.
  // TODO(shimazu): Clarify the dependency of OnVersionStateChanged and
  // |status_change_callbacks_|
  for (auto& observer : observers_)
    observer.OnVersionStateChanged(this);

  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(status_change_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run();

  if (status == INSTALLED) {
    embedded_worker_->OnWorkerVersionInstalled();
  } else if (status == ACTIVATED) {
#if !BUILDFLAG(IS_ANDROID)
    // Notify the hid delegate observer if the active service worker has any hid
    // event handlers.
    context_->hid_delegate_observer()->UpdateHasEventHandlers(
        registration_id_, has_hid_event_handlers_);

    // Notify the usb delegate observer if the active service worker has any usb
    // event handlers.
    context_->usb_delegate_observer()->UpdateHasEventHandlers(
        registration_id_, has_usb_event_handlers_);
#endif  // !BUILDFLAG(IS_ANDROID)
  } else if (status == REDUNDANT) {
    embedded_worker_->OnWorkerVersionDoomed();

    // Drop the remote reference to tell the storage system that the worker
    // script resources can now be deleted.
    remote_reference_.reset();
  }
}

void ServiceWorkerVersion::RegisterStatusChangeCallback(
    base::OnceClosure callback) {
  status_change_callbacks_.push_back(std::move(callback));
}

ServiceWorkerVersionInfo ServiceWorkerVersion::GetInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::optional<std::string> router_rules;
  if (router_evaluator_) {
    router_rules = router_evaluator_->ToString();
  }
  ServiceWorkerVersionInfo info(
      running_status(), status(), fetch_handler_type_, script_url(), scope(),
      key(), registration_id(), version_id(), embedded_worker()->process_id(),
      embedded_worker()->thread_id(),
      embedded_worker()->worker_devtools_agent_route_id(), ukm_source_id(),
      ancestor_frame_type_, router_rules);
  for (const auto& controllee : controllee_map_) {
    ServiceWorkerClient* service_worker_client = controllee.second.get();
    info.clients.emplace(service_worker_client->client_uuid(),
                         service_worker_client->GetServiceWorkerClientInfo());
  }

  info.script_response_time = script_response_time_for_devtools_;
  if (!main_script_response_)
    return info;

  // If the service worker hasn't started, then |main_script_response_| is not
  // set, so we use |script_response_time_for_devtools_| to populate |info|. If
  // the worker has started, this value should match with the timestamp stored
  // in |main_script_response_|.
  DCHECK_EQ(info.script_response_time, main_script_response_->response_time);
  info.script_last_modified = main_script_response_->last_modified;

  return info;
}

ServiceWorkerVersion::FetchHandlerExistence
ServiceWorkerVersion::fetch_handler_existence() const {
  if (!fetch_handler_type_) {
    return FetchHandlerExistence::UNKNOWN;
  }
  return (fetch_handler_type_ == FetchHandlerType::kNoHandler)
             ? FetchHandlerExistence::DOES_NOT_EXIST
             : FetchHandlerExistence::EXISTS;
}

ServiceWorkerVersion::FetchHandlerType
ServiceWorkerVersion::fetch_handler_type() const {
  DCHECK(fetch_handler_type_);
  return fetch_handler_type_ ? *fetch_handler_type_
                             : FetchHandlerType::kNoHandler;
}

void ServiceWorkerVersion::set_fetch_handler_type(
    FetchHandlerType fetch_handler_type) {
  DCHECK(!fetch_handler_type_);
  fetch_handler_type_ = fetch_handler_type;
}

void ServiceWorkerVersion::set_has_hid_event_handlers(
    bool has_hid_event_handlers) {
  has_hid_event_handlers_ = has_hid_event_handlers;
}

void ServiceWorkerVersion::set_has_usb_event_handlers(
    bool has_usb_event_handlers) {
  has_usb_event_handlers_ = has_usb_event_handlers;
}

void ServiceWorkerVersion::StartWorker(ServiceWorkerMetrics::EventType purpose,
                                       StatusCallback callback) {
  TRACE_EVENT_INSTANT2(
      "ServiceWorker", "ServiceWorkerVersion::StartWorker (instant)",
      TRACE_EVENT_SCOPE_THREAD, "Script", script_url_.spec(), "Purpose",
      ServiceWorkerMetrics::EventTypeToString(purpose));

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const bool is_browser_startup_complete =
      GetContentClient()->browser()->IsBrowserStartupComplete();
  if (!context_) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            blink::ServiceWorkerStatusCode::kErrorAbort);
    RunSoon(base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  if (is_redundant()) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            blink::ServiceWorkerStatusCode::kErrorRedundant);
    RunSoon(base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorRedundant));
    return;
  }
  if (!IsStartWorkerAllowed()) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            blink::ServiceWorkerStatusCode::kErrorDisallowed);
    RunSoon(base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorDisallowed));
    return;
  }

  if (is_running_start_callbacks_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceWorkerVersion::StartWorker,
                                  weak_factory_.GetWeakPtr(), purpose,
                                  std::move(callback)));
    return;
  }

  // Ensure the live registration during starting worker so that the worker can
  // get associated with it in
  // ServiceWorkerHost::CompleteStartWorkerPreparation.
  context_->registry()->FindRegistrationForId(
      registration_id_, key_,
      base::BindOnce(
          &ServiceWorkerVersion::DidEnsureLiveRegistrationForStartWorker,
          weak_factory_.GetWeakPtr(), purpose, status_,
          is_browser_startup_complete, std::move(callback)));
}

void ServiceWorkerVersion::StopWorker(base::OnceClosure callback) {
  TRACE_EVENT_INSTANT2("ServiceWorker",
                       "ServiceWorkerVersion::StopWorker (instant)",
                       TRACE_EVENT_SCOPE_THREAD, "Script", script_url_.spec(),
                       "Status", VersionStatusToString(status_));

  switch (running_status()) {
    case blink::EmbeddedWorkerStatus::kStarting:
    case blink::EmbeddedWorkerStatus::kRunning: {
      // EmbeddedWorkerInstance::Stop() may synchronously call
      // ServiceWorkerVersion::OnStopped() and destroy |this|. This protection
      // avoids it.
      scoped_refptr<ServiceWorkerVersion> protect = this;
      embedded_worker_->Stop();
      if (running_status() == blink::EmbeddedWorkerStatus::kStopped) {
        RunSoon(std::move(callback));
        return;
      }
      stop_callbacks_.push_back(std::move(callback));

      // Protect |this| until Stop() correctly finished. Otherwise the
      // |stop_callbacks_| might not be called. The destruction of |this| could
      // happen before the message OnStopped() when the final
      // ServiceWorkerObjectHost is destructed because of the termination.
      // Note that this isn't necessary to be the final element of
      // |stop_callbacks_| because there's another logic to protect |this| when
      // calling |stop_callbacks_|.
      stop_callbacks_.push_back(base::BindOnce(
          [](scoped_refptr<content::ServiceWorkerVersion>) {}, protect));
      return;
    }
    case blink::EmbeddedWorkerStatus::kStopping:
      stop_callbacks_.push_back(std::move(callback));
      return;
    case blink::EmbeddedWorkerStatus::kStopped:
      RunSoon(std::move(callback));
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void ServiceWorkerVersion::TriggerIdleTerminationAsap() {
  needs_to_be_terminated_asap_ = true;
  endpoint()->SetIdleDelay(base::Seconds(0));
}

bool ServiceWorkerVersion::OnRequestTermination() {
  if (running_status() == blink::EmbeddedWorkerStatus::kStopping) {
    return true;
  }
  DCHECK_EQ(blink::EmbeddedWorkerStatus::kRunning, running_status());

  worker_is_idle_on_renderer_ = true;

  // Determine if the worker can be terminated.
  bool will_be_terminated = HasNoWork();
  if (embedded_worker_->devtools_attached()) {
    // Basically the service worker won't be terminated if DevTools is attached.
    // But when activation is happening and this worker needs to be terminated
    // asap, it'll be terminated.
    will_be_terminated = needs_to_be_terminated_asap_;

    if (!will_be_terminated) {
      // When the worker is being kept alive due to devtools, it's important to
      // set the service worker's idle delay back to the default value rather
      // than zero. Otherwise, the service worker might see that it has no work
      // and immediately send a RequestTermination() back to the browser again,
      // repeating this over and over. In the non-devtools case, it's
      // necessarily being kept alive due to an inflight request, and will only
      // send a RequestTermination() once that request settles (which is the
      // intended behavior).
      endpoint()->SetIdleDelay(
          base::Seconds(blink::mojom::kServiceWorkerDefaultIdleDelayInSeconds));
    }
  }

  static const bool kSpeculativeServiceWorkerWarmUpOnIdleTimeoutEnabled =
      base::FeatureList::IsEnabled(
          blink::features::kSpeculativeServiceWorkerWarmUp) &&
      blink::features::kSpeculativeServiceWorkerWarmUpOnIdleTimeout.Get();
  will_warm_up_on_stopped_ =
      will_be_terminated &&
      kSpeculativeServiceWorkerWarmUpOnIdleTimeoutEnabled &&
      scope_.SchemeIsHTTPOrHTTPS();

  if (will_be_terminated) {
    embedded_worker_->Stop();
  } else {
    // The worker needs to run more. The worker should start handling queued
    // events dispatched to the worker directly (e.g. FetchEvent for
    // subresources).
    worker_is_idle_on_renderer_ = false;
  }

  return will_be_terminated;
}

void ServiceWorkerVersion::ScheduleUpdate() {
  if (!context_)
    return;
  if (update_timer_.IsRunning()) {
    update_timer_.Reset();
    return;
  }
  if (is_update_scheduled_)
    return;
  is_update_scheduled_ = true;

  // Protect |this| until the timer fires, since we may be stopping
  // and soon no one might hold a reference to us.
  context_->ProtectVersion(base::WrapRefCounted(this));

  update_timer_.Start(FROM_HERE, ServiceWorkerContext::GetUpdateDelay(),
                      base::BindOnce(&ServiceWorkerVersion::StartUpdate,
                                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::StartUpdate() {
  if (!context_)
    return;
  context_->registry()->FindRegistrationForId(
      registration_id_, key_,
      base::BindOnce(&ServiceWorkerVersion::FoundRegistrationForUpdate,
                     weak_factory_.GetWeakPtr()));
}

int ServiceWorkerVersion::StartRequest(
    ServiceWorkerMetrics::EventType event_type,
    StatusCallback error_callback) {
  return StartRequestWithCustomTimeout(event_type, std::move(error_callback),
                                       kRequestTimeout, KILL_ON_TIMEOUT);
}

int ServiceWorkerVersion::StartRequestWithCustomTimeout(
    ServiceWorkerMetrics::EventType event_type,
    StatusCallback error_callback,
    const base::TimeDelta& timeout,
    TimeoutBehavior timeout_behavior) {
  DCHECK(blink::EmbeddedWorkerStatus::kRunning == running_status() ||
         blink::EmbeddedWorkerStatus::kStarting == running_status())
      << "Can only start a request with a running or starting worker.";
  DCHECK(event_type == ServiceWorkerMetrics::EventType::INSTALL ||
         event_type == ServiceWorkerMetrics::EventType::ACTIVATE ||
         event_type == ServiceWorkerMetrics::EventType::MESSAGE ||
         event_type == ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST ||
         status() == ACTIVATED)
      << "Event of type " << static_cast<int>(event_type)
      << " can only be dispatched to an active worker: " << status();

  // |context_| is needed for some bookkeeping. If there's no context, the
  // request will be aborted soon, so don't bother aborting the request directly
  // here, and just skip this bookkeeping.
  if (context_) {
    if (event_type != ServiceWorkerMetrics::EventType::INSTALL &&
        event_type != ServiceWorkerMetrics::EventType::ACTIVATE &&
        event_type != ServiceWorkerMetrics::EventType::MESSAGE) {
      // Reset the self-update delay iff this is not an event that can triggered
      // by a service worker itself. Otherwise, service workers can use update()
      // to keep running forever via install and activate events, or
      // postMessage() between themselves to reset the delay via message event.
      // postMessage() resets the delay in ServiceWorkerObjectHost, iff it
      // didn't come from a service worker.
      scoped_refptr<ServiceWorkerRegistration> registration =
          context_->GetLiveRegistration(registration_id_);
      if (registration) {
        registration->set_self_update_delay(base::TimeDelta());
      }
    }
  }

  auto request = std::make_unique<InflightRequest>(
      std::move(error_callback), clock_->Now(), tick_clock_->NowTicks(),
      event_type);
  InflightRequest* request_rawptr = request.get();
  int request_id = inflight_requests_.Add(std::move(request));
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      "ServiceWorker", "ServiceWorkerVersion::Request",
      TRACE_ID_LOCAL(request_rawptr), "Request id", request_id, "Event type",
      ServiceWorkerMetrics::EventTypeToString(event_type));

  base::TimeTicks expiration_time = tick_clock_->NowTicks() + timeout;
  auto [iter, is_inserted] = request_timeouts_.emplace(
      request_id, event_type, expiration_time, timeout_behavior);
  DCHECK(is_inserted);
  request_rawptr->timeout_iter = iter;
  // TODO(crbug.com/40864997): remove the following DCHECK when the cause
  // identified.
  DCHECK_EQ(request_timeouts_.size(), inflight_requests_.size());
  if (expiration_time > max_request_expiration_time_)
    max_request_expiration_time_ = expiration_time;

  // Even if the worker is in the idle state, the new event which is about to
  // be dispatched will reset the idle status. That means the worker can receive
  // events directly from any client, so we cannot trigger OnNoWork after this
  // point.
  worker_is_idle_on_renderer_ = false;
  return request_id;
}

ServiceWorkerExternalRequestResult ServiceWorkerVersion::StartExternalRequest(
    const base::Uuid& request_uuid,
    ServiceWorkerExternalRequestTimeoutType timeout_type) {
  if (running_status() == blink::EmbeddedWorkerStatus::kStarting) {
    return pending_external_requests_.insert({request_uuid, timeout_type})
                   .second
               ? ServiceWorkerExternalRequestResult::kOk
               : ServiceWorkerExternalRequestResult::kBadRequestId;
  }

  if (running_status() == blink::EmbeddedWorkerStatus::kStopping ||
      running_status() == blink::EmbeddedWorkerStatus::kStopped) {
    return ServiceWorkerExternalRequestResult::kWorkerNotRunning;
  }

  if (base::Contains(external_request_uuid_to_request_id_, request_uuid))
    return ServiceWorkerExternalRequestResult::kBadRequestId;

  base::TimeDelta request_timeout =
      timeout_type == ServiceWorkerExternalRequestTimeoutType::kDefault
          ? kRequestTimeout
          : base::TimeDelta::Max();
  int request_id = StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST,
      base::BindOnce(&ServiceWorkerVersion::CleanUpExternalRequest, this,
                     request_uuid),
      request_timeout, CONTINUE_ON_TIMEOUT);
  external_request_uuid_to_request_id_[request_uuid] = request_id;

  // Cancel idle timeout when there is a new request started.
  // Idle timer will be scheduled when request finishes, if there is no other
  // requests and events.
  endpoint()->AddKeepAlive();

  return ServiceWorkerExternalRequestResult::kOk;
}

bool ServiceWorkerVersion::FinishRequest(int request_id, bool was_handled) {
  return FinishRequestWithFetchCount(request_id, was_handled,
                                     /*fetch_count=*/0);
}

bool ServiceWorkerVersion::FinishRequestWithFetchCount(int request_id,
                                                       bool was_handled,
                                                       uint32_t fetch_count) {
  InflightRequest* request = inflight_requests_.Lookup(request_id);
  if (!request)
    return false;
  ServiceWorkerMetrics::RecordEventDuration(
      request->event_type, tick_clock_->NowTicks() - request->start_time_ticks,
      was_handled, fetch_count);

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerVersion::Request", TRACE_ID_LOCAL(request),
      "Handled", was_handled);
  request_timeouts_.erase(request->timeout_iter);
  inflight_requests_.Remove(request_id);
  // TODO(crbug.com/40864997): remove the following DCHECK when the cause
  // identified.
  DCHECK_EQ(request_timeouts_.size(), inflight_requests_.size());

  if (!HasWorkInBrowser())
    OnNoWorkInBrowser();
  return true;
}

ServiceWorkerExternalRequestResult ServiceWorkerVersion::FinishExternalRequest(
    const base::Uuid& request_uuid) {
  if (running_status() == blink::EmbeddedWorkerStatus::kStarting) {
    auto iter = pending_external_requests_.find(request_uuid);
    if (iter == pending_external_requests_.end())
      return ServiceWorkerExternalRequestResult::kBadRequestId;
    pending_external_requests_.erase(iter);
    return ServiceWorkerExternalRequestResult::kOk;
  }

  // If it's STOPPED, there is no request to finish. We could just consider this
  // a success, but the caller may want to know about it. (If it's STOPPING,
  // proceed with finishing the request as normal.)
  if (running_status() == blink::EmbeddedWorkerStatus::kStopped) {
    return ServiceWorkerExternalRequestResult::kWorkerNotRunning;
  }

  auto iter = external_request_uuid_to_request_id_.find(request_uuid);
  if (iter != external_request_uuid_to_request_id_.end()) {
    int request_id = iter->second;
    external_request_uuid_to_request_id_.erase(iter);
    bool ok = FinishRequest(request_id, /*was_handled=*/true);

    // If an request is finished and there is no other requests, we ask event
    // queue to check if idle timeout should be scheduled. Event queue may
    // schedule idle timeout if there is no events at the time.
    // Also checks running status. Idle timeout is not meaningful if the worker
    // is stopping or stopped.
    if (ok && !HasWorkInBrowser() &&
        running_status() == blink::EmbeddedWorkerStatus::kRunning) {
      // If SW event queue request termination at this very moment, then SW can
      // be terminated before waiting for the next idle timeout. Details are
      // described in crbug/1399324.
      // TODO(richardzh): Complete crbug/1399324 which would resolve this issue.
      endpoint()->ClearKeepAlive();
    }

    return ok ? ServiceWorkerExternalRequestResult::kOk
              : ServiceWorkerExternalRequestResult::kBadRequestId;
  }

  // It is possible that the request was cancelled or timed out before and we
  // won't find it in |external_request_uuid_to_request_id_|. Just return
  // kOk.
  // TODO(falken): Consider keeping track of these so we can return
  // kBadRequestId for invalid requests ids.
  return ServiceWorkerExternalRequestResult::kOk;
}

ServiceWorkerVersion::SimpleEventCallback
ServiceWorkerVersion::CreateSimpleEventCallback(int request_id) {
  // The weak reference to |this| is safe because storage of the callbacks, the
  // inflight responses of blink::mojom::ServiceWorker messages, is owned by
  // |this|.
  return base::BindOnce(&ServiceWorkerVersion::OnSimpleEventFinished,
                        base::Unretained(this), request_id);
}

void ServiceWorkerVersion::RunAfterStartWorker(
    ServiceWorkerMetrics::EventType purpose,
    StatusCallback callback) {
  ServiceWorkerMetrics::RecordRunAfterStartWorkerStatus(running_status(),
                                                        purpose);
  if (running_status() == blink::EmbeddedWorkerStatus::kRunning) {
    DCHECK(start_callbacks_.empty());
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
    return;
  }
  StartWorker(purpose,
              base::BindOnce(&RunCallbackAfterStartWorker,
                             weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerVersion::AddControllee(
    ServiceWorkerClient* service_worker_client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(crbug.com/40657227): Remove this CHECK once we figure out the cause of
  // crash.
  CHECK(service_worker_client);
  const std::string& uuid = service_worker_client->client_uuid();
  CHECK(!service_worker_client->client_uuid().empty());
  // TODO(crbug.com/40657227): Change to DCHECK once we figure out the cause of
  // crash.
  CHECK(!base::Contains(controllee_map_, uuid));

  controllee_map_[uuid] = service_worker_client->AsWeakPtr();
  // Even if `context_` is invalid, `controllee_map_` should have `uuid`.
  // Otherwise, `Uncontrol()` may fail with CHECK. (crbug.com/357954498)
  // However, if `context_` is invalid, we may not need to raise a worker to
  // foreground priority or reset timer because this ServiceWorkerVersion is
  // shutting down.
  if (!context_) {
    return;
  }
  embedded_worker_->UpdateForegroundPriority();
  ClearTick(&no_controllees_time_);

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id_);
  if (registration) {
    registration->set_self_update_delay(base::TimeDelta());
  }

  // Notify observers asynchronously for consistency with RemoveControllee.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerVersion::NotifyControlleeAdded,
                     weak_factory_.GetWeakPtr(), uuid,
                     service_worker_client->GetServiceWorkerClientInfo()));

  // Also send a notification if OnEndNavigationCommit() was already invoked for
  // this container.
  if (service_worker_client->navigation_commit_ended()) {
    OnControlleeNavigationCommitted(
        service_worker_client->client_uuid(),
        service_worker_client->GetRenderFrameHostId());
  }
}

void ServiceWorkerVersion::RemoveControllee(const std::string& client_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(crbug.com/40653867): Remove this once RemoveControllee() matches with
  // AddControllee().
  if (!base::Contains(controllee_map_, client_uuid))
    return;

  controllee_map_.erase(client_uuid);

  embedded_worker_->UpdateForegroundPriority();

  // Notify observers asynchronously since this gets called during
  // ServiceWorkerHost's destructor, and we don't want observers to do work
  // during that.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerVersion::NotifyControlleeRemoved,
                                weak_factory_.GetWeakPtr(), client_uuid));
}

void ServiceWorkerVersion::OnControlleeNavigationCommitted(
    const std::string& client_uuid,
    const GlobalRenderFrameHostId& rfh_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if DCHECK_IS_ON()
  // Ensures this function is only called for a known window client.
  auto it = controllee_map_.find(client_uuid);
  CHECK(it != controllee_map_.end(), base::NotFatalUntil::M130);

  DCHECK_EQ(it->second->GetClientType(),
            blink::mojom::ServiceWorkerClientType::kWindow);
#endif  // DCHECK_IS_ON()

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerVersion::NotifyControlleeNavigationCommitted,
                     weak_factory_.GetWeakPtr(), client_uuid, rfh_id));
}

void ServiceWorkerVersion::MoveControlleeToBackForwardCacheMap(
    const std::string& client_uuid) {
  DCHECK(IsBackForwardCacheEnabled());
  CHECK(base::Contains(controllee_map_, client_uuid));
  CHECK(!base::Contains(bfcached_controllee_map_, client_uuid));
  bfcached_controllee_map_[client_uuid] = controllee_map_[client_uuid];
  RemoveControllee(client_uuid);
}

void ServiceWorkerVersion::RestoreControlleeFromBackForwardCacheMap(
    const std::string& client_uuid) {
  // TODO(crbug.com/40657227): Change these to DCHECK once we figure out the
  // cause of crash.
  CHECK(IsBackForwardCacheEnabled());
  CHECK(!base::Contains(controllee_map_, client_uuid));
  if (!base::Contains(bfcached_controllee_map_, client_uuid)) {
    // We are navigating to the page using BackForwardCache, which is being
    // evicted due to activation, postMessage or claim. In this case, we reload
    // the page without using BackForwardCache, so we can assume that
    // ContainerHost will be deleted soon.
    // TODO(crbug.com/40657227): Remove this CHECK once we fix the crash.
    CHECK(base::Contains(controllees_to_be_evicted_, client_uuid));
    // TODO(crbug.com/40657227): Remove DumpWithoutCrashing once we confirm the
    // cause of the crash.
    BackForwardCacheCanStoreDocumentResult can_store;
    can_store.No(controllees_to_be_evicted_.at(client_uuid));
    TRACE_EVENT(
        "navigation",
        "ServiceWorkerVersion::RestoreControlleeFromBackForwardCacheMap",
        ChromeTrackEvent::kBackForwardCacheCanStoreDocumentResult, can_store);
    SCOPED_CRASH_KEY_STRING32("RestoreForBFCache", "no_controllee_reason",
                              can_store.ToString());
    base::debug::DumpWithoutCrashing();
    return;
  }
  AddControllee(bfcached_controllee_map_.at(client_uuid).get());
  bfcached_controllee_map_.erase(client_uuid);
}

void ServiceWorkerVersion::RemoveControlleeFromBackForwardCacheMap(
    const std::string& client_uuid) {
  CHECK(IsBackForwardCacheEnabled());
  // TODO(crbug.com/341322515): Investigate why sometimes
  // `bfcache_controllee_map_` does not contain the client.
  SCOPED_CRASH_KEY_BOOL("ServiceWorkerBfcache", "in_controllee_map",
                        base::Contains(controllee_map_, client_uuid));
  CHECK(base::Contains(bfcached_controllee_map_, client_uuid));
  bfcached_controllee_map_.erase(client_uuid);
}

void ServiceWorkerVersion::Uncontrol(const std::string& client_uuid) {
  if (!IsBackForwardCacheEnabled()) {
    RemoveControllee(client_uuid);
  } else {
    if (base::Contains(controllee_map_, client_uuid)) {
      RemoveControllee(client_uuid);
    } else if (base::Contains(bfcached_controllee_map_, client_uuid)) {
      RemoveControlleeFromBackForwardCacheMap(client_uuid);
    } else {
      // It is possible that the controllee belongs to neither |controllee_map_|
      // or |bfcached_controllee_map_|. This happens when a BackForwardCached
      // controllee is deleted after eviction, which has already removed it from
      // |bfcached_controllee_map_|.
      // In this case, |controllees_to_be_evicted_| should contain the
      // controllee.
      // TODO(crbug.com/40657227): Remove this CHECK once we fix the crash.
      CHECK(base::Contains(controllees_to_be_evicted_, client_uuid));
      controllees_to_be_evicted_.erase(client_uuid);
    }
  }
}

void ServiceWorkerVersion::EvictBackForwardCachedControllees(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  DCHECK(IsBackForwardCacheEnabled());
  while (!bfcached_controllee_map_.empty()) {
    auto controllee = bfcached_controllee_map_.begin();
    EvictBackForwardCachedControllee(controllee->second.get(), reason);
  }
}

void ServiceWorkerVersion::EvictBackForwardCachedControllee(
    ServiceWorkerClient* controllee,
    BackForwardCacheMetrics::NotRestoredReason reason) {
  controllee->EvictFromBackForwardCache(reason);
  controllees_to_be_evicted_[controllee->client_uuid()] = reason;
  RemoveControlleeFromBackForwardCacheMap(controllee->client_uuid());
}

void ServiceWorkerVersion::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ServiceWorkerVersion::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ServiceWorkerVersion::ReportError(blink::ServiceWorkerStatusCode status,
                                       const std::string& status_message) {
  if (status_message.empty()) {
    OnReportException(
        base::UTF8ToUTF16(blink::ServiceWorkerStatusToString(status)), -1, -1,
        GURL());
  } else {
    OnReportException(base::UTF8ToUTF16(status_message), -1, -1, GURL());
  }
}

void ServiceWorkerVersion::ReportForceUpdateToDevTools() {
  AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                      kForceUpdateInfoMessage);
}

void ServiceWorkerVersion::SetStartWorkerStatusCode(
    blink::ServiceWorkerStatusCode status) {
  start_worker_status_ = status;
}

void ServiceWorkerVersion::Doom() {
  // Protect |this| because NotifyControllerLost() and Stop() callees
  // may drop references to |this|.
  scoped_refptr<ServiceWorkerVersion> protect(this);

  // Tell controllees that this version is dead. Each controllee will call
  // ServiceWorkerVersion::RemoveControllee(), so be careful with iterators.
  auto iter = controllee_map_.begin();
  while (iter != controllee_map_.end()) {
    ServiceWorkerClient* service_worker_client = iter->second.get();
    ++iter;
    service_worker_client->NotifyControllerLost();
  }
  // Tell the bfcached controllees that this version is dead. Each controllee
  // will call ServiceWorkerContainerHost:EvictFromBackForwardCache().
  // Called when this container host's controller has been terminated and
  // doomed. This can happen in several cases:
  // - A fatal error when trying to start the service worker, like an installed
  // script is unable to read from storage.
  // - The service worker was forcibly remoevd due to ClearSiteData or browser
  // setting.
  // - If this is a client in the back/forward cache, the service worker may
  // still be normally unregistered, because back/forward cached clients do not
  // count as true controllees for service worker lifecycle purposes.
  auto bf_iter = bfcached_controllee_map_.begin();
  while (bf_iter != bfcached_controllee_map_.end()) {
    ServiceWorkerClient* bf_service_worker_client = bf_iter->second.get();
    ++bf_iter;
    bf_service_worker_client->NotifyControllerLost();
  }

  // Any controllee this version had should have removed itself.
  DCHECK(!HasControllee());

  SetStatus(REDUNDANT);
  if (running_status() == blink::EmbeddedWorkerStatus::kStarting ||
      running_status() == blink::EmbeddedWorkerStatus::kRunning) {
    // |start_worker_status_| == kErrorExists means that this version was
    // created for update but the script was identical to the incumbent version.
    // In this case we should stop the worker immediately even when DevTools is
    // attached. Otherwise the redundant worker stays as a selectable context
    // in DevTools' console.
    // TODO(bashi): Remove this workaround when byte-for-byte update check is
    // shipped.
    bool stop_immediately =
        start_worker_status_ == blink::ServiceWorkerStatusCode::kErrorExists;
    if (stop_immediately || !embedded_worker()->devtools_attached()) {
      embedded_worker_->Stop();
    } else {
      stop_when_devtools_detached_ = true;
    }
  }

  // If we abort before transferring |main_script_load_params_| to the remote
  // worker service, we need to release it to avoid creating a reference loop
  // between ServiceWorker(New|Updated)ScriptLoader and this class.
  main_script_load_params_.reset();
}

void ServiceWorkerVersion::InitializeGlobalScope() {
  TRACE_EVENT0("ServiceWorker", "ServiceWorkerVersion::InitializeGlobalScope");
  receiver_.reset();
  associated_interface_receiver_.reset();
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerHost>
      service_worker_host;
  receiver_.Bind(service_worker_host.InitWithNewEndpointAndPassReceiver());

  mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
      associated_remote_from_browser;
  associated_interface_receiver_.Bind(
      associated_remote_from_browser.InitWithNewEndpointAndPassReceiver());

  mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
      associated_receiver_to_renderer;
  associated_interface_provider_ =
      std::make_unique<blink::AssociatedInterfaceProvider>(
          associated_receiver_to_renderer.InitWithNewEndpointAndPassRemote());

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id_);
  // The registration must exist since we keep a reference to it during
  // service worker startup.
  DCHECK(registration);
  CHECK(worker_host_);
  CHECK(worker_host_->container_host());
  DCHECK(service_worker_remote_);
  service_worker_remote_->InitializeGlobalScope(
      std::move(service_worker_host), std::move(associated_remote_from_browser),
      std::move(associated_receiver_to_renderer),
      worker_host_->container_host()->registration_object_manager().CreateInfo(
          std::move(registration)),
      worker_host_->container_host()->version_object_manager().CreateInfoToSend(
          this),
      fetch_handler_existence(), std::move(reporting_observer_receiver_),
      ancestor_frame_type_, key_);

  is_endpoint_ready_ = true;
  associated_registry_ = std::make_unique<blink::AssociatedInterfaceRegistry>();

  // If we have allocated the process we can tell the client to register
  // services.
  if (embedded_worker()->process_id() != ChildProcessHost::kInvalidUniqueID) {
    GetContentClient()
        ->browser()
        ->RegisterAssociatedInterfaceBindersForServiceWorker(
            GetInfo(), *associated_registry_);
  }
}

bool ServiceWorkerVersion::IsControlleeProcessID(int process_id) const {
  for (const auto& controllee : controllee_map_) {
    if (controllee.second && controllee.second->GetProcessId() == process_id)
      return true;
  }
  return false;
}

void ServiceWorkerVersion::ExecuteScriptForTest(
    const std::string& script,
    ServiceWorkerScriptExecutionCallback callback) {
  DCHECK(running_status() == blink::EmbeddedWorkerStatus::kStarting ||
         running_status() == blink::EmbeddedWorkerStatus::kRunning)
      << "Cannot execute a script in a non-running worker!";
  bool wants_result = !callback.is_null();
  endpoint()->ExecuteScriptForTest(  // IN-TEST
      base::UTF8ToUTF16(script), wants_result, std::move(callback));
}

bool ServiceWorkerVersion::IsWarmingUp() const {
  if (running_status() != blink::EmbeddedWorkerStatus::kStarting ||
      !embedded_worker_->pause_initializing_global_scope()) {
    return false;
  }
  return !warm_up_callbacks_.empty();
}

bool ServiceWorkerVersion::IsWarmedUp() const {
  if (running_status() != blink::EmbeddedWorkerStatus::kStarting ||
      !embedded_worker_->pause_initializing_global_scope()) {
    return false;
  }
  switch (embedded_worker_->starting_phase()) {
    case EmbeddedWorkerInstance::StartingPhase::NOT_STARTING:
    case EmbeddedWorkerInstance::StartingPhase::ALLOCATING_PROCESS:
    case EmbeddedWorkerInstance::StartingPhase::SENT_START_WORKER:
    case EmbeddedWorkerInstance::StartingPhase::SCRIPT_STREAMING:
    case EmbeddedWorkerInstance::StartingPhase::SCRIPT_DOWNLOADING:
      return false;
    case EmbeddedWorkerInstance::StartingPhase::SCRIPT_LOADED:
    case EmbeddedWorkerInstance::StartingPhase::SCRIPT_EVALUATION:
      CHECK(warm_up_callbacks_.empty());
      return true;
    case EmbeddedWorkerInstance::StartingPhase::STARTING_PHASE_MAX_VALUE:
      NOTREACHED();
  }
}

void ServiceWorkerVersion::SetValidOriginTrialTokens(
    const blink::TrialTokenValidator::FeatureToTokensMap& tokens) {
  origin_trial_tokens_ =
      validator_.GetValidTokens(key_.origin(), tokens, clock_->Now());
}

void ServiceWorkerVersion::SetDevToolsAttached(bool attached) {
  embedded_worker()->SetDevToolsAttached(attached);

  if (stop_when_devtools_detached_ && !attached) {
    DCHECK_EQ(REDUNDANT, status());
    if (running_status() == blink::EmbeddedWorkerStatus::kStarting ||
        running_status() == blink::EmbeddedWorkerStatus::kRunning) {
      embedded_worker_->Stop();
    }
    return;
  }
  if (attached) {
    // TODO(falken): Canceling the timeouts when debugging could cause
    // heisenbugs; we should instead run them as normal show an educational
    // message in DevTools when they occur. crbug.com/470419

    // Don't record the startup time metric once DevTools is attached.
    ClearTick(&start_time_);
    skip_recording_startup_time_ = true;

    // Cancel request timeouts.
    SetAllRequestExpirations(base::TimeTicks());
    return;
  }
  if (!start_callbacks_.empty()) {
    // Reactivate the timer for start timeout.
    DCHECK(timeout_timer_.IsRunning());
    DCHECK(running_status() == blink::EmbeddedWorkerStatus::kStarting ||
           running_status() == blink::EmbeddedWorkerStatus::kStopping)
        << static_cast<int>(running_status());
    RestartTick(&start_time_);
  }

  // Reactivate request timeouts, setting them all to the same expiration time.
  SetAllRequestExpirations(tick_clock_->NowTicks() + kRequestTimeout);
}

void ServiceWorkerVersion::SetMainScriptResponse(
    std::unique_ptr<MainScriptResponse> response) {
  script_response_time_for_devtools_ = response->response_time;
  main_script_response_ = std::move(response);

  // Updates |origin_trial_tokens_| if it is not set yet. This happens when:
  //  1) The worker is a new one.
  //  OR
  //  2) The worker is an existing one but the entry in ServiceWorkerDatabase
  //     was written by old version Chrome (< M56), so |origin_trial_tokens|
  //     wasn't set in the entry.
  if (!origin_trial_tokens_) {
    origin_trial_tokens_ = validator_.GetValidTokensFromHeaders(
        key_.origin(), main_script_response_->headers.get(), clock_->Now());
  }

  if (context_) {
    context_->OnMainScriptResponseSet(version_id(), *main_script_response_);
  }
}

void ServiceWorkerVersion::SimulatePingTimeoutForTesting() {
  ping_controller_.SimulateTimeoutForTesting();
}

void ServiceWorkerVersion::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void ServiceWorkerVersion::RunUserTasksForTesting() {
  timeout_timer_.user_task().Run();
}

bool ServiceWorkerVersion::HasNoWork() const {
  return !HasWorkInBrowser() && worker_is_idle_on_renderer_;
}

const ServiceWorkerVersion::MainScriptResponse*
ServiceWorkerVersion::GetMainScriptResponse() {
  return main_script_response_.get();
}

ServiceWorkerVersion::InflightRequestTimeoutInfo::InflightRequestTimeoutInfo(
    int id,
    ServiceWorkerMetrics::EventType event_type,
    const base::TimeTicks& expiration_time,
    TimeoutBehavior timeout_behavior)
    : id(id),
      event_type(event_type),
      expiration_time(expiration_time),
      timeout_behavior(timeout_behavior) {}

ServiceWorkerVersion::InflightRequestTimeoutInfo::
    ~InflightRequestTimeoutInfo() {}

bool ServiceWorkerVersion::InflightRequestTimeoutInfo::operator<(
    const InflightRequestTimeoutInfo& other) const {
  if (expiration_time == other.expiration_time) {
    return id < other.id;
  }
  return expiration_time < other.expiration_time;
}

ServiceWorkerVersion::InflightRequest::InflightRequest(
    StatusCallback callback,
    base::Time time,
    const base::TimeTicks& time_ticks,
    ServiceWorkerMetrics::EventType event_type)
    : error_callback(std::move(callback)),
      start_time(time),
      start_time_ticks(time_ticks),
      event_type(event_type) {}

ServiceWorkerVersion::InflightRequest::~InflightRequest() {}

void ServiceWorkerVersion::OnScriptEvaluationStart() {
  DCHECK_EQ(blink::EmbeddedWorkerStatus::kStarting, running_status());
  // Activate ping/pong now that JavaScript execution will start.
  ping_controller_.Activate();
}

void ServiceWorkerVersion::OnScriptLoaded() {
  std::vector<StatusCallback> callbacks;
  callbacks.swap(warm_up_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
  }
}

void ServiceWorkerVersion::OnProcessAllocated() {
  // If we have not initialized the global scope yet, return early.
  if (!is_endpoint_ready_) {
    return;
  }
  GetContentClient()
      ->browser()
      ->RegisterAssociatedInterfaceBindersForServiceWorker(
          GetInfo(), *associated_registry_);
}

void ServiceWorkerVersion::OnStarting() {
  for (auto& observer : observers_)
    observer.OnRunningStateChanged(this);
}

void ServiceWorkerVersion::OnStarted(
    blink::mojom::ServiceWorkerStartStatus start_status,
    FetchHandlerType new_fetch_handler_type,
    bool new_has_hid_event_handlers,
    bool new_has_usb_event_handlers) {
  DCHECK_EQ(blink::EmbeddedWorkerStatus::kRunning, running_status());

  // TODO(falken): This maps kAbruptCompletion to kErrorScriptEvaluated, which
  // most start callbacks will consider to be a failure. But the worker thread
  // is running, and the spec considers it a success, so the callbacks should
  // change to treat kErrorScriptEvaluated as success, or use
  // ServiceWorkerStartStatus directly.
  blink::ServiceWorkerStatusCode status =
      mojo::ConvertTo<blink::ServiceWorkerStatusCode>(start_status);

  if (status == blink::ServiceWorkerStatusCode::kOk) {
    if (fetch_handler_type_ && fetch_handler_type_ != new_fetch_handler_type) {
      context_->registry()->UpdateFetchHandlerType(
          registration_id_, key_, new_fetch_handler_type,
          // Ignore errors; bumping the update fetch handler type is
          // just best-effort.
          base::DoNothing());
      base::UmaHistogramEnumeration(
          "ServiceWorker.OnStarted.UpdatedFetchHandlerType",
          new_fetch_handler_type);
      base::UmaHistogramEnumeration(
          base::StrCat(
              {"ServiceWorker.OnStarted.UpdatedFetchHandlerTypeBySourceType",
               FetchHandlerTypeToSuffix(*fetch_handler_type_)}),
          new_fetch_handler_type);
    }
    if (!fetch_handler_type_) {
      // When the new service worker starts, the fetch handler type is unknown
      // until this point.
      set_fetch_handler_type(new_fetch_handler_type);
    } else {
      // Starting the installed service worker should not change the existence
      // of the fetch handler.
      SCOPED_CRASH_KEY_NUMBER("SWVersion", "old_FHType",
                              static_cast<int32_t>(*fetch_handler_type_));
      SCOPED_CRASH_KEY_NUMBER("SWVersion", "new_FHType",
                              static_cast<int32_t>(new_fetch_handler_type));
      DCHECK_EQ(*fetch_handler_type_ != FetchHandlerType::kNoHandler,
                new_fetch_handler_type != FetchHandlerType::kNoHandler);
      fetch_handler_type_ = new_fetch_handler_type;
    }

    has_hid_event_handlers_ = new_has_hid_event_handlers;
    has_usb_event_handlers_ = new_has_usb_event_handlers;
  }

  // Update |sha256_script_checksum_| if it's empty. This can happen when the
  // script is updated and the new service worker version is created. This case
  // ServiceWorkerVersion::SetResources() isn't called and
  // |sha256_script_checksum_| should be empty. Calculate the checksum string
  // with the script newly added/updated in |script_cache_map_|.
  if (!sha256_script_checksum_) {
    sha256_script_checksum_ = MergeResourceRecordSHA256ScriptChecksum(
        script_url_, script_cache_map_, fetch_handler_type_);
  }

  // Fire all start callbacks.
  scoped_refptr<ServiceWorkerVersion> protect(this);
  FinishStartWorker(status);
  for (auto& observer : observers_)
    observer.OnRunningStateChanged(this);

  if (!pending_external_requests_.empty()) {
    std::map<base::Uuid, ServiceWorkerExternalRequestTimeoutType>
        pending_external_requests;
    std::swap(pending_external_requests_, pending_external_requests);
    for (const auto& [uuid, timeout_type] : pending_external_requests)
      StartExternalRequest(uuid, timeout_type);
  }
}

void ServiceWorkerVersion::OnStopping() {
  TRACE_EVENT0("ServiceWorker", "ServiceWorkerVersion::OnStopping");
  DCHECK(stop_time_.is_null());
  RestartTick(&stop_time_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      "ServiceWorker", "ServiceWorkerVersion::StopWorker",
      TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::StopWorker",
                          stop_time_.since_origin().InMicroseconds()),
      "Script", script_url_.spec(), "Version Status",
      VersionStatusToString(status_));

  // If the service worker is warming up or warmed up, Such workers
  // don't need to be restarted in `OnStoppedInternal()`.
  is_stopping_warmed_up_worker_ =
      embedded_worker_->pause_initializing_global_scope();

  // Endpoint isn't available after calling EmbeddedWorkerInstance::Stop().
  // This needs to be set here without waiting until the worker is actually
  // stopped because subsequent StartWorker() may read the flag to decide
  // whether an event can be dispatched or not.
  is_endpoint_ready_ = false;

  // Shorten the interval so stalling in stopped can be fixed quickly. Once the
  // worker stops, the timer is disabled. The interval will be reset to normal
  // when the worker starts up again.
  SetTimeoutTimerInterval(kStopWorkerTimeout);
  for (auto& observer : observers_)
    observer.OnRunningStateChanged(this);
}

void ServiceWorkerVersion::OnStopped(blink::EmbeddedWorkerStatus old_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnStoppedInternal(old_status);
}

void ServiceWorkerVersion::OnDetached(blink::EmbeddedWorkerStatus old_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnStoppedInternal(old_status);
}

void ServiceWorkerVersion::OnRegisteredToDevToolsManager() {
  for (auto& observer : observers_)
    observer.OnDevToolsRoutingIdChanged(this);
}

void ServiceWorkerVersion::OnReportException(
    const std::u16string& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  for (auto& observer : observers_) {
    observer.OnErrorReported(this, error_message, line_number, column_number,
                             source_url);
  }
}

void ServiceWorkerVersion::OnReportConsoleMessage(
    blink::mojom::ConsoleMessageSource source,
    blink::mojom::ConsoleMessageLevel message_level,
    const std::u16string& message,
    int line_number,
    const GURL& source_url) {
  for (auto& observer : observers_) {
    observer.OnReportConsoleMessage(this, source, message_level, message,
                                    line_number, source_url);
  }
}

void ServiceWorkerVersion::OnStartSent(blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    scoped_refptr<ServiceWorkerVersion> protect(this);
    FinishStartWorker(DeduceStartWorkerFailureReason(status));
  }
}

void ServiceWorkerVersion::SetCachedMetadata(const GURL& url,
                                             base::span<const uint8_t> data) {
  int64_t callback_id =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "ServiceWorker", "ServiceWorkerVersion::SetCachedMetadata",
      TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::SetCachedMetadata",
                          callback_id),
      "URL", url.spec());
  script_cache_map_.WriteMetadata(
      url, data,
      base::BindOnce(&ServiceWorkerVersion::OnSetCachedMetadataFinished,
                     weak_factory_.GetWeakPtr(), callback_id, data.size()));
}

void ServiceWorkerVersion::ClearCachedMetadata(const GURL& url) {
  int64_t callback_id =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "ServiceWorker", "ServiceWorkerVersion::ClearCachedMetadata",
      TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::ClearCachedMetadata",
                          callback_id),
      "URL", url.spec());
  script_cache_map_.ClearMetadata(
      url, base::BindOnce(&ServiceWorkerVersion::OnClearCachedMetadataFinished,
                          weak_factory_.GetWeakPtr(), callback_id));
}

void ServiceWorkerVersion::ClaimClients(ClaimClientsCallback callback) {
  if (status_ != ACTIVATING && status_ != ACTIVATED) {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kState,
                            std::string(kClaimClientsStateErrorMesage));
    return;
  }
  if (!context_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kAbort,
                            std::string(kClaimClientsShutdownErrorMesage));
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id_);
  // Registration must be kept alive by ServiceWorkerGlobalScope#registration.
  if (!registration) {
    associated_interface_receiver_.ReportBadMessage(
        "ClaimClients: No live registration");
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string());
    return;
  }

  registration->ClaimClients();
  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          std::nullopt);
}

void ServiceWorkerVersion::GetClients(
    blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
    GetClientsCallback callback) {
  service_worker_client_utils::GetClients(
      weak_factory_.GetWeakPtr(), std::move(options), std::move(callback));
}

void ServiceWorkerVersion::GetClient(const std::string& client_uuid,
                                     GetClientCallback callback) {
  if (!context_) {
    // The promise will be resolved to 'undefined'.
    std::move(callback).Run(nullptr);
    return;
  }
  ServiceWorkerClient* service_worker_client =
      context_->service_worker_client_owner().GetServiceWorkerClientByClientID(
          client_uuid);
  if (!service_worker_client ||
      service_worker_client->url().DeprecatedGetOriginAsURL() !=
          script_url_.DeprecatedGetOriginAsURL()) {
    // The promise will be resolved to 'undefined'.
    // Note that we don't BadMessage here since Clients#get() can be passed an
    // arbitrary UUID. The BadMessages for the origin mismatches below are
    // appropriate because the UUID is taken directly from a Client object so we
    // expect it to be valid.
    std::move(callback).Run(nullptr);
    return;
  }
  if (!service_worker_client->is_execution_ready()) {
    service_worker_client->AddExecutionReadyCallback(
        base::BindOnce(&ServiceWorkerVersion::GetClientInternal, this,
                       client_uuid, std::move(callback)));
    return;
  }
  service_worker_client_utils::GetClient(service_worker_client,
                                         std::move(callback));
}

void ServiceWorkerVersion::GetClientInternal(const std::string& client_uuid,
                                             GetClientCallback callback) {
  if (!context_) {
    // It is shutting down, so resolve the promise to undefined in this case.
    std::move(callback).Run(nullptr);
    return;
  }

  ServiceWorkerClient* service_worker_client =
      context_->service_worker_client_owner().GetServiceWorkerClientByClientID(
          client_uuid);
  if (!service_worker_client || !service_worker_client->is_execution_ready()) {
    std::move(callback).Run(nullptr);
    return;
  }
  service_worker_client_utils::GetClient(service_worker_client,
                                         std::move(callback));
}

void ServiceWorkerVersion::OpenNewTab(const GURL& url,
                                      OpenNewTabCallback callback) {
  // TODO(crbug.com/40177656): After StorageKey implements partitioning update
  // this to reject with InvalidAccessError if key_ is partitioned.
  OpenWindow(url, service_worker_client_utils::WindowType::NEW_TAB_WINDOW,
             std::move(callback));
}

void ServiceWorkerVersion::OpenPaymentHandlerWindow(
    const GURL& url,
    OpenPaymentHandlerWindowCallback callback) {
  // Just respond failure if we are shutting down.
  if (!context_) {
    std::move(callback).Run(
        false /* success */, nullptr /* client */,
        std::string("The service worker system is shutting down."));
    return;
  }

  if (!url.is_valid() || !key_.origin().IsSameOriginWith(url)) {
    associated_interface_receiver_.ReportBadMessage(
        "Received PaymentRequestEvent#openWindow() request for a cross-origin "
        "URL.");
    receiver_.reset();
    return;
  }

  PaymentHandlerSupport::ShowPaymentHandlerWindow(
      url, context_.get(),
      base::BindOnce(&DidShowPaymentHandlerWindow, url, key_, context_),
      base::BindOnce(
          &ServiceWorkerVersion::OpenWindow, weak_factory_.GetWeakPtr(), url,
          service_worker_client_utils::WindowType::PAYMENT_HANDLER_WINDOW),
      std::move(callback));
}

void ServiceWorkerVersion::PostMessageToClient(
    const std::string& client_uuid,
    blink::TransferableMessage message) {
  if (!context_)
    return;
  ServiceWorkerClient* service_worker_client =
      context_->service_worker_client_owner().GetServiceWorkerClientByClientID(
          client_uuid);
  if (!service_worker_client) {
    // The client may already have been closed, just ignore.
    return;
  }

  if (IsBackForwardCacheEnabled()) {
    // When |PostMessageToClient| is called on a client that is in bfcache,
    // evict the bfcache entry.
    if (service_worker_client->IsInBackForwardCache()) {
      EvictBackForwardCachedControllee(
          service_worker_client, BackForwardCacheMetrics::NotRestoredReason::
                                     kServiceWorkerPostMessage);
      return;
    }
  }

  if (service_worker_client->url().DeprecatedGetOriginAsURL() !=
      script_url_.DeprecatedGetOriginAsURL()) {
    associated_interface_receiver_.ReportBadMessage(
        "Received Client#postMessage() request for a cross-origin client.");
    receiver_.reset();
    return;
  }
  if (!service_worker_client->is_execution_ready()) {
    // It's subtle why this ReportBadMessage is correct. Consider the
    // sequence:
    // 1. Page does ServiceWorker.postMessage().
    // 2. Service worker does onmessage = (evt) => {evt.source.postMessage()};.
    //
    // The IPC sequence is:
    // 1. Page sends NotifyExecutionReady() to its ServiceWorkerContainerHost
    //    once created.
    // 2. Page sends PostMessageToServiceWorker() to the object's
    //    ServiceWorkerObjectHost.
    // 3. Service worker sends PostMessageToClient() to its ServiceWorkerHost.
    //
    // It's guaranteed that 1. arrives before 2., since the
    // ServiceWorkerObjectHost must have been sent over
    // ServiceWorkerContainerHost (using Register, GetRegistrationForReady), so
    // they are associated. After that 3. occurs and we get here and are
    // guaranteed execution ready the above ordering.
    //
    // The above reasoning would break if there is a way for a page to get a
    // ServiceWorkerObjectHost not associated with its
    // ServiceWorkerContainerHost. If that world should occur, we should queue
    // the message instead of crashing.
    associated_interface_receiver_.ReportBadMessage(
        "Received Client#postMessage() request for a reserved client.");
    receiver_.reset();
    return;
  }
  // As we don't track tasks between workers and renderers, we can nullify the
  // message's parent task ID.
  message.parent_task_id = std::nullopt;
  service_worker_client->container_host()->PostMessageToClient(
      *this, std::move(message));
}

void ServiceWorkerVersion::FocusClient(const std::string& client_uuid,
                                       FocusClientCallback callback) {
  if (!context_) {
    std::move(callback).Run(nullptr /* client */);
    return;
  }
  ServiceWorkerClient* service_worker_client =
      context_->service_worker_client_owner().GetServiceWorkerClientByClientID(
          client_uuid);
  if (!service_worker_client) {
    // The client may already have been closed, just fail.
    std::move(callback).Run(nullptr /* client */);
    return;
  }
  if (service_worker_client->url().DeprecatedGetOriginAsURL() !=
      script_url_.DeprecatedGetOriginAsURL()) {
    associated_interface_receiver_.ReportBadMessage(
        "Received WindowClient#focus() request for a cross-origin client.");
    receiver_.reset();
    return;
  }
  if (!service_worker_client->IsContainerForWindowClient()) {
    // focus() should be called only for WindowClient.
    associated_interface_receiver_.ReportBadMessage(
        "Received WindowClient#focus() request for a non-window client.");
    receiver_.reset();
    return;
  }

  service_worker_client_utils::FocusWindowClient(service_worker_client,
                                                 std::move(callback));
}

void ServiceWorkerVersion::NavigateClient(const std::string& client_uuid,
                                          const GURL& url,
                                          NavigateClientCallback callback) {
  if (!context_) {
    std::move(callback).Run(
        false /* success */, nullptr /* client */,
        std::string("The service worker system is shutting down."));
    return;
  }

  if (!url.is_valid() ||
      !base::Uuid::ParseCaseInsensitive(client_uuid).is_valid()) {
    associated_interface_receiver_.ReportBadMessage(
        "Received unexpected invalid URL/UUID from renderer process.");
    receiver_.reset();
    return;
  }

  // Reject requests for URLs that the process is not allowed to access. It's
  // possible to receive such requests since the renderer-side checks are
  // slightly different. For example, the view-source scheme will not be
  // filtered out by Blink.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          embedded_worker_->process_id(), url)) {
    std::move(callback).Run(
        false /* success */, nullptr /* client */,
        "The service worker is not allowed to access URL: " + url.spec());
    return;
  }

  ServiceWorkerClient* service_worker_client =
      context_->service_worker_client_owner().GetServiceWorkerClientByClientID(
          client_uuid);
  if (!service_worker_client) {
    std::move(callback).Run(false /* success */, nullptr /* client */,
                            std::string("The client was not found."));
    return;
  }
  if (service_worker_client->url().DeprecatedGetOriginAsURL() !=
      script_url_.DeprecatedGetOriginAsURL()) {
    associated_interface_receiver_.ReportBadMessage(
        "Received WindowClient#navigate() request for a cross-origin client.");
    receiver_.reset();
    return;
  }
  if (!service_worker_client->IsContainerForWindowClient()) {
    // navigate() should be called only for WindowClient.
    associated_interface_receiver_.ReportBadMessage(
        "Received WindowClient#navigate() request for a non-window client.");
    receiver_.reset();
    return;
  }
  if (service_worker_client->controller() != this) {
    std::move(callback).Run(
        false /* success */, nullptr /* client */,
        std::string(
            "This service worker is not the client's active service worker."));
    return;
  }

  service_worker_client_utils::NavigateClient(
      url, script_url_, key_, service_worker_client->GetRenderFrameHostId(),
      context_, base::BindOnce(&DidNavigateClient, std::move(callback), url));

  NotifyClientNavigated(script_url_, url);
}

void ServiceWorkerVersion::SkipWaiting(SkipWaitingCallback callback) {
  skip_waiting_ = true;

  // Per spec, resolve the skip waiting promise now if activation won't be
  // triggered here. The ActivateWaitingVersionWhenReady() call below only
  // triggers it if we're in INSTALLED state. So if we're not in INSTALLED
  // state, resolve the promise now. Even if we're in INSTALLED state, there are
  // still cases where ActivateWaitingVersionWhenReady() won't trigger the
  // activation. In that case, it's a slight spec violation to not resolve now,
  // but we'll eventually resolve the promise in SetStatus().
  if (status_ != INSTALLED) {
    std::move(callback).Run(true);
    return;
  }

  if (!context_) {
    std::move(callback).Run(false);
    return;
  }
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id_);
  // TODO(leonhsl): Here we should be guaranteed a registration since
  // ServiceWorkerGlobalScope#registration should be keeping the registration
  // alive currently. So we need to confirm and remove this nullable check
  // later.
  if (!registration) {
    std::move(callback).Run(false);
    return;
  }
  if (skip_waiting_time_.is_null())
    RestartTick(&skip_waiting_time_);
  pending_skip_waiting_requests_.push_back(std::move(callback));
  if (pending_skip_waiting_requests_.size() == 1)
    registration->ActivateWaitingVersionWhenReady();
}

void ServiceWorkerVersion::AddRoutes(
    const blink::ServiceWorkerRouterRules& rules,
    AddRoutesCallback callback) {
  if (!IsStaticRouterEnabled()) {
    // This renderer should have called this only when the feature is enabled.
    associated_interface_receiver_.ReportBadMessage(
        "Unexpected router registration call during the feature is disabled.");
    return;
  }
  auto error = SetupRouterEvaluator(rules);
  bool is_parse_error = false;
  switch (error) {
    case ServiceWorkerRouterEvaluatorErrorEnums::kNoError:
      break;
    case ServiceWorkerRouterEvaluatorErrorEnums::kParseError:
      is_parse_error = true;
      break;
    default:
      // The renderer should have denied calling this method while the setup
      // fails.
      associated_interface_receiver_.ReportBadMessage(
          "Failed to configure a router. Possibly a syntax error");
      return;
  }
  std::move(callback).Run(is_parse_error);
}

void ServiceWorkerVersion::OnSetCachedMetadataFinished(int64_t callback_id,
                                                       size_t size,
                                                       int result) {
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerVersion::SetCachedMetadata",
      TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::SetCachedMetadata",
                          callback_id),
      "result", result);
  for (auto& observer : observers_)
    observer.OnCachedMetadataUpdated(this, size);
}

void ServiceWorkerVersion::OnClearCachedMetadataFinished(int64_t callback_id,
                                                         int result) {
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerVersion::ClearCachedMetadata",
      TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::ClearCachedMetadata",
                          callback_id),
      "result", result);
  for (auto& observer : observers_)
    observer.OnCachedMetadataUpdated(this, 0);
}

void ServiceWorkerVersion::OpenWindow(
    GURL url,
    service_worker_client_utils::WindowType type,
    OpenNewTabCallback callback) {
  // Just respond failure if we are shutting down.
  if (!context_) {
    std::move(callback).Run(
        false /* success */, nullptr /* client */,
        std::string("The service worker system is shutting down."));
    return;
  }

  if (!url.is_valid()) {
    associated_interface_receiver_.ReportBadMessage(
        "Received unexpected invalid URL from renderer process.");
    receiver_.reset();
    return;
  }

  // The renderer treats all URLs in the about: scheme as being about:blank.
  // Canonicalize about: URLs to about:blank.
  if (url.SchemeIs(url::kAboutScheme))
    url = GURL(url::kAboutBlankURL);

  // Reject requests for URLs that the process is not allowed to access. It's
  // possible to receive such requests since the renderer-side checks are
  // slightly different. For example, the view-source scheme will not be
  // filtered out by Blink.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          embedded_worker_->process_id(), url)) {
    std::move(callback).Run(false /* success */, nullptr /* client */,
                            url.spec() + " cannot be opened.");
    return;
  }

  service_worker_client_utils::OpenWindow(
      url, script_url_, key_, embedded_worker_->embedded_worker_id(),
      embedded_worker_->process_id(), context_, type,
      base::BindOnce(&OnOpenWindowFinished, std::move(callback)));

  NotifyWindowOpened(script_url_, url);
}

bool ServiceWorkerVersion::HasWorkInBrowser() const {
  return !inflight_requests_.IsEmpty() || !start_callbacks_.empty() ||
         !warm_up_callbacks_.empty();
}

void ServiceWorkerVersion::OnSimpleEventFinished(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  InflightRequest* request = inflight_requests_.Lookup(request_id);
  // |request| will be null when the request has been timed out.
  if (!request)
    return;
  // Copy error callback before calling FinishRequest.
  StatusCallback error_callback = std::move(request->error_callback);

  FinishRequest(request_id,
                status == blink::mojom::ServiceWorkerEventStatus::COMPLETED);
  // TODO(http://crbug.com/1251834): Why are we running the "error callback"
  // even when there is no error? Clean this up.
  std::move(error_callback)
      .Run(mojo::ConvertTo<blink::ServiceWorkerStatusCode>(status));
}

void ServiceWorkerVersion::CountFeature(blink::mojom::WebFeature feature) {
  if (!used_features_.insert(feature).second)
    return;

  // TODO(crbug.com/1253581 crbug.com/1021718): Speculative bug fix code.
  // Take snapshot of the `controllee_map_` instead of iterating on it directly.
  // This is to rule out the possibility of `controllee_map_` being modified
  // while we call `CountFeature`.
  std::vector<base::WeakPtr<ServiceWorkerClient>> snapshot;
  snapshot.reserve(controllee_map_.size());
  for (auto service_worker_client_by_uuid : controllee_map_) {
    snapshot.push_back(service_worker_client_by_uuid.second);
  }
  for (auto service_worker_client : snapshot) {
    if (service_worker_client) {
      service_worker_client->CountFeature(feature);
    }
  }
}

network::mojom::CrossOriginEmbedderPolicyValue
ServiceWorkerVersion::cross_origin_embedder_policy_value() const {
  return policy_container_host_
             ? policy_container_host_->cross_origin_embedder_policy().value
             : network::mojom::CrossOriginEmbedderPolicyValue::kNone;
}

const network::CrossOriginEmbedderPolicy*
ServiceWorkerVersion::cross_origin_embedder_policy() const {
  return policy_container_host_
             ? &policy_container_host_->cross_origin_embedder_policy()
             : nullptr;
}

const network::DocumentIsolationPolicy*
ServiceWorkerVersion::document_isolation_policy() const {
  return policy_container_host_
             ? &policy_container_host_->document_isolation_policy()
             : nullptr;
}

const network::mojom::ClientSecurityStatePtr
ServiceWorkerVersion::BuildClientSecurityState() const {
  if (!policy_container_host_) {
    return nullptr;
  }

  const PolicyContainerPolicies& policies = policy_container_host_->policies();
  return network::mojom::ClientSecurityState::New(
      policies.cross_origin_embedder_policy, policies.is_web_secure_context,
      policies.ip_address_space,
      DerivePrivateNetworkRequestPolicy(policies.ip_address_space,
                                        policies.is_web_secure_context,
                                        PrivateNetworkRequestContext::kWorker),
      policies.document_isolation_policy);
}

// static
bool ServiceWorkerVersion::IsInstalled(ServiceWorkerVersion::Status status) {
  switch (status) {
    case ServiceWorkerVersion::NEW:
    case ServiceWorkerVersion::INSTALLING:
    case ServiceWorkerVersion::REDUNDANT:
      return false;
    case ServiceWorkerVersion::INSTALLED:
    case ServiceWorkerVersion::ACTIVATING:
    case ServiceWorkerVersion::ACTIVATED:
      return true;
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected status: " << status;
  return false;
}

// static
std::string ServiceWorkerVersion::VersionStatusToString(
    ServiceWorkerVersion::Status status) {
  switch (status) {
    case ServiceWorkerVersion::NEW:
      return "new";
    case ServiceWorkerVersion::INSTALLING:
      return "installing";
    case ServiceWorkerVersion::INSTALLED:
      return "installed";
    case ServiceWorkerVersion::ACTIVATING:
      return "activating";
    case ServiceWorkerVersion::ACTIVATED:
      return "activated";
    case ServiceWorkerVersion::REDUNDANT:
      return "redundant";
  }
  NOTREACHED_IN_MIGRATION() << status;
  return std::string();
}

void ServiceWorkerVersion::IncrementPendingUpdateHintCount() {
  pending_update_hint_count_++;
}

void ServiceWorkerVersion::DecrementPendingUpdateHintCount() {
  DCHECK_GT(pending_update_hint_count_, 0);
  pending_update_hint_count_--;
  if (pending_update_hint_count_ == 0)
    ScheduleUpdate();
}

void ServiceWorkerVersion::OnPongFromWorker() {
  ping_controller_.OnPongReceived();
}

void ServiceWorkerVersion::DidEnsureLiveRegistrationForStartWorker(
    ServiceWorkerMetrics::EventType purpose,
    Status prestart_status,
    bool is_browser_startup_complete,
    StatusCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  scoped_refptr<ServiceWorkerRegistration> protect = registration;
  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // When the registration has already been deleted from the storage but its
    // active worker is still controlling clients, the event should be
    // dispatched on the worker. However, the storage cannot find the
    // registration. To handle the case, check the live registrations here.
    protect = context_->GetLiveRegistration(registration_id_);
    if (protect) {
      DCHECK(protect->is_uninstalling());
      status = blink::ServiceWorkerStatusCode::kOk;
    }
  }
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    RecordStartWorkerResult(purpose, prestart_status, kInvalidTraceId,
                            is_browser_startup_complete, status);
    RunSoon(base::BindOnce(
        std::move(callback),
        blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed));
    return;
  }
  if (is_redundant()) {
    RecordStartWorkerResult(purpose, prestart_status, kInvalidTraceId,
                            is_browser_startup_complete,
                            blink::ServiceWorkerStatusCode::kErrorRedundant);
    RunSoon(base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorRedundant));
    return;
  }

  MarkIfStale();

  switch (running_status()) {
    case blink::EmbeddedWorkerStatus::kRunning:
      RunSoon(base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kOk));
      return;
    case blink::EmbeddedWorkerStatus::kStarting:
      DCHECK(!start_callbacks_.empty());
      if (embedded_worker_->pause_initializing_global_scope()) {
        CHECK(IsWarmingUp() || IsWarmedUp());
        // Extend timeout.
        if (!start_time_.is_null()) {
          RestartTick(&start_time_);
        }
        // When 'pause_initializing_global_scope()' returns true, the
        // STARTING state means this service worker is already warmed
        // up. Do nothing here.
        if (purpose == ServiceWorkerMetrics::EventType::WARM_UP) {
          RunSoon(base::BindOnce(std::move(callback),
                                 blink::ServiceWorkerStatusCode::kOk));
          return;
        } else {
          int trace_id = NextTraceId();
          TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
              "ServiceWorker", "ServiceWorkerVersion::StartWorker",
              TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::StartWorker",
                                  trace_id),
              "Script", script_url_.spec(), "Purpose",
              ServiceWorkerMetrics::EventTypeToString(purpose));

          embedded_worker_->ResumeInitializingGlobalScope();

          start_callbacks_.push_back(base::BindOnce(
              &ServiceWorkerVersion::RecordStartWorkerResult,
              weak_factory_.GetWeakPtr(), purpose, prestart_status, trace_id,
              is_browser_startup_complete));
        }
      }
      break;
    case blink::EmbeddedWorkerStatus::kStopping:
    case blink::EmbeddedWorkerStatus::kStopped:
      if (running_status() == blink::EmbeddedWorkerStatus::kStopped &&
          purpose == ServiceWorkerMetrics::EventType::WARM_UP) {
        // Postpone initializing the global scope.
        embedded_worker_->SetPauseInitializingGlobalScope();

        if (warm_up_callbacks_.empty()) {
          int trace_id = NextTraceId();
          TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
              "ServiceWorker", "ServiceWorkerVersion::WarmUpWorker",
              TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::WarmUpWorker",
                                  trace_id),
              "Script", script_url_.spec());
          warm_up_callbacks_.push_back(base::BindOnce(
              [](int trace_id, blink::ServiceWorkerStatusCode status) {
                TRACE_EVENT_NESTABLE_ASYNC_END1(
                    "ServiceWorker", "ServiceWorkerVersion::WarmUpWorker",
                    TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::WarmUpWorker",
                                        trace_id),
                    "Status", blink::ServiceWorkerStatusToString(status));
              },
              trace_id));
        }
      } else {
        if (start_callbacks_.empty()) {
          int trace_id = NextTraceId();
          TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
              "ServiceWorker", "ServiceWorkerVersion::StartWorker",
              TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::StartWorker",
                                  trace_id),
              "Script", script_url_.spec(), "Purpose",
              ServiceWorkerMetrics::EventTypeToString(purpose));
          start_callbacks_.push_back(base::BindOnce(
              &ServiceWorkerVersion::RecordStartWorkerResult,
              weak_factory_.GetWeakPtr(), purpose, prestart_status, trace_id,
              is_browser_startup_complete));
        }
      }
      break;
  }

  if (purpose == ServiceWorkerMetrics::EventType::WARM_UP) {
    // Keep the live registration while starting the worker.
    start_callbacks_.push_back(
        base::BindOnce([](scoped_refptr<ServiceWorkerRegistration> protect,
                          blink::ServiceWorkerStatusCode status) {},
                       protect));
    warm_up_callbacks_.push_back(base::BindOnce(
        [](StatusCallback callback, blink::ServiceWorkerStatusCode status) {
          std::move(callback).Run(status);
        },
        std::move(callback)));
  } else {
    // Keep the live registration while starting the worker.
    start_callbacks_.push_back(base::BindOnce(
        [](StatusCallback callback,
           scoped_refptr<ServiceWorkerRegistration> protect,
           blink::ServiceWorkerStatusCode status) {
          std::move(callback).Run(status);
        },
        std::move(callback), protect));
  }

  if (running_status() == blink::EmbeddedWorkerStatus::kStopped) {
    StartWorkerInternal();
  }
  // Warning: StartWorkerInternal() might have deleted `this` on failure.
}

void ServiceWorkerVersion::StartWorkerInternal() {
  DCHECK_EQ(blink::EmbeddedWorkerStatus::kStopped, running_status());
  DCHECK(inflight_requests_.IsEmpty());
  DCHECK(request_timeouts_.empty());

  // Don't try to start a new worker thread if the `context_` has been
  // destroyed.  This can happen during browser shutdown or if corruption
  // forced a storage reset.
  if (!context_) {
    FinishStartWorker(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }

  StartTimeoutTimer();

  // Set expiration time in advance so that the service worker can
  // call postMessage() to itself immediately after it starts.
  max_request_expiration_time_ = tick_clock_->NowTicks() + kRequestTimeout;

  worker_is_idle_on_renderer_ = false;
  needs_to_be_terminated_asap_ = false;

  auto provider_info =
      blink::mojom::ServiceWorkerProviderInfoForStartWorker::New();
  DCHECK(!worker_host_);
  worker_host_ = std::make_unique<content::ServiceWorkerHost>(
      provider_info->host_remote.InitWithNewEndpointAndPassReceiver(), *this,
      context());

  auto params = blink::mojom::EmbeddedWorkerStartParams::New();
  params->service_worker_version_id = version_id_;
  params->scope = scope_;
  params->script_url = script_url_;
  params->script_type = script_type_;
  // Need to clone this object because StartWorkerInternal() can/ be called
  // more than once.
  params->outside_fetch_client_settings_object =
      outside_fetch_client_settings_object_.Clone();

  ContentBrowserClient* browser_client = GetContentClient()->browser();
  params->user_agent = browser_client->GetUserAgentBasedOnPolicy(
      context_->wrapper()->browser_context());
  params->ua_metadata = browser_client->GetUserAgentMetadata();
  params->is_installed = IsInstalled(status_);
  params->script_url_to_skip_throttling = updated_script_url_;
  params->main_script_load_params = std::move(main_script_load_params_);

  if (IsInstalled(status())) {
    DCHECK(!installed_scripts_sender_);
    installed_scripts_sender_ =
        std::make_unique<ServiceWorkerInstalledScriptsSender>(this);
    params->installed_scripts_info =
        installed_scripts_sender_->CreateInfoAndBind();
    installed_scripts_sender_->Start();
  }

  params->service_worker_receiver =
      service_worker_remote_.BindNewPipeAndPassReceiver();
  // TODO(horo): These CHECKs are for debugging crbug.com/759938.
  CHECK(service_worker_remote_.is_bound());
  CHECK(params->service_worker_receiver.is_valid());
  service_worker_remote_.set_disconnect_handler(
      base::BindOnce(&OnConnectionError, embedded_worker_->AsWeakPtr()));

  if (!controller_receiver_.is_valid()) {
    controller_receiver_ = remote_controller_.BindNewPipeAndPassReceiver();
  }
  params->controller_receiver = std::move(controller_receiver_);

  params->provider_info = std::move(provider_info);
  params->service_worker_token = worker_host_->token();
  params->ukm_source_id = ukm_source_id_;
  params->storage_key = key_;

  // policy_container_host could be null for registration restored from old DB
  if (policy_container_host_) {
    params->policy_container =
        policy_container_host_->CreatePolicyContainerForBlink();

    if (!client_security_state_) {
      client_security_state_ = network::mojom::ClientSecurityState::New();
    }
    client_security_state_->ip_address_space =
        policy_container_host_->ip_address_space();
    client_security_state_->is_web_secure_context =
        policy_container_host_->policies().is_web_secure_context;
    client_security_state_->private_network_request_policy =
        DerivePrivateNetworkRequestPolicy(
            policy_container_host_->policies(),
            PrivateNetworkRequestContext::kWorker);
  }

  embedded_worker_->Start(std::move(params),
                          base::BindOnce(&ServiceWorkerVersion::OnStartSent,
                                         weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::StartTimeoutTimer() {
  DCHECK(!timeout_timer_.IsRunning());

  if (embedded_worker_->devtools_attached()) {
    // Don't record the startup time metric once DevTools is attached.
    ClearTick(&start_time_);
    skip_recording_startup_time_ = true;
  } else {
    RestartTick(&start_time_);
    skip_recording_startup_time_ = false;
  }

  // Ping will be activated in OnScriptEvaluationStart.
  ping_controller_.Deactivate();

  timeout_timer_.Start(FROM_HERE, kTimeoutTimerDelay, this,
                       &ServiceWorkerVersion::OnTimeoutTimer);
}

void ServiceWorkerVersion::StopTimeoutTimer() {
  timeout_timer_.Stop();

  // Trigger update if worker is stale.
  if (!in_dtor_ && !stale_time_.is_null()) {
    ClearTick(&stale_time_);
    if (!update_timer_.IsRunning())
      ScheduleUpdate();
  }
}

void ServiceWorkerVersion::SetTimeoutTimerInterval(base::TimeDelta interval) {
  DCHECK(timeout_timer_.IsRunning());
  if (timeout_timer_.GetCurrentDelay() != interval) {
    timeout_timer_.Stop();
    timeout_timer_.Start(FROM_HERE, interval, this,
                         &ServiceWorkerVersion::OnTimeoutTimer);
  }
}

void ServiceWorkerVersion::OnTimeoutTimer() {
  // TODO(horo): This CHECK is for debugging crbug.com/759938.
  CHECK(running_status() == blink::EmbeddedWorkerStatus::kStarting ||
        running_status() == blink::EmbeddedWorkerStatus::kRunning ||
        running_status() == blink::EmbeddedWorkerStatus::kStopping)
      << static_cast<int>(running_status());

  if (!context_)
    return;

  MarkIfStale();

  // Global `this` protecter.
  // callbacks initiated by this function sometimes reduce refcnt to 0
  // to make this instance freed.
  scoped_refptr<ServiceWorkerVersion> protect_this(this);

  // Stopping the worker hasn't finished within a certain period.
  if (GetTickDuration(stop_time_) > kStopWorkerTimeout) {
    DCHECK_EQ(blink::EmbeddedWorkerStatus::kStopping, running_status());
    ReportError(blink::ServiceWorkerStatusCode::kErrorTimeout,
                "DETACH_STALLED_IN_STOPPING");

    embedded_worker_->RemoveObserver(this);
    embedded_worker_->Detach();
    embedded_worker_ = std::make_unique<EmbeddedWorkerInstance>(this);
    embedded_worker_->AddObserver(this);

    // Call OnStoppedInternal to fail callbacks and possibly restart.
    OnStoppedInternal(blink::EmbeddedWorkerStatus::kStopping);
    return;
  }

  // Trigger update if worker is stale and we waited long enough for it to go
  // idle.
  if (GetTickDuration(stale_time_) > kRequestTimeout) {
    ClearTick(&stale_time_);
    if (!update_timer_.IsRunning())
      ScheduleUpdate();
  }

  // Starting a worker hasn't finished within a certain period.
  base::TimeDelta start_limit = IsInstalled(status())
                                    ? kStartInstalledWorkerTimeout
                                    : kStartNewWorkerTimeout;

  if (IsWarmedUp()) {
    static const base::TimeDelta kStartLimit =
        blink::features::kSpeculativeServiceWorkerWarmUpDuration.Get();
    start_limit = kStartLimit;
  }

  if (GetTickDuration(start_time_) > start_limit) {
    DCHECK(running_status() == blink::EmbeddedWorkerStatus::kStarting ||
           running_status() == blink::EmbeddedWorkerStatus::kStopping)
        << static_cast<int>(running_status());
    FinishStartWorker(blink::ServiceWorkerStatusCode::kErrorTimeout);
    if (running_status() == blink::EmbeddedWorkerStatus::kStarting) {
      embedded_worker_->Stop();
    }
    return;
  }

  // Are there requests that have not finished before their expiration.
  bool has_kill_on_timeout = false;
  bool has_continue_on_timeout = false;
  // In case, `request_timeouts_` can be modified in the callbacks initiated
  // in `MaybeTimeoutRequest`, we keep its contents locally during the
  // following while loop.
  std::set<InflightRequestTimeoutInfo> request_timeouts;
  request_timeouts.swap(request_timeouts_);
  auto timeout_iter = request_timeouts.begin();
  while (timeout_iter != request_timeouts.end()) {
    const InflightRequestTimeoutInfo& info = *timeout_iter;
    if (!RequestExpired(info.expiration_time)) {
      break;
    }
    if (MaybeTimeoutRequest(info)) {
      switch (info.timeout_behavior) {
        case KILL_ON_TIMEOUT:
          has_kill_on_timeout = true;
          break;
        case CONTINUE_ON_TIMEOUT:
          has_continue_on_timeout = true;
          break;
      }
    }
    timeout_iter = request_timeouts.erase(timeout_iter);
  }
  // Ensure the `request_timeouts_` won't be touched during the loop.
  DCHECK(request_timeouts_.empty());
  request_timeouts_.swap(request_timeouts);
  // TODO(crbug.com/40864997): remove the following DCHECK when the cause
  // identified.
  DCHECK_EQ(request_timeouts_.size(), inflight_requests_.size());

  if (has_kill_on_timeout &&
      running_status() != blink::EmbeddedWorkerStatus::kStopping) {
    embedded_worker_->Stop();
  }

  // For the timeouts below, there are no callbacks to timeout so there is
  // nothing more to do if the worker is already stopping.
  if (running_status() == blink::EmbeddedWorkerStatus::kStopping) {
    return;
  }

  // If an request is expired and there is no other requests, we ask event
  // queue to check if idle timeout should be scheduled. Event queue may
  // schedule idle timeout if there is no events at the time.
  if (has_continue_on_timeout && !HasWorkInBrowser()) {
    endpoint()->ClearKeepAlive();
  }

  // Check ping status.
  ping_controller_.CheckPingStatus();
}

void ServiceWorkerVersion::PingWorker() {
  // TODO(horo): This CHECK is for debugging crbug.com/759938.
  CHECK(running_status() == blink::EmbeddedWorkerStatus::kStarting ||
        running_status() == blink::EmbeddedWorkerStatus::kRunning);
  // base::Unretained here is safe because endpoint() is owned by
  // |this|.
  endpoint()->Ping(base::BindOnce(&ServiceWorkerVersion::OnPongFromWorker,
                                  base::Unretained(this)));
}

void ServiceWorkerVersion::OnPingTimeout() {
  DCHECK(running_status() == blink::EmbeddedWorkerStatus::kStarting ||
         running_status() == blink::EmbeddedWorkerStatus::kRunning);
  MaybeReportConsoleMessageToInternals(
      blink::mojom::ConsoleMessageLevel::kVerbose, kNotRespondingErrorMesage);
  embedded_worker_->StopIfNotAttachedToDevTools();
}

void ServiceWorkerVersion::RecordStartWorkerResult(
    ServiceWorkerMetrics::EventType purpose,
    Status prestart_status,
    int trace_id,
    bool is_browser_startup_complete,
    blink::ServiceWorkerStatusCode status) {
  if (trace_id != kInvalidTraceId) {
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "ServiceWorker", "ServiceWorkerVersion::StartWorker",
        TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::StartWorker", trace_id),
        "Status", blink::ServiceWorkerStatusToString(status));
  }
  base::TimeTicks start_time = start_time_;
  ClearTick(&start_time_);

  if (context_ && IsInstalled(prestart_status))
    context_->UpdateVersionFailureCount(version_id_, status);

  if (IsInstalled(prestart_status))
    ServiceWorkerMetrics::RecordStartInstalledWorkerStatus(status, purpose);

  if (status == blink::ServiceWorkerStatusCode::kOk && !start_time.is_null() &&
      !skip_recording_startup_time_) {
    ServiceWorkerMetrics::RecordStartWorkerTime(
        GetTickDuration(start_time), IsInstalled(prestart_status),
        embedded_worker_->start_situation(), purpose);
  }

  if (status != blink::ServiceWorkerStatusCode::kErrorTimeout)
    return;
  EmbeddedWorkerInstance::StartingPhase phase =
      EmbeddedWorkerInstance::NOT_STARTING;
  blink::EmbeddedWorkerStatus running_status = embedded_worker_->status();
  // Build an artificial JavaScript exception to show in the ServiceWorker
  // log for developers; it's not user-facing so it's not a localized resource.
  std::string message = "ServiceWorker startup timed out. ";
  if (running_status != blink::EmbeddedWorkerStatus::kStarting) {
    message.append("The worker had unexpected status: ");
    message.append(EmbeddedWorkerInstance::StatusToString(running_status));
  } else {
    phase = embedded_worker_->starting_phase();
    message.append("The worker was in startup phase: ");
    message.append(EmbeddedWorkerInstance::StartingPhaseToString(phase));
  }
  message.append(".");
  OnReportException(base::UTF8ToUTF16(message), -1, -1, GURL());
  DVLOG(1) << message;
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.TimeoutPhase", phase,
                            EmbeddedWorkerInstance::STARTING_PHASE_MAX_VALUE);
}

bool ServiceWorkerVersion::MaybeTimeoutRequest(
    const InflightRequestTimeoutInfo& info) {
  InflightRequest* request = inflight_requests_.Lookup(info.id);
  if (!request)
    return false;

  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker",
                                  "ServiceWorkerVersion::Request",
                                  TRACE_ID_LOCAL(request), "Error", "Timeout");
  std::move(request->error_callback)
      .Run(blink::ServiceWorkerStatusCode::kErrorTimeout);
  inflight_requests_.Remove(info.id);
  return true;
}

void ServiceWorkerVersion::SetAllRequestExpirations(
    const base::TimeTicks& expiration_time) {
  std::set<InflightRequestTimeoutInfo> new_timeouts;
  for (const auto& info : request_timeouts_) {
    auto [iter, is_inserted] = new_timeouts.emplace(
        info.id, info.event_type,
        // Keep expiration that has `Max` value to avoid stop the worker after
        // `expiration_time`.
        info.expiration_time.is_max() ? info.expiration_time : expiration_time,
        info.timeout_behavior);
    DCHECK(is_inserted);
    InflightRequest* request = inflight_requests_.Lookup(info.id);
    DCHECK(request);
    request->timeout_iter = iter;
  }
  request_timeouts_.swap(new_timeouts);
  // TODO(crbug.com/40864997): remove the following DCHECK when the cause
  // identified.
  DCHECK_EQ(request_timeouts_.size(), inflight_requests_.size());
}

blink::ServiceWorkerStatusCode
ServiceWorkerVersion::DeduceStartWorkerFailureReason(
    blink::ServiceWorkerStatusCode default_code) {
  if (ping_controller_.IsTimedOut())
    return blink::ServiceWorkerStatusCode::kErrorTimeout;

  if (start_worker_status_ != blink::ServiceWorkerStatusCode::kOk)
    return start_worker_status_;

  int main_script_net_error = script_cache_map()->main_script_net_error();
  if (main_script_net_error != net::OK) {
    if (net::IsCertificateError(main_script_net_error))
      return blink::ServiceWorkerStatusCode::kErrorSecurity;
    switch (main_script_net_error) {
      case net::ERR_INSECURE_RESPONSE:
      case net::ERR_UNSAFE_REDIRECT:
        return blink::ServiceWorkerStatusCode::kErrorSecurity;
      case net::ERR_ABORTED:
        return blink::ServiceWorkerStatusCode::kErrorAbort;
      default:
        return blink::ServiceWorkerStatusCode::kErrorNetwork;
    }
  }

  return default_code;
}

net::Error ServiceWorkerVersion::GetMainScriptNetError() {
  return net::Error(script_cache_map()->main_script_net_error());
}

void ServiceWorkerVersion::MarkIfStale() {
  if (!context_)
    return;
  if (update_timer_.IsRunning() || !stale_time_.is_null())
    return;
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id_);
  if (!registration || registration->active_version() != this)
    return;
  base::TimeDelta time_since_last_check =
      clock_->Now() - registration->last_update_check();
  if (time_since_last_check >
      ServiceWorkerConsts::kServiceWorkerScriptMaxCacheAge)
    RestartTick(&stale_time_);
}

void ServiceWorkerVersion::FoundRegistrationForUpdate(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (!context_)
    return;

  const scoped_refptr<ServiceWorkerVersion> protect = this;
  if (is_update_scheduled_) {
    context_->UnprotectVersion(version_id_);
    is_update_scheduled_ = false;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk ||
      registration->active_version() != this)
    return;

  context_->UpdateServiceWorkerWithoutExecutionContext(
      registration.get(), false /* force_bypass_cache */);
}

void ServiceWorkerVersion::OnStoppedInternal(
    blink::EmbeddedWorkerStatus old_status) {
  TRACE_EVENT0("ServiceWorker", "ServiceWorkerVersion::OnStoppedInternal");
  DCHECK_EQ(blink::EmbeddedWorkerStatus::kStopped, running_status());
  scoped_refptr<ServiceWorkerVersion> protect;
  if (!in_dtor_)
    protect = this;

  // |start_callbacks_| can be non-empty if a start worker request arrived while
  // the worker was stopping. The worker must be restarted to fulfill the
  // request.
  bool should_restart = !start_callbacks_.empty();
  bool should_warm_up =
      will_warm_up_on_stopped_ && !is_stopping_warmed_up_worker_;
  if (is_redundant() || in_dtor_) {
    // This worker will be destroyed soon.
    should_restart = false;
    should_warm_up = false;
  } else if (ping_controller_.IsTimedOut()) {
    // This worker exhausted its time to run, don't let it restart.
    should_restart = false;
    should_warm_up = false;
  } else if (old_status == blink::EmbeddedWorkerStatus::kStarting) {
    // This worker unexpectedly stopped because start failed.  Attempting to
    // restart on start failure could cause an endless loop of start attempts,
    // so don't try to restart now.
    should_restart = false;
    should_warm_up = false;
  } else if (is_stopping_warmed_up_worker_) {
    // This worker is stopped while warmed-up or warming-up. Such workers don't
    // need to restart nor re-warm-up.
    should_restart = false;
    should_warm_up = false;
  }

  if (!stop_time_.is_null()) {
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "ServiceWorker", "ServiceWorkerVersion::StopWorker",
        TRACE_ID_WITH_SCOPE("ServiceWorkerVersion::StopWorker",
                            stop_time_.since_origin().InMicroseconds()),
        "Restart", should_restart);
    ClearTick(&stop_time_);
  }
  StopTimeoutTimer();

  // Fire all stop callbacks.
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(stop_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run();

  if (!should_restart) {
    // Let all start callbacks fail.
    FinishStartWorker(DeduceStartWorkerFailureReason(
        blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed));
  }

  // TODO(crbug.com/40864997): remove the following DCHECK when the cause
  // identified.
  // Failing this DCHECK means, the function is called while
  // the function is modifying contents of request_timeouts_.
  DCHECK(inflight_requests_.IsEmpty() || !request_timeouts_.empty());
  DCHECK_EQ(request_timeouts_.size(), inflight_requests_.size());

  // Let all message callbacks fail (this will also fire and clear all
  // callbacks for events).
  // TODO(kinuko): Consider if we want to add queue+resend mechanism here.
  base::IDMap<std::unique_ptr<InflightRequest>>::iterator iter(
      &inflight_requests_);
  while (!iter.IsAtEnd()) {
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "ServiceWorker", "ServiceWorkerVersion::Request",
        TRACE_ID_LOCAL(iter.GetCurrentValue()), "Error", "Worker Stopped");
    std::move(iter.GetCurrentValue()->error_callback)
        .Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    iter.Advance();
  }
  inflight_requests_.Clear();
  request_timeouts_.clear();
  external_request_uuid_to_request_id_.clear();
  service_worker_remote_.reset();
  is_endpoint_ready_ = false;
  remote_controller_.reset();
  DCHECK(!controller_receiver_.is_valid());
  installed_scripts_sender_.reset();
  receiver_.reset();
  associated_interface_receiver_.reset();
  associated_interface_provider_.reset();
  pending_external_requests_.clear();
  worker_is_idle_on_renderer_ = true;
  worker_host_.reset();
  will_warm_up_on_stopped_ = false;
  is_stopping_warmed_up_worker_ = false;

  for (auto& observer : observers_)
    observer.OnRunningStateChanged(this);
  if (should_restart) {
    StartWorkerInternal();
  } else if (!HasWorkInBrowser()) {
    OnNoWorkInBrowser();
  }

  if (should_warm_up && !should_restart && context_) {
    // Posts a re-warm-up task so that the warming up operation runs in a
    // different task.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<ServiceWorkerContextCore> context,
                          const GURL scope, const blink::StorageKey key) {
                         if (!context) {
                           return;
                         }
                         context->wrapper()->WarmUpServiceWorker(
                             scope, key, base::DoNothing());
                       },
                       context_, scope_, key_));
  }
}

void ServiceWorkerVersion::FinishStartWorker(
    blink::ServiceWorkerStatusCode status) {
  std::vector<StatusCallback> callbacks;
  callbacks.swap(start_callbacks_);
  is_running_start_callbacks_ = true;
  for (auto& callback : callbacks)
    std::move(callback).Run(status);
  is_running_start_callbacks_ = false;

  std::vector<StatusCallback> warm_up_callbacks;
  warm_up_callbacks.swap(warm_up_callbacks_);
  for (auto& callback : warm_up_callbacks) {
    std::move(callback).Run(status);
  }
}

void ServiceWorkerVersion::CleanUpExternalRequest(
    const base::Uuid& request_uuid,
    blink::ServiceWorkerStatusCode status) {
  if (status == blink::ServiceWorkerStatusCode::kOk)
    return;
  external_request_uuid_to_request_id_.erase(request_uuid);
}

void ServiceWorkerVersion::OnNoWorkInBrowser() {
  DCHECK(!HasWorkInBrowser());
  if (context_ && worker_is_idle_on_renderer_) {
    scoped_refptr<ServiceWorkerRegistration> registration =
        context_->GetLiveRegistration(registration_id());
    if (registration)
      registration->OnNoWork(this);

    for (auto& observer : observers_)
      observer.OnNoWork(this);
  }
}

bool ServiceWorkerVersion::IsStartWorkerAllowed() const {
  // Check that the worker is allowed on this origin. It's possible a
  // worker was previously allowed and installed, but later the embedder's
  // policy or binary changed to disallow this origin.
  if (!service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          {script_url_})) {
    return false;
  }

  auto* browser_context = context_->wrapper()->browser_context();
  // Check that the browser context is not nullptr.  It becomes nullptr
  // when the service worker process manager is being shutdown.
  if (!browser_context) {
    return false;
  }

  // Check that the worker is allowed on the given scope. It's possible a worker
  // was previously allowed and installed, but later content settings changed to
  // disallow this scope. Since this worker might not be used for a specific
  // tab, pass a null callback as WebContents getter.
  if (!GetContentClient()->browser()->AllowServiceWorker(
          scope_, net::SiteForCookies::FromUrl(scope_),
          url::Origin::Create(scope_), script_url_, browser_context)) {
    return false;
  }

  return true;
}

void ServiceWorkerVersion::NotifyControlleeAdded(
    const std::string& uuid,
    const ServiceWorkerClientInfo& info) {
  if (context_)
    context_->OnControlleeAdded(this, uuid, info);
}

void ServiceWorkerVersion::NotifyControlleeRemoved(const std::string& uuid) {
  if (!context_)
    return;

  // The OnNoControllees() can destroy |this|, so protect it first.
  auto protect = base::WrapRefCounted(this);
  context_->OnControlleeRemoved(this, uuid);
  if (!HasControllee()) {
    RestartTick(&no_controllees_time_);
    context_->OnNoControllees(this);
  }
}

void ServiceWorkerVersion::NotifyControlleeNavigationCommitted(
    const std::string& uuid,
    GlobalRenderFrameHostId render_frame_host_id) {
  if (context_)
    context_->OnControlleeNavigationCommitted(this, uuid, render_frame_host_id);
}

void ServiceWorkerVersion::NotifyWindowOpened(const GURL& script_url,
                                              const GURL& url) {
  if (context_) {
    context_->OnWindowOpened(script_url, url);
  }
}

void ServiceWorkerVersion::NotifyClientNavigated(const GURL& script_url,
                                                 const GURL& url) {
  if (context_) {
    context_->OnClientNavigated(script_url, url);
  }
}

void ServiceWorkerVersion::PrepareForUpdate(
    std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>
        compared_script_info_map,
    const GURL& updated_script_url,
    scoped_refptr<PolicyContainerHost> policy_container_host) {
  compared_script_info_map_ = std::move(compared_script_info_map);
  updated_script_url_ = updated_script_url;
  if (!GetContentClient()
           ->browser()
           ->ShouldServiceWorkerInheritPolicyContainerFromCreator(
               updated_script_url)) {
    set_policy_container_host(policy_container_host);
  }
}

const std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>&
ServiceWorkerVersion::compared_script_info_map() const {
  return compared_script_info_map_;
}

ServiceWorkerUpdateChecker::ComparedScriptInfo
ServiceWorkerVersion::TakeComparedScriptInfo(const GURL& script_url) {
  auto it = compared_script_info_map_.find(script_url);
  CHECK(it != compared_script_info_map_.end(), base::NotFatalUntil::M130);
  ServiceWorkerUpdateChecker::ComparedScriptInfo info = std::move(it->second);
  compared_script_info_map_.erase(it);
  return info;
}

bool ServiceWorkerVersion::ShouldRequireForegroundPriority(
    int worker_process_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Currently FetchEvents are the only type of event we need to really process
  // at foreground priority.  If the service worker does not have a FetchEvent
  // handler then we can always allow it to go to the background.
  if (fetch_handler_existence() != FetchHandlerExistence::EXISTS)
    return false;

  // Keep the service worker at foreground priority if it has clients from
  // different foreground processes.  In this situation we are likely to need to
  // quickly service FetchEvents when the worker's process does not have any
  // visible windows and would have otherwise been moved to the background.
  //
  // For now the requirement for cross-process clients should filter out most
  // service workers.  The impact of foreground service workers is further
  // limited by the automatic shutdown mechanism.
  for (const auto& controllee : controllee_map_) {
    const int controllee_process_id = controllee.second->GetProcessId();
    RenderProcessHost* render_host =
        RenderProcessHost::FromID(controllee_process_id);

    // It's possible that |controllee_process_id| and |render_host| won't be
    // valid until the controllee commits. Require foreground priority in this
    // case.
    if (!render_host)
      return true;

    // Require foreground if the controllee is in different process and is
    // foreground.
    if (controllee_process_id != worker_process_id &&
        render_host->GetPriority() != base::Process::Priority::kBestEffort) {
      return true;
    }
  }
  return false;
}

void ServiceWorkerVersion::UpdateForegroundPriority() {
  embedded_worker_->UpdateForegroundPriority();
}

void ServiceWorkerVersion::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel message_level,
    const std::string& message) {
  if (running_status() == blink::EmbeddedWorkerStatus::kStarting ||
      running_status() == blink::EmbeddedWorkerStatus::kRunning) {
    endpoint()->AddMessageToConsole(message_level, message);
  }
}

void ServiceWorkerVersion::MaybeReportConsoleMessageToInternals(
    blink::mojom::ConsoleMessageLevel message_level,
    const std::string& message) {
  // When the internals UI page is opened, the page listens to
  // OnReportConsoleMessage().
  OnReportConsoleMessage(blink::mojom::ConsoleMessageSource::kOther,
                         message_level, base::UTF8ToUTF16(message), -1,
                         script_url_);
}

storage::mojom::ServiceWorkerLiveVersionInfoPtr
ServiceWorkerVersion::RebindStorageReference() {
  DCHECK(context_);

  std::vector<int64_t> purgeable_resources;
  // Resources associated with this version are purgeable when the corresponding
  // registration is uninstalling or uninstalled.
  switch (registration_status_) {
    case ServiceWorkerRegistration::Status::kIntact:
      break;
    case ServiceWorkerRegistration::Status::kUninstalling:
    case ServiceWorkerRegistration::Status::kUninstalled:
      for (auto& resource : script_cache_map_.GetResources()) {
        purgeable_resources.push_back(resource->resource_id);
      }
      break;
  }

  remote_reference_.reset();
  return storage::mojom::ServiceWorkerLiveVersionInfo::New(
      version_id_, std::move(purgeable_resources),
      remote_reference_.BindNewPipeAndPassReceiver());
}

void ServiceWorkerVersion::SetResources(
    const std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>&
        resources) {
  DCHECK_EQ(status_, NEW);
  DCHECK(!sha256_script_checksum_);
  script_cache_map_.SetResources(resources);
  sha256_script_checksum_ = MergeResourceRecordSHA256ScriptChecksum(
      script_url_, script_cache_map_, fetch_handler_type_);
}

ServiceWorkerRouterEvaluatorErrorEnums
ServiceWorkerVersion::SetupRouterEvaluator(
    const blink::ServiceWorkerRouterRules& rules) {
  CHECK(IsStaticRouterEnabled());
  blink::ServiceWorkerRouterRules new_rules;
  // If there are existing router rules, set them first.
  // TODO(crbug.com/40277030) Consider having a method to merge rules instead of
  // replacing each time.
  if (router_evaluator()) {
    for (const auto& e : router_evaluator()->rules().rules) {
      new_rules.rules.push_back(e);
    }
  }
  for (const auto& e : rules.rules) {
    new_rules.rules.push_back(e);
  }
  router_evaluator_ = std::make_unique<ServiceWorkerRouterEvaluator>(new_rules);
  if (!router_evaluator_->IsValid()) {
    auto error = router_evaluator_->invalid_error_code();
    CHECK(error.has_value());
    router_evaluator_.reset();
    return *error;
  }
  CHECK_NE(fetch_handler_existence(), FetchHandlerExistence::UNKNOWN);

  // Check if we have fetch handler. This is a rare case, since this should have
  // been validated in the renderer already when adding a new router rule.
  if (router_evaluator_->has_fetch_event_source() &&
      fetch_handler_existence() == FetchHandlerExistence::DOES_NOT_EXIST) {
    router_evaluator_.reset();
    return ServiceWorkerRouterEvaluatorErrorEnums::
        kFetchSourceWithoutFetchHandler;
  }
  return ServiceWorkerRouterEvaluatorErrorEnums::kNoError;
}

bool ServiceWorkerVersion::NeedRouterEvaluate() const {
  // If there's no router, return false.
  if (!router_evaluator_) {
    return false;
  }
  // If the router has non fetch-event source e.g. cache, we can't skip the
  // router evaluate.
  if (router_evaluator_->has_non_fetch_event_source()) {
    return true;
  }
  // In this case, there are router rules, but all sources are "fetch-event".
  switch (fetch_handler_type()) {
    case FetchHandlerType::kNoHandler:
      // If there's no fetch handler, we can skip the router evaluate because
      // the router evaluation will be no-op.
      return false;
    case FetchHandlerType::kEmptyFetchHandler:
    case FetchHandlerType::kNotSkippable:
      return true;
  }
}

bool ServiceWorkerVersion::IsStaticRouterEnabled() {
  if (base::FeatureList::IsEnabled(features::kServiceWorkerStaticRouter)) {
    return true;
  }
  if (origin_trial_tokens_ &&
      origin_trial_tokens_->contains("ServiceWorkerStaticRouter")) {
    return true;
  }
  return false;
}

void ServiceWorkerVersion::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  // `associated_interface_provider_receiver_` and `associated_registry_` are
  // both reset at the same time, so we should never get a request for an
  // associated interface when `associated_registry_` is not valid.
  DCHECK(associated_registry_);

  mojo::ScopedInterfaceEndpointHandle handle = receiver.PassHandle();
  associated_registry_->TryBindInterface(name, &handle);
}

bool ServiceWorkerVersion::BFCacheContainsControllee(
    const std::string& uuid) const {
  return base::Contains(bfcached_controllee_map_, uuid);
}

base::WeakPtr<ServiceWorkerVersion> ServiceWorkerVersion::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

mojo::PendingRemote<blink::mojom::CacheStorage>
ServiceWorkerVersion::GetRemoteCacheStorage() {
  auto* control = GetCacheStorageControl(*this);
  if (!control) {
    return mojo::NullRemote();
  }

  // Since this is offloading the cache storage API access in ServiceWorker,
  // we need to follow COEP used there.
  // The reason why COEP is enforced to the cache storage API can be seen in:
  // crbug.com/991428.
  const network::CrossOriginEmbedderPolicy* coep =
      cross_origin_embedder_policy();
  if (!coep) {
    return mojo::NullRemote();
  }

  // Similarly, DIP should be passed to cache storage to enforce it.
  const network::DocumentIsolationPolicy* dip = document_isolation_policy();
  if (!dip) {
    return mojo::NullRemote();
  }

  mojo::PendingRemote<blink::mojom::CacheStorage> remote;
  control->AddReceiver(*coep, embedded_worker()->GetCoepReporter(), *dip,
                       storage::BucketLocator::ForDefaultBucket(key()),
                       storage::mojom::CacheStorageOwner::kCacheAPI,
                       remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerVersion::GetControllerMode() const {
  switch (fetch_handler_existence()) {
    case ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST:
      return blink::mojom::ControllerServiceWorkerMode::kNoFetchEventHandler;
    case ServiceWorkerVersion::FetchHandlerExistence::EXISTS:
      return blink::mojom::ControllerServiceWorkerMode::kControlled;
    case ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN:
      // UNKNOWN means the controller is still installing. It's not possible to
      // have a controller that hasn't finished installing.
      NOTREACHED_IN_MIGRATION();
      return blink::mojom::ControllerServiceWorkerMode::kNoController;
  }
}

}  // namespace content
