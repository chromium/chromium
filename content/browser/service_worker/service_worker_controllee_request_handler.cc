// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_controllee_request_handler.h"

#include <optional>
#include <set>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_main_resource_loader.h"
#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/service_worker/service_worker_loader_helpers.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "components/offline_pages/core/request_header/offline_page_header.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

namespace content {

namespace {

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
// A web page, regardless of whether the service worker is used or not, could
// be downloaded with the offline snapshot captured. The user can then open
// the downloaded page which is identified by the presence of a specific
// offline header in the network request. In this case, we want to fall back
// in order for the subsequent offline page interceptor to bring up the
// offline snapshot of the page.
bool ShouldFallbackToLoadOfflinePage(
    const net::HttpRequestHeaders& extra_request_headers) {
  std::optional<std::string> offline_header_value =
      extra_request_headers.GetHeader(offline_pages::kOfflinePageHeader);
  if (!offline_header_value) {
    return false;
  }
  offline_pages::OfflinePageHeader offline_header(*offline_header_value);
  return offline_header.reason !=
             offline_pages::OfflinePageHeader::Reason::NONE &&
         offline_header.reason !=
             offline_pages::OfflinePageHeader::Reason::RELOAD;
}
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

void RecordSkipReason(
    ServiceWorkerControlleeRequestHandler::FetchHandlerSkipReason skip_reason) {
  base::UmaHistogramEnumeration("ServiceWorker.FetchHandler.SkipReason",
                                skip_reason);
}

const char* FetchHandlerTypeToString(
    ServiceWorkerVersion::FetchHandlerType type) {
  switch (type) {
    case ServiceWorkerVersion::FetchHandlerType::kNoHandler:
      return "no handler";
    case ServiceWorkerVersion::FetchHandlerType::kNotSkippable:
      return "not skippable";
    case ServiceWorkerVersion::FetchHandlerType::kEmptyFetchHandler:
      return "empty fetch handler";
  }
}
}  // namespace

std::optional<int> ServiceWorkerControlleeRequestHandler::
    start_service_worker_for_empty_fetch_handler_duration_for_testing_;

ServiceWorkerControlleeRequestHandler::ServiceWorkerControlleeRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerClient> service_worker_client,
    network::mojom::RequestDestination destination,
    bool skip_service_worker,
    FrameTreeNodeId frame_tree_node_id,
    ServiceWorkerAccessedCallback service_worker_accessed_callback)
    : context_(std::move(context)),
      service_worker_client_(std::move(service_worker_client)),
      destination_(destination),
      skip_service_worker_(skip_service_worker),
      force_update_started_(false),
      frame_tree_node_id_(frame_tree_node_id),
      service_worker_accessed_callback_(
          std::move(service_worker_accessed_callback)) {
  DCHECK(
      blink::ServiceWorkerLoaderHelpers::IsMainRequestDestination(destination));
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerControlleeRequestHandler::"
                         "ServiceWorkerControlleeRequestHandler",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_OUT);
}

ServiceWorkerControlleeRequestHandler::
    ~ServiceWorkerControlleeRequestHandler() {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerControlleeRequestHandler::"
                         "~ServiceWorkerControlleeRequestHandler",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
  MaybeScheduleUpdate();
}

void ServiceWorkerControlleeRequestHandler::MaybeScheduleUpdate() {
  if (!service_worker_client_ || !service_worker_client_->controller()) {
    return;
  }

  // For navigations, the update logic is taken care of
  // during navigation and waits for the HintToUpdateServiceWorker message.
  if (blink::IsRequestDestinationFrame(destination_)) {
    return;
  }

  // For shared workers. The renderer doesn't yet send a
  // HintToUpdateServiceWorker message.
  // TODO(falken): Make the renderer send the message for shared worker,
  // to simplify the code.

  // If DevTools forced an update, there is no need to update again.
  if (force_update_started_) {
    return;
  }

  service_worker_client_->controller()->ScheduleUpdate();
}

void ServiceWorkerControlleeRequestHandler::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    const blink::StorageKey& storage_key,
    BrowserContext* browser_context,
    NavigationLoaderInterceptor::LoaderCallback loader_callback,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback) {
  loader_callback_ = std::move(loader_callback);
  fallback_callback_ = std::move(fallback_callback);

  if (!service_worker_client_) {
    // We can't do anything other than to fall back to network.
    CompleteWithoutLoader();
    return;
  }

  // Update the host. This is important to do before falling back to network
  // below, so service worker APIs still work even if the service worker is
  // bypassed for request interception.
  InitializeServiceWorkerClient(tentative_resource_request, storage_key);

  // Fall back to network if we were instructed to bypass the service worker for
  // request interception, or if the context is gone so we have to bypass
  // anyway.
  if (skip_service_worker_ || !context_) {
    ServiceWorkerMetrics::RecordSkipServiceWorkerOnNavigation(true);
    CompleteWithoutLoader();
    return;
  }
  ServiceWorkerMetrics::RecordSkipServiceWorkerOnNavigation(false);

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // Fall back for the subsequent offline page interceptor to load the offline
  // snapshot of the page if required.
  //
  // TODO(crbug.com/40590410): Figure out how offline page interception should
  // interact with URLLoaderThrottles. It might be incorrect to use
  // |tentative_resource_request.headers| here, since throttles can rewrite
  // headers between now and when the request handler passed to
  // |loader_callback_| is invoked.
  if (ShouldFallbackToLoadOfflinePage(tentative_resource_request.headers)) {
    CompleteWithoutLoader();
    return;
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  // TODO(bashi): Consider using a global navigation ID instead of using |this|.
  // Using a global ID gives us a convenient way to analyze event flows across
  // classes.
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerControlleeRequestHandler::MaybeCreateLoader",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "URL",
      tentative_resource_request.url.spec());

  // Look up a registration.
  context_->registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNavigation, stripped_url_, storage_key_,
      base::BindOnce(
          &ServiceWorkerControlleeRequestHandler::ContinueWithRegistration,
          weak_factory_.GetWeakPtr(), /*is_for_navigation=*/true,
          base::TimeTicks::Now()));
}

void ServiceWorkerControlleeRequestHandler::InitializeServiceWorkerClient(
    const network::ResourceRequest& tentative_resource_request,
    const blink::StorageKey& storage_key) {
  // Update the container host with this request, clearing old controller state
  // if this is a redirect.
  service_worker_client_->SetControllerRegistration(
      nullptr,
      /*notify_controllerchange=*/false);
  stripped_url_ = net::SimplifyUrlForRequest(tentative_resource_request.url);

  storage_key_ = storage_key;

  service_worker_client_->UpdateUrls(
      stripped_url_,
      // The storage key only has a top_level_site, not
      // an origin, so we must extract the origin from
      // trusted_params.
      tentative_resource_request.trusted_params
          ? tentative_resource_request.trusted_params->isolation_info
                .top_frame_origin()
          : std::nullopt,
      storage_key_);
}

void ServiceWorkerControlleeRequestHandler::ContinueWithRegistration(
    bool is_for_navigation,
    base::TimeTicks find_registration_start_time,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (is_for_navigation) {
    DCHECK(!find_registration_start_time.is_null());
    auto now = base::TimeTicks::Now();

    ServiceWorkerMetrics::RecordFindRegistrationForClientUrlTime(
        now - find_registration_start_time);

    base::UmaHistogramBoolean(
        "ServiceWorker.FoundServiceWorkerRegistrationOnNavigation",
        status == blink::ServiceWorkerStatusCode::kOk);

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "ServiceWorker",
        "ServiceWorker.MaybeCreateLoaderToContinueWithRegistration",
        TRACE_ID_LOCAL(this), find_registration_start_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "ServiceWorker",
        "ServiceWorker.MaybeCreateLoaderToContinueWithRegistration",
        TRACE_ID_LOCAL(this), now);
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::ContinueWithRegistration",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Status",
        blink::ServiceWorkerStatusToString(status));
    CompleteWithoutLoader();
    return;
  }
  DCHECK(registration);

  if (!service_worker_client_) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::ContinueWithRegistration",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "No container");
    CompleteWithoutLoader();
    return;
  }
  service_worker_client_->AddMatchingRegistration(registration.get());

  if (!context_) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::ContinueWithRegistration",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "No Context");
    CompleteWithoutLoader();
    return;
  }

  AllowServiceWorkerResult allow_service_worker =
      GetContentClient()->browser()->AllowServiceWorker(
          registration->scope(),
          service_worker_security_utils::site_for_cookies(
              service_worker_client_->key()),
          service_worker_client_->top_frame_origin(), /*script_url=*/GURL(),
          context_->wrapper()->browser_context());

  service_worker_accessed_callback_.Run(registration->scope(),
                                        allow_service_worker);

  if (!allow_service_worker) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::ContinueWithRegistration",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "ServiceWorker is blocked");
    CompleteWithoutLoader();
    return;
  }

  if (!service_worker_client_->IsEligibleForServiceWorkerController()) {
    // TODO(falken): Figure out a way to surface in the page's DevTools
    // console that the service worker was blocked for security.
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::ContinueWithRegistration",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "Insecure context");
    CompleteWithoutLoader();
    return;
  }

  // Some registrations can't update like a web service worker (e.g. extension
  // service workers are updated separately through the extensions system, along
  // with the rest of the extension).
  const bool can_update =
      GetContentClient()->browser()->ShouldTryToUpdateServiceWorkerRegistration(
          registration->scope(), context_->wrapper()->browser_context());
  const bool need_to_update = !force_update_started_ &&
                              context_->force_update_on_page_load() &&
                              can_update;
  if (need_to_update) {
    force_update_started_ = true;
    context_->UpdateServiceWorker(
        registration.get(), true /* force_bypass_cache */,
        true /* skip_script_comparison */,
        // Passing an empty outside fetch client settings object as there is no
        // associated execution context.
        blink::mojom::FetchClientSettingsObject::New(),
        base::BindOnce(
            &ServiceWorkerControlleeRequestHandler::DidUpdateRegistration,
            weak_factory_.GetWeakPtr(), registration));
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::ContinueWithRegistration",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "Need to update");
    return;
  }

  // Initiate activation of a waiting version. Usually a register job initiates
  // activation but that doesn't happen if the browser exits prior to activation
  // having occurred. This check handles that case.
  if (registration->waiting_version()) {
    registration->ActivateWaitingVersionWhenReady();
  }

  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();
  if (!active_version) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::ContinueWithRegistration",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "No active version, so falling back to network");
    CompleteWithoutLoader();
    return;
  }

  DCHECK(active_version->status() == ServiceWorkerVersion::ACTIVATING ||
         active_version->status() == ServiceWorkerVersion::ACTIVATED)
      << ServiceWorkerVersion::VersionStatusToString(active_version->status());
  // Wait until it's activated before firing fetch events.
  if (active_version->status() == ServiceWorkerVersion::ACTIVATING) {
    registration->active_version()->RegisterStatusChangeCallback(base::BindOnce(
        &ServiceWorkerControlleeRequestHandler::ContinueWithActivatedVersion,
        weak_factory_.GetWeakPtr(), registration, active_version,
        std::move(find_registration_start_time)));
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::ContinueWithRegistration",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "Wait until finished SW activation");
    return;
  }

  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerControlleeRequestHandler::ContinueWithRegistration",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  ContinueWithActivatedVersion(std::move(registration),
                               std::move(active_version),
                               std::move(find_registration_start_time));
}

void ServiceWorkerControlleeRequestHandler::ContinueWithActivatedVersion(
    scoped_refptr<ServiceWorkerRegistration> registration,
    scoped_refptr<ServiceWorkerVersion> active_version,
    base::TimeTicks find_registration_start_time) {
  if (!context_ || !service_worker_client_) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::ContinueWithActivatedVersion",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "The context or container host is gone, so falling back to network");
    CompleteWithoutLoader();
    return;
  }

  if (active_version->status() != ServiceWorkerVersion::ACTIVATED) {
    // TODO(falken): Clean this up and clarify in what cases we come here. I
    // guess it's:
    // - strange system error cases where promoting from ACTIVATING to ACTIVATED
    //   failed (shouldn't happen)
    // - something calling Doom(), etc, making the active_version REDUNDANT
    // - a version called skipWaiting() during activation so the expected
    //   version is no longer the active one (shouldn't happen: skipWaiting()
    //   waits for the active version to finish activating).
    // In most cases, it sounds like falling back to network would not be right,
    // since it's still in-scope. We probably should do:
    //   1) If the container host has an active version that is ACTIVATED, just
    //      use that, even if it wasn't the expected one.
    //   2) If the container host has an active version that is not ACTIVATED,
    //      just fail the load. The correct thing is probably to re-try
    //      activating that version, but there's a risk of an infinite loop of
    //      retries.
    //   3) If the container host does not have an active version, just fail the
    //      load.
    TRACE_EVENT_WITH_FLOW2(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::ContinueWithActivatedVersion",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "The expected active version is not ACTIVATED, so falling back to "
        "network",
        "Status",
        ServiceWorkerVersion::VersionStatusToString(active_version->status()));
    CompleteWithoutLoader();
    return;
  }

  service_worker_client_->SetControllerRegistration(
      registration, false /* notify_controllerchange */);

  DCHECK_EQ(active_version, registration->active_version());
  DCHECK_EQ(active_version, service_worker_client_->controller());
  DCHECK_NE(active_version->fetch_handler_existence(),
            ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN);

  base::UmaHistogramEnumeration(
      "ServiceWorker.FetchHandler."
      "TypeAtContinueWithActivatedVersion",
      active_version->fetch_handler_type());

  if (blink::IsRequestDestinationFrame(destination_)) {
    service_worker_client_->AddServiceWorkerToUpdate(active_version);
  }

  // If the router evaluation is needed, always forward to the service worker.
  // Because the router evaluation is done in ServiceWorkerMainResourceLoader.
  if (active_version->NeedRouterEvaluate()) {
    CreateLoaderAndStartRequest(std::move(find_registration_start_time));
    return;
  }

  switch (active_version->fetch_handler_type()) {
    case ServiceWorkerVersion::FetchHandlerType::kNoHandler: {
      RecordSkipReason(FetchHandlerSkipReason::kNoFetchHandler);
      TRACE_EVENT_WITH_FLOW1(
          "ServiceWorker",
          "ServiceWorkerControlleeRequestHandler::ContinueWithActivatedVersion",
          TRACE_ID_LOCAL(this),
          TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
          "Skipping the ServiceWorker which has no fetch handler");
      CompleteWithoutLoader();
      return;
    }
    case ServiceWorkerVersion::FetchHandlerType::kEmptyFetchHandler: {
      RecordSkipReason(FetchHandlerSkipReason::kSkippedForEmptyFetchHandler);
      TRACE_EVENT_WITH_FLOW2(
          "ServiceWorker",
          "ServiceWorkerControlleeRequestHandler::ContinueWithActivatedVersion",
          TRACE_ID_LOCAL(this),
          TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
          "The fetch handler is skippable. Falling back to network",
          "FetchHandlerType",
          FetchHandlerTypeToString(active_version->fetch_handler_type()));
      active_version->CountFeature(
          blink::mojom::WebFeature::kServiceWorkerSkippedForEmptyFetchHandler);
      CompleteWithoutLoader();

      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &ServiceWorkerControlleeRequestHandler::MaybeStartServiceWorker,
              std::move(active_version),
              ServiceWorkerMetrics::EventType::SKIP_EMPTY_FETCH_HANDLER,
              reinterpret_cast<uintptr_t>(this)),
          base::Milliseconds(GetServiceWorkerForEmptyFetchHandlerDurationMs()));
      return;
    }
    case ServiceWorkerVersion::FetchHandlerType::kNotSkippable: {
      // Otherwise, record the skip reason as kNotSkipped.
      RecordSkipReason(FetchHandlerSkipReason::kNotSkipped);
      TRACE_EVENT_WITH_FLOW1(
          "ServiceWorker",
          "ServiceWorkerControlleeRequestHandler::ContinueWithActivatedVersion",
          TRACE_ID_LOCAL(this),
          TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
          "Forwarding to the ServiceWorker");
      break;
    }
  }

  // Finally, we want to forward to the service worker! Make a
  // ServiceWorkerMainResourceLoader which does that work.
  CreateLoaderAndStartRequest(std::move(find_registration_start_time));
}

void ServiceWorkerControlleeRequestHandler::CreateLoaderAndStartRequest(
    base::TimeTicks find_registration_start_time) {
  loader_wrapper_ = std::make_unique<ServiceWorkerMainResourceLoaderWrapper>(
      std::make_unique<ServiceWorkerMainResourceLoader>(
          std::move(fallback_callback_), service_worker_client_,
          frame_tree_node_id_, std::move(find_registration_start_time)));
  loader_wrapper_->get()->set_worker_parent_client_uuid(parent_client_uuid_);
  std::move(loader_callback_)
      .Run(NavigationLoaderInterceptor::Result(
          base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
              base::BindOnce(&ServiceWorkerMainResourceLoader::StartRequest,
                             loader_wrapper_->get()->AsWeakPtr())),
          SubresourceLoaderParams()));
}

void ServiceWorkerControlleeRequestHandler::DidStartWorker(
    uintptr_t trace_id,
    blink::ServiceWorkerStatusCode status) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerControlleeRequestHandler::DidStartWorker",
      TRACE_ID_LOCAL(trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Status",
      blink::ServiceWorkerStatusToString(status));
}

void ServiceWorkerControlleeRequestHandler::DidUpdateRegistration(
    scoped_refptr<ServiceWorkerRegistration> original_registration,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  DCHECK(force_update_started_);

  if (!context_) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::DidUpdateRegistration",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "The context is gone in DidUpdateRegistration");
    CompleteWithoutLoader();
    return;
  }
  if (status != blink::ServiceWorkerStatusCode::kOk ||
      !original_registration->installing_version()) {
    // Update failed. Look up the registration again since the original
    // registration was possibly unregistered in the meantime.
    context_->registry()->FindRegistrationForClientUrl(
        ServiceWorkerRegistry::Purpose::kNotForNavigation, stripped_url_,
        storage_key_,
        base::BindOnce(
            &ServiceWorkerControlleeRequestHandler::ContinueWithRegistration,
            weak_factory_.GetWeakPtr(),
            /*is_for_navigation=*/false, base::TimeTicks()));
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::DidUpdateRegistration",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "Update failed, look up the registration again");
    return;
  }

  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerControlleeRequestHandler::DidUpdateRegistration",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  DCHECK_EQ(original_registration->id(), registration_id);
  ServiceWorkerVersion* new_version =
      original_registration->installing_version();
  new_version->ReportForceUpdateToDevTools();
  new_version->set_skip_waiting(true);
  new_version->RegisterStatusChangeCallback(base::BindOnce(
      &ServiceWorkerControlleeRequestHandler::OnUpdatedVersionStatusChanged,
      weak_factory_.GetWeakPtr(), std::move(original_registration),
      base::WrapRefCounted(new_version)));
}

void ServiceWorkerControlleeRequestHandler::OnUpdatedVersionStatusChanged(
    scoped_refptr<ServiceWorkerRegistration> registration,
    scoped_refptr<ServiceWorkerVersion> version) {
  if (!context_) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::OnUpdatedVersionStatusChanged",
        TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Info",
        "The context is gone in OnUpdatedVersionStatusChanged");
    CompleteWithoutLoader();
    return;
  }

  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerControlleeRequestHandler::OnUpdatedVersionStatusChanged",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (version->status() == ServiceWorkerVersion::ACTIVATED ||
      version->status() == ServiceWorkerVersion::REDUNDANT) {
    // When the status is REDUNDANT, the update failed (eg: script error), we
    // continue with the incumbent version.
    // In case unregister job may have run, look up the registration again.
    context_->registry()->FindRegistrationForClientUrl(
        ServiceWorkerRegistry::Purpose::kNotForNavigation, stripped_url_,
        storage_key_,
        base::BindOnce(
            &ServiceWorkerControlleeRequestHandler::ContinueWithRegistration,
            weak_factory_.GetWeakPtr(),
            /*is_for_navigation=*/false, base::TimeTicks()));
    return;
  }
  version->RegisterStatusChangeCallback(base::BindOnce(
      &ServiceWorkerControlleeRequestHandler::OnUpdatedVersionStatusChanged,
      weak_factory_.GetWeakPtr(), std::move(registration), version));
}

void ServiceWorkerControlleeRequestHandler::CompleteWithoutLoader() {
  fallback_callback_.Reset();
  ServiceWorkerMainResourceLoaderInterceptor::CompleteWithoutLoader(
      std::move(loader_callback_), service_worker_client_);
}

void ServiceWorkerControlleeRequestHandler::MaybeStartServiceWorker(
    scoped_refptr<ServiceWorkerVersion> active_version,
    ServiceWorkerMetrics::EventType event_type,
    uintptr_t trace_id) {
  // Start service worker if it is not running so that we run the code
  // written in the top level.
  if (active_version->running_status() ==
      blink::EmbeddedWorkerStatus::kRunning) {
    return;
  }
  active_version->StartWorker(
      event_type,
      base::BindOnce(&ServiceWorkerControlleeRequestHandler::DidStartWorker,
                     trace_id));
}

int ServiceWorkerControlleeRequestHandler::
    GetServiceWorkerForEmptyFetchHandlerDurationMs() {
  if (start_service_worker_for_empty_fetch_handler_duration_for_testing_) {
    return start_service_worker_for_empty_fetch_handler_duration_for_testing_
        .value();
  }
  return kStartServiceWorkerForEmptyFetchHandlerDurationInMs;
}

void ServiceWorkerControlleeRequestHandler::
    SetStartServiceWorkerForEmptyFetchHandlerDurationForTesting(int duration) {
  start_service_worker_for_empty_fetch_handler_duration_for_testing_ = duration;
}

}  // namespace content
