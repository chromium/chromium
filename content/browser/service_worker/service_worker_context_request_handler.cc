// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_request_handler.h"

#include "base/command_line.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_read_from_cache_job.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/service_worker/service_worker_write_to_cache_job.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_switches.h"
#include "net/base/load_flags.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_error_job.h"
#include "services/network/public/cpp/resource_response_info.h"

namespace content {

ServiceWorkerContextRequestHandler::ServiceWorkerContextRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    ResourceType resource_type)
    : ServiceWorkerRequestHandler(context,
                                  provider_host,
                                  blob_storage_context,
                                  resource_type),
      version_(provider_host_->running_hosted_version()) {
  DCHECK(provider_host_->IsProviderForServiceWorker());
  DCHECK(version_);
}

ServiceWorkerContextRequestHandler::~ServiceWorkerContextRequestHandler() {
}

// static
std::string ServiceWorkerContextRequestHandler::CreateJobStatusToString(
    CreateJobStatus status) {
  switch (status) {
    case CreateJobStatus::UNINITIALIZED:
      return "UNINITIALIZED";
    case CreateJobStatus::WRITE_JOB:
      return "WRITE_JOB";
    case CreateJobStatus::WRITE_JOB_WITH_INCUMBENT:
      return "WRITE_JOB_WITH_INCUMBENT";
    case CreateJobStatus::READ_JOB:
      return "READ_JOB";
    case CreateJobStatus::READ_JOB_FOR_DUPLICATE_SCRIPT_IMPORT:
      return "READ_JOB_FOR_DUPLICATE_SCRIPT_IMPORT";
    case CreateJobStatus::ERROR_NO_PROVIDER:
      return "ERROR_NO_PROVIDER";
    case CreateJobStatus::ERROR_REDUNDANT_VERSION:
      return "ERROR_REDUNDANT_VERSION";
    case CreateJobStatus::ERROR_NO_CONTEXT:
      return "ERROR_NO_CONTEXT";
    case CreateJobStatus::ERROR_REDIRECT:
      return "ERROR_REDIRECT";
    case CreateJobStatus::ERROR_UNINSTALLED_SCRIPT_IMPORT:
      return "ERROR_UNINSTALLED_SCRIPT_IMPORT";
    case CreateJobStatus::ERROR_OUT_OF_RESOURCE_IDS:
      return "ERROR_OUT_OF_RESOURCE_IDS";
  }
  NOTREACHED() << static_cast<int>(status);
  return "UNKNOWN";
}

net::URLRequestJob* ServiceWorkerContextRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    ResourceContext* resource_context) {
  // We only use the script cache for main script loading and
  // importScripts(), even if a cached script is xhr'd, we don't
  // retrieve it from the script cache.
  // TODO(falken): Get the desired behavior clarified in the spec,
  // and make tweak the behavior here to match.
  if (resource_type_ != RESOURCE_TYPE_SERVICE_WORKER &&
      resource_type_ != RESOURCE_TYPE_SCRIPT) {
    // Fall back to network.
    return nullptr;
  }

  CreateJobStatus status = CreateJobStatus::UNINITIALIZED;
  net::URLRequestJob* job =
      MaybeCreateJobImpl(request, network_delegate, &status);
  const bool is_main_script = resource_type_ == RESOURCE_TYPE_SERVICE_WORKER;
  ServiceWorkerMetrics::RecordContextRequestHandlerStatus(
      status, ServiceWorkerVersion::IsInstalled(version_->status()),
      is_main_script);
  if (job)
    return job;

  // If we got here, a job couldn't be created. Return an error job rather than
  // falling back to network. Otherwise the renderer may receive the response
  // from network and start a service worker whose browser-side
  // ServiceWorkerVersion is not properly initialized.
  std::string error_str(CreateJobStatusToString(status));
  request->net_log().AddEvent(
      net::NetLogEventType::SERVICE_WORKER_SCRIPT_LOAD_UNHANDLED_REQUEST_ERROR,
      net::NetLog::StringCallback("error", &error_str));

  return new net::URLRequestErrorJob(request, network_delegate,
                                     net::Error::ERR_FAILED);
}

net::URLRequestJob* ServiceWorkerContextRequestHandler::MaybeCreateJobImpl(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    CreateJobStatus* out_status) {
  if (!context_) {
    *out_status = CreateJobStatus::ERROR_NO_CONTEXT;
    return nullptr;
  }
  if (!provider_host_) {
    *out_status = CreateJobStatus::ERROR_NO_PROVIDER;
    return nullptr;
  }

  // This could happen if browser-side has set the status to redundant but the
  // worker has not yet stopped. The worker is already doomed so just reject the
  // request. Handle it specially here because otherwise it'd be unclear whether
  // "REDUNDANT" should count as installed or not installed when making
  // decisions about how to handle the request and logging UMA.
  if (version_->status() == ServiceWorkerVersion::REDUNDANT) {
    *out_status = CreateJobStatus::ERROR_REDUNDANT_VERSION;
    return nullptr;
  }

  // We currently have no use case for hijacking a redirected request.
  if (request->url_chain().size() > 1) {
    *out_status = CreateJobStatus::ERROR_REDIRECT;
    return nullptr;
  }

  const bool is_main_script = resource_type_ == RESOURCE_TYPE_SERVICE_WORKER;
  int64_t resource_id =
      version_->script_cache_map()->LookupResourceId(request->url());
  if (resource_id != kInvalidServiceWorkerResourceId) {
    if (ServiceWorkerVersion::IsInstalled(version_->status())) {
      // An installed worker is loading a stored script.
      *out_status = CreateJobStatus::READ_JOB;
    } else {
      // A new worker is loading a stored script. The script was already
      // imported (or the main script is being recursively imported).
      *out_status = CreateJobStatus::READ_JOB_FOR_DUPLICATE_SCRIPT_IMPORT;
    }
    return new ServiceWorkerReadFromCacheJob(request, network_delegate,
                                             resource_type_, context_, version_,
                                             resource_id);
  }

  // An installed worker is importing a non-stored script.
  if (ServiceWorkerVersion::IsInstalled(version_->status())) {
    DCHECK(!is_main_script);
    *out_status = CreateJobStatus::ERROR_UNINSTALLED_SCRIPT_IMPORT;
    return nullptr;
  }

  // A new worker is loading a script for the first time. Create a write job to
  // store the script.
  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(version_->registration_id());
  DCHECK(registration);  // We're registering or updating so must be there.

  resource_id = context_->storage()->NewResourceId();
  if (resource_id == kInvalidServiceWorkerResourceId) {
    *out_status = CreateJobStatus::ERROR_OUT_OF_RESOURCE_IDS;
    return nullptr;
  }

  // Bypass the browser cache for initial installs and update checks after 24
  // hours have passed.
  int extra_load_flags = 0;
  base::TimeDelta time_since_last_check =
      base::Time::Now() - registration->last_update_check();

  if (ServiceWorkerUtils::ShouldBypassCacheDueToUpdateViaCache(
          is_main_script, registration->update_via_cache()) ||
      time_since_last_check > kServiceWorkerScriptMaxCacheAge ||
      version_->force_bypass_cache_for_scripts()) {
    extra_load_flags = net::LOAD_BYPASS_CACHE;
  }

  ServiceWorkerVersion* stored_version = registration->waiting_version()
                                             ? registration->waiting_version()
                                             : registration->active_version();
  int64_t incumbent_resource_id = kInvalidServiceWorkerResourceId;
  if (is_main_script) {
    if (stored_version && stored_version->script_url() == request->url()) {
      incumbent_resource_id =
          stored_version->script_cache_map()->LookupResourceId(request->url());
    }
  }
  *out_status = incumbent_resource_id == kInvalidServiceWorkerResourceId
                    ? CreateJobStatus::WRITE_JOB
                    : CreateJobStatus::WRITE_JOB_WITH_INCUMBENT;
  return new ServiceWorkerWriteToCacheJob(
      request, network_delegate, resource_type_, context_, version_.get(),
      extra_load_flags, resource_id, incumbent_resource_id);
}

}  // namespace content
