// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_event_dispatcher.h"

#include <map>
#include <sstream>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_service_impl.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

// Returns a human-readable string for the given |event| type.
std::string EventTypeToString(ServiceWorkerMetrics::EventType event) {
  switch (event) {
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_ABORT:
      return "BackgroundFetchAbortEvent";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_CLICK:
      return "BackgroundFetchClickEvent";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_FAIL:
      return "BackgroundFetchFailEvent";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_SUCCESS:
      return "BackgroundFetchSuccessEvent";
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

}  // namespace

BackgroundFetchEventDispatcher::BackgroundFetchEventDispatcher(
    BackgroundFetchContext* background_fetch_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    DevToolsBackgroundServicesContextImpl& devtools_context)
    : background_fetch_context_(background_fetch_context),
      service_worker_context_(std::move(service_worker_context)),
      devtools_context_(&devtools_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(background_fetch_context_);
}

BackgroundFetchEventDispatcher::~BackgroundFetchEventDispatcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BackgroundFetchEventDispatcher::DispatchBackgroundFetchCompletionEvent(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchRegistrationDataPtr registration_data,
    base::OnceClosure finished_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(registration_data);

  auto registration = blink::mojom::BackgroundFetchRegistration::New(
      std::move(registration_data),
      BackgroundFetchRegistrationServiceImpl::CreateInterfaceInfo(
          registration_id, background_fetch_context_->GetWeakPtr()));

  switch (registration->registration_data->failure_reason) {
    case blink::mojom::BackgroundFetchFailureReason::NONE:
      DCHECK_EQ(registration->registration_data->result,
                blink::mojom::BackgroundFetchResult::SUCCESS);
      DispatchBackgroundFetchSuccessEvent(registration_id,
                                          std::move(registration),
                                          std::move(finished_closure));
      return;
    case blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI:
    case blink::mojom::BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER:
      DCHECK_EQ(registration->registration_data->result,
                blink::mojom::BackgroundFetchResult::FAILURE);
      DispatchBackgroundFetchAbortEvent(registration_id,
                                        std::move(registration),
                                        std::move(finished_closure));
      return;
    case blink::mojom::BackgroundFetchFailureReason::BAD_STATUS:
    case blink::mojom::BackgroundFetchFailureReason::FETCH_ERROR:
    case blink::mojom::BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE:
    case blink::mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED:
    case blink::mojom::BackgroundFetchFailureReason::DOWNLOAD_TOTAL_EXCEEDED:
      DCHECK_EQ(registration->registration_data->result,
                blink::mojom::BackgroundFetchResult::FAILURE);
      DispatchBackgroundFetchFailEvent(registration_id, std::move(registration),
                                       std::move(finished_closure));
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void BackgroundFetchEventDispatcher::DispatchBackgroundFetchAbortEvent(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    base::OnceClosure finished_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LogBackgroundFetchCompletionForDevTools(
      registration_id, ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_ABORT,
      registration->registration_data->failure_reason);

  LoadServiceWorkerRegistrationForDispatch(
      registration_id, ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_ABORT,
      std::move(finished_closure),
      base::BindOnce(
          &BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchAbortEvent,
          std::move(registration)));
}

void BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchAbortEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    int request_id) {
  DCHECK(service_worker_version);
  DCHECK(registration);
  service_worker_version->endpoint()->DispatchBackgroundFetchAbortEvent(
      std::move(registration),
      service_worker_version->CreateSimpleEventCallback(request_id));
}

void BackgroundFetchEventDispatcher::DispatchBackgroundFetchClickEvent(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchRegistrationDataPtr registration_data,
    base::OnceClosure finished_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(registration_data);

  auto registration = blink::mojom::BackgroundFetchRegistration::New(
      std::move(registration_data),
      BackgroundFetchRegistrationServiceImpl::CreateInterfaceInfo(
          registration_id, background_fetch_context_->GetWeakPtr()));

  LoadServiceWorkerRegistrationForDispatch(
      registration_id, ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_CLICK,
      std::move(finished_closure),
      base::BindOnce(
          &BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchClickEvent,
          std::move(registration)));
}

void BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchClickEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    int request_id) {
  DCHECK(service_worker_version);
  DCHECK(registration);
  service_worker_version->endpoint()->DispatchBackgroundFetchClickEvent(
      std::move(registration),
      service_worker_version->CreateSimpleEventCallback(request_id));
}

void BackgroundFetchEventDispatcher::DispatchBackgroundFetchFailEvent(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    base::OnceClosure finished_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LogBackgroundFetchCompletionForDevTools(
      registration_id, ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_FAIL,
      registration->registration_data->failure_reason);

  LoadServiceWorkerRegistrationForDispatch(
      registration_id, ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_FAIL,
      std::move(finished_closure),
      base::BindOnce(
          &BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchFailEvent,
          std::move(registration)));
}

void BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchFailEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    int request_id) {
  DCHECK(service_worker_version);
  DCHECK(registration);
  service_worker_version->endpoint()->DispatchBackgroundFetchFailEvent(
      std::move(registration),
      service_worker_version->CreateSimpleEventCallback(request_id));
}

void BackgroundFetchEventDispatcher::DispatchBackgroundFetchSuccessEvent(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    base::OnceClosure finished_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LogBackgroundFetchCompletionForDevTools(
      registration_id,
      ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_SUCCESS,
      registration->registration_data->failure_reason);

  LoadServiceWorkerRegistrationForDispatch(
      registration_id,
      ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_SUCCESS,
      std::move(finished_closure),
      base::BindOnce(&BackgroundFetchEventDispatcher::
                         DoDispatchBackgroundFetchSuccessEvent,
                     std::move(registration)));
}

void BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchSuccessEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    int request_id) {
  DCHECK(service_worker_version);
  DCHECK(registration);
  service_worker_version->endpoint()->DispatchBackgroundFetchSuccessEvent(
      std::move(registration),
      service_worker_version->CreateSimpleEventCallback(request_id));
}

void BackgroundFetchEventDispatcher::LoadServiceWorkerRegistrationForDispatch(
    const BackgroundFetchRegistrationId& registration_id,
    ServiceWorkerMetrics::EventType event,
    base::OnceClosure finished_closure,
    ServiceWorkerLoadedCallback loaded_callback) {
  service_worker_context_->FindReadyRegistrationForId(
      registration_id.service_worker_registration_id(),
      registration_id.storage_key(),
      base::BindOnce(
          &BackgroundFetchEventDispatcher::StartActiveWorkerForDispatch, event,
          std::move(finished_closure), std::move(loaded_callback)));
}

void BackgroundFetchEventDispatcher::StartActiveWorkerForDispatch(
    ServiceWorkerMetrics::EventType event,
    base::OnceClosure finished_closure,
    ServiceWorkerLoadedCallback loaded_callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    DidDispatchEvent(event, std::move(finished_closure), DispatchPhase::FINDING,
                     service_worker_status);
    return;
  }

  ServiceWorkerVersion* service_worker_version = registration->active_version();
  DCHECK(service_worker_version);

  service_worker_version->RunAfterStartWorker(
      event,
      base::BindOnce(&BackgroundFetchEventDispatcher::DispatchEvent, event,
                     std::move(finished_closure), std::move(loaded_callback),
                     base::WrapRefCounted(service_worker_version)));
}

void BackgroundFetchEventDispatcher::DispatchEvent(
    ServiceWorkerMetrics::EventType event,
    base::OnceClosure finished_closure,
    ServiceWorkerLoadedCallback loaded_callback,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    DidDispatchEvent(event, std::move(finished_closure),
                     DispatchPhase::STARTING, start_worker_status);
    return;
  }

  int request_id = service_worker_version->StartRequest(
      event,
      base::BindOnce(&BackgroundFetchEventDispatcher::DidDispatchEvent, event,
                     std::move(finished_closure), DispatchPhase::DISPATCHING));

  std::move(loaded_callback).Run(std::move(service_worker_version), request_id);
}

void BackgroundFetchEventDispatcher::DidDispatchEvent(
    ServiceWorkerMetrics::EventType event,
    base::OnceClosure finished_closure,
    DispatchPhase dispatch_phase,
    blink::ServiceWorkerStatusCode service_worker_status) {
  std::move(finished_closure).Run();
}

void BackgroundFetchEventDispatcher::LogBackgroundFetchCompletionForDevTools(
    const BackgroundFetchRegistrationId& registration_id,
    ServiceWorkerMetrics::EventType event_type,
    blink::mojom::BackgroundFetchFailureReason failure_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(devtools_context_);

  if (!devtools_context_->IsRecording(
          DevToolsBackgroundService::kBackgroundFetch)) {
    return;
  }

  std::map<std::string, std::string> metadata = {
      {"Event Type", EventTypeToString(event_type)}};
  if (failure_reason != blink::mojom::BackgroundFetchFailureReason::NONE) {
    std::stringstream stream;
    stream << failure_reason;
    metadata["Failure Reason"] = stream.str();
  }

  devtools_context_->LogBackgroundServiceEvent(
      registration_id.service_worker_registration_id(),
      registration_id.storage_key(),
      DevToolsBackgroundService::kBackgroundFetch,
      /* event_name= */ "Background Fetch completed",
      /* instance_id= */ registration_id.developer_id(), metadata);
}

void BackgroundFetchEventDispatcher::Shutdown() {
  devtools_context_ = nullptr;
}

}  // namespace content
