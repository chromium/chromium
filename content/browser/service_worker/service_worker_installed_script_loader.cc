// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_installed_script_loader.h"

#include <memory>
#include <string>
#include <utility>
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "net/base/ip_endpoint.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace content {

using FinishedReason = ServiceWorkerInstalledScriptReader::FinishedReason;

ServiceWorkerInstalledScriptLoader::ServiceWorkerInstalledScriptLoader(
    uint32_t options,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> resource_reader,
    scoped_refptr<ServiceWorkerVersion>
        version_for_main_script_http_response_info,
    const GURL& request_url)
    : client_(std::move(client)), request_start_time_(base::TimeTicks::Now()) {
  // Normally, the main script info is set by ServiceWorkerNewScriptLoader for
  // new service workers and ServiceWorkerInstalledScriptsSender for installed
  // service workes. But some embedders might preinstall scripts to the
  // ServiceWorkerScriptCacheMap while not setting the ServiceWorkerVersion
  // status to INSTALLED, so we can come to here instead of using
  // SeviceWorkerInstalledScriptsSender.
  // In this case, the main script info would not yet have been set, so set it
  // here.
  if (request_url == version_for_main_script_http_response_info->script_url() &&
      !version_for_main_script_http_response_info->GetMainScriptResponse()) {
    version_for_main_script_http_response_info_ =
        std::move(version_for_main_script_http_response_info);
  }
  reader_ = std::make_unique<ServiceWorkerInstalledScriptReader>(
      std::move(resource_reader), this);
  reader_->Start();
  // We continue in OnStarted().
}

ServiceWorkerInstalledScriptLoader::~ServiceWorkerInstalledScriptLoader() =
    default;

void ServiceWorkerInstalledScriptLoader::OnStarted(
    network::mojom::URLResponseHeadPtr response_head,
    std::optional<mojo_base::BigBuffer> metadata,
    mojo::ScopedDataPipeConsumerHandle body_handle,
    mojo::ScopedDataPipeConsumerHandle metadata_handle) {
  DCHECK(response_head);
  DCHECK(response_head->headers);
  DCHECK(encoding_.empty());
  response_head->headers->GetCharset(&encoding_);
  body_size_ = response_head->content_length;

  // Just drain the metadata (V8 code cache): this entire class is just to
  // handle a corner case for non-installed service workers and high performance
  // is not needed.
  metadata_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(metadata_handle));

  if (version_for_main_script_http_response_info_) {
    version_for_main_script_http_response_info_->SetMainScriptResponse(
        std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
            *response_head));
  }

  client_->OnReceiveResponse(std::move(response_head), std::move(body_handle),
                             std::move(metadata));
  // We continue in OnFinished().
}

void ServiceWorkerInstalledScriptLoader::OnFinished(FinishedReason reason) {
  int net_error = net::ERR_FAILED;
  switch (reason) {
    case FinishedReason::kSuccess:
      net_error = net::OK;
      break;
    case FinishedReason::kCreateDataPipeError:
      net_error = net::ERR_INSUFFICIENT_RESOURCES;
      break;
    case FinishedReason::kNoResponseHeadError:
    case FinishedReason::kResponseReaderError:
      net_error = net::ERR_FILE_NOT_FOUND;
      break;
    case FinishedReason::kConnectionError:
    case FinishedReason::kMetaDataSenderError:
    case FinishedReason::kNoContextError:
      net_error = net::ERR_FAILED;
      break;
    case FinishedReason::kNotFinished:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  client_->OnComplete(network::URLLoaderCompletionStatus(net_error));
}

void ServiceWorkerInstalledScriptLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  // This class never returns a redirect response to its client, so should never
  // be asked to follow one.
  NOTREACHED_IN_MIGRATION();
}

void ServiceWorkerInstalledScriptLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  // Ignore: this class doesn't have a concept of priority.
}

void ServiceWorkerInstalledScriptLoader::PauseReadingBodyFromNet() {
  // Ignore: this class doesn't read from network.
}

void ServiceWorkerInstalledScriptLoader::ResumeReadingBodyFromNet() {
  // Ignore: this class doesn't read from network.
}

}  // namespace content
