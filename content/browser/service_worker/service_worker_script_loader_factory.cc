// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_loader_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_installed_script_loader.h"
#include "content/browser/service_worker/service_worker_new_script_loader.h"
#include "content/browser/service_worker/service_worker_updated_script_loader.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

ServiceWorkerScriptLoaderFactory::ServiceWorkerScriptLoaderFactory(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerHost> worker_host,
    scoped_refptr<network::SharedURLLoaderFactory>
        loader_factory_for_new_scripts)
    : context_(context),
      worker_host_(worker_host),
      loader_factory_for_new_scripts_(
          std::move(loader_factory_for_new_scripts)) {
  DCHECK(loader_factory_for_new_scripts_ ||
         ServiceWorkerVersion::IsInstalled(worker_host_->version()->status()));
}

ServiceWorkerScriptLoaderFactory::~ServiceWorkerScriptLoaderFactory() = default;

void ServiceWorkerScriptLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!CheckIfScriptRequestIsValid(resource_request)) {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }

  // There are four cases of how to handle the request for the script.
  // A) service worker is installed, script is installed: serve from storage
  //    (use ServceWorkerInstalledScriptLoader). Typically this case is handled
  //    by ServiceWorkerInstalledScriptsSender, but we can still get here when a
  //    new service worker starts up and becomes installed while it is running.
  // B) service worker is installed, script is not installed: return a network
  //    error. This happens when the script is newly imported after
  //    installation, which is disallowed by the spec.
  // C) service worker is not installed, script is installed: serve from
  //    storage (use ServiceWorkerInstalledScriptLoader)
  // D) service worker is not installed, script is not installed:
  //    1) If compared script info exists and specifies that the script is
  //       installed in an old service worker and content is not changed, then
  //       copy the old script into the new service worker and load it using
  //       ServiceWorkerInstalledScriptLoader.
  //    2) If compared script info exists and specifies that the script is
  //       installed in an old service worker but content has changed, then
  //       ServiceWorkerUpdatedScriptLoader::CreateAndStart() is called to
  //       resume the paused state stored in the compared script info.
  //    3) For other cases, serve from network with installing the script by
  //       using ServiceWorkerNewScriptLoader.

  // Case A and C:
  scoped_refptr<ServiceWorkerVersion> version = worker_host_->version();
  int64_t resource_id =
      version->script_cache_map()->LookupResourceId(resource_request.url);
  if (resource_id != blink::mojom::kInvalidServiceWorkerResourceId) {
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> resource_reader;
    context_->registry()->GetRemoteStorageControl()->CreateResourceReader(
        resource_id, resource_reader.BindNewPipeAndPassReceiver());
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<ServiceWorkerInstalledScriptLoader>(
            options, std::move(client), std::move(resource_reader), version,
            resource_request.url),
        std::move(receiver));
    return;
  }

  // Case B:
  if (ServiceWorkerVersion::IsInstalled(version->status())) {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  // Case D:
  // Compared script info is used to decide which sub case should be used.
  // If there is no compared script info, goto D.3 directly.
  const auto& compared_script_info_map = version->compared_script_info_map();
  if (!compared_script_info_map.empty()) {
    auto it = compared_script_info_map.find(resource_request.url);
    if (it != compared_script_info_map.end()) {
      switch (it->second.result) {
        case ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical:
          // Case D.1:
          context_->GetStorageControl()->GetNewResourceId(base::BindOnce(
              &ServiceWorkerScriptLoaderFactory::CopyScript,
              weak_factory_.GetWeakPtr(), it->first, it->second.old_resource_id,
              base::BindOnce(
                  &ServiceWorkerScriptLoaderFactory::OnCopyScriptFinished,
                  weak_factory_.GetWeakPtr(), std::move(receiver), options,
                  resource_request, std::move(client))));
          return;
        case ServiceWorkerSingleScriptUpdateChecker::Result::kFailed:
          // Network failure is treated as D.2
        case ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent:
          // Case D.2:
          mojo::MakeSelfOwnedReceiver(
              ServiceWorkerUpdatedScriptLoader::CreateAndStart(
                  options, resource_request, std::move(client), version),
              std::move(receiver));
          return;
        case ServiceWorkerSingleScriptUpdateChecker::Result::kNotCompared:
          // This is invalid, as scripts in compared script info must have been
          // compared.
          NOTREACHED_IN_MIGRATION();
          return;
      }
    }
  }

  // Case D.3:
  // Assign a new resource ID for the script from network.
  context_->GetStorageControl()->GetNewResourceId(base::BindOnce(
      &ServiceWorkerScriptLoaderFactory::OnResourceIdAssignedForNewScriptLoader,
      weak_factory_.GetWeakPtr(), std::move(receiver), request_id, options,
      resource_request, std::move(client), traffic_annotation));
}

void ServiceWorkerScriptLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ServiceWorkerScriptLoaderFactory::Update(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
  loader_factory_for_new_scripts_ = std::move(loader_factory);
}

bool ServiceWorkerScriptLoaderFactory::CheckIfScriptRequestIsValid(
    const network::ResourceRequest& resource_request) {
  if (!context_ || !worker_host_)
    return false;

  scoped_refptr<ServiceWorkerVersion> version = worker_host_->version();
  if (!version)
    return false;

  // Handle only RequestDestination::kServiceWorker (the service worker main
  // script or static-import) or RequestDestination::kScript (importScripts()).
  if (resource_request.destination !=
          network::mojom::RequestDestination::kServiceWorker &&
      resource_request.destination !=
          network::mojom::RequestDestination::kScript) {
    SCOPED_CRASH_KEY_STRING32(
        "ServiceWorkerSLF", "bad_type",
        network::RequestDestinationToString(resource_request.destination));
    mojo::ReportBadMessage("SWSLF_BAD_REQUEST_DESTINATION");
    return false;
  }

  if (version->status() == ServiceWorkerVersion::REDUNDANT) {
    // This could happen if browser-side has set the status to redundant but
    // the worker has not yet stopped. The worker is already doomed so just
    // reject the request. Handle it specially here because otherwise it'd
    // be unclear whether "REDUNDANT" should count as installed or not
    // installed when making decisions about how to handle the request and
    // logging UMA.
    return false;
  }

  // TODO(falken): Make sure we don't handle a redirected request.

  return true;
}

void ServiceWorkerScriptLoaderFactory::CopyScript(
    const GURL& url,
    int64_t resource_id,
    base::OnceCallback<void(int64_t, net::Error)> callback,
    int64_t new_resource_id) {
  if (!context_ || !worker_host_) {
    std::move(callback).Run(new_resource_id, net::ERR_FAILED);
    return;
  }
  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader;
  context_->registry()->GetRemoteStorageControl()->CreateResourceReader(
      resource_id, reader.BindNewPipeAndPassReceiver());
  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer;
  context_->registry()->GetRemoteStorageControl()->CreateResourceWriter(
      new_resource_id, writer.BindNewPipeAndPassReceiver());

  cache_writer_ = ServiceWorkerCacheWriter::CreateForCopy(
      std::move(reader), std::move(writer), new_resource_id);

  scoped_refptr<ServiceWorkerVersion> version = worker_host_->version();
  version->script_cache_map()->NotifyStartedCaching(url, new_resource_id);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  net::Error error = cache_writer_->StartCopy(
      base::BindOnce(std::move(split_callback.first), new_resource_id));

  // Run the callback directly if the operation completed or failed
  // synchronously.
  if (net::ERR_IO_PENDING != error) {
    std::move(split_callback.second).Run(new_resource_id, error);
  }
}

void ServiceWorkerScriptLoaderFactory::OnCopyScriptFinished(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    int64_t new_resource_id,
    net::Error error) {
  if (!worker_host_) {
    // Null |worker_host_| means the worker has been terminated unexpectedly.
    // Nothing can do in this case.
    return;
  }

  int64_t resource_size = cache_writer_->bytes_written();
  DCHECK_EQ(cache_writer_->checksum_update_timing(),
            ServiceWorkerCacheWriter::ChecksumUpdateTiming::kCacheMismatch);
  std::string sha256_checksum = cache_writer_->GetSha256Checksum();
  cache_writer_.reset();
  scoped_refptr<ServiceWorkerVersion> version = worker_host_->version();

  if (error != net::OK) {
    version->script_cache_map()->NotifyFinishedCaching(
        resource_request.url, resource_size, sha256_checksum, error,
        ServiceWorkerConsts::kServiceWorkerCopyScriptError);

    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(error));
    return;
  }

  // The copy operation is successful, add the newly copied resource record to
  // the script cache map to identify that the script is installed.
  version->script_cache_map()->NotifyFinishedCaching(
      resource_request.url, resource_size, sha256_checksum, net::OK,
      std::string());

  // Use ServiceWorkerInstalledScriptLoader to load the new copy.
  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> resource_reader;
  context_->registry()->GetRemoteStorageControl()->CreateResourceReader(
      new_resource_id, resource_reader.BindNewPipeAndPassReceiver());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ServiceWorkerInstalledScriptLoader>(
          options, std::move(client), std::move(resource_reader), version,
          resource_request.url),
      std::move(receiver));
}

void ServiceWorkerScriptLoaderFactory::OnResourceIdAssignedForNewScriptLoader(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    int64_t resource_id) {
  if (!worker_host_) {
    // Null |worker_host_| means the worker has been terminated unexpectedly.
    // Nothing can do in this case.
    return;
  }

  if (resource_id == blink::mojom::kInvalidServiceWorkerResourceId) {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }

  // Note: We do not need to run throttles because they have been already by
  // the ResourceFetcher on the renderer-side.
  mojo::MakeSelfOwnedReceiver(
      ServiceWorkerNewScriptLoader::CreateAndStart(
          request_id, options, resource_request, std::move(client),
          worker_host_->version(), loader_factory_for_new_scripts_,
          traffic_annotation, resource_id, /*is_throttle_needed=*/false,
          /*requesting_frame_id=*/GlobalRenderFrameHostId()),
      std::move(receiver));
}

}  // namespace content
