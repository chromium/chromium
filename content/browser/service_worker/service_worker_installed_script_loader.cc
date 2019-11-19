// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_installed_script_loader.h"

#include <memory>
#include <string>
#include <utility>
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "net/base/ip_endpoint.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace content {

using FinishedReason = ServiceWorkerInstalledScriptReader::FinishedReason;

ServiceWorkerInstalledScriptLoader::ServiceWorkerInstalledScriptLoader(
    uint32_t options,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    std::unique_ptr<ServiceWorkerResponseReader> response_reader,
    scoped_refptr<ServiceWorkerVersion>
        version_for_main_script_http_response_info,
    const GURL& request_url)
    : options_(options),
      client_(std::move(client)),
      request_start_(base::TimeTicks::Now()) {
  // Normally, the main script info is set by ServiceWorkerNewScriptLoader for
  // new service workers and ServiceWorkerInstalledScriptsSender for installed
  // service workes. But some embedders might preinstall scripts to the
  // ServiceWorkerScriptCacheMap while not setting the ServiceWorkerVersion
  // status to INSTALLED, so we can come to here instead of using
  // SeviceWorkerInstalledScriptsSender.
  // In this case, the main script info would not yet have been set, so set it
  // here.
  if (request_url == version_for_main_script_http_response_info->script_url() &&
      !version_for_main_script_http_response_info
           ->GetMainScriptHttpResponseInfo()) {
    version_for_main_script_http_response_info_ =
        std::move(version_for_main_script_http_response_info);
  }
  reader_ = std::make_unique<ServiceWorkerInstalledScriptReader>(
      std::move(response_reader), this);
  reader_->Start();
  // We continue in OnStarted().
}

ServiceWorkerInstalledScriptLoader::~ServiceWorkerInstalledScriptLoader() =
    default;

void ServiceWorkerInstalledScriptLoader::OnStarted(
    std::string encoding,
    base::flat_map<std::string, std::string> headers,
    mojo::ScopedDataPipeConsumerHandle body_handle,
    uint64_t body_size,
    mojo::ScopedDataPipeConsumerHandle metadata_handle,
    uint64_t metadata_size) {
  encoding_ = encoding;
  body_handle_ = std::move(body_handle);
  body_size_ = body_size;

  // Just drain the metadata (V8 code cache): this entire class is just to
  // handle a corner case for non-installed service workers and high performance
  // is not needed.
  metadata_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(metadata_handle));

  // We continue in OnHttpInfoRead().
}

void ServiceWorkerInstalledScriptLoader::OnHttpInfoRead(
    scoped_refptr<HttpResponseInfoIOBuffer> http_info) {
  net::HttpResponseInfo* info = http_info->http_info.get();
  DCHECK(info);

  if (version_for_main_script_http_response_info_) {
    version_for_main_script_http_response_info_->SetMainScriptHttpResponseInfo(
        *info);
  }

  auto response = ServiceWorkerUtils::CreateResourceResponseHeadAndMetadata(
      info, options_, request_start_, base::TimeTicks::Now(),
      http_info->response_data_size);
  client_->OnReceiveResponse(std::move(response.head));
  if (!response.metadata.empty())
    client_->OnReceiveCachedMetadata(std::move(response.metadata));
  client_->OnStartLoadingResponseBody(std::move(body_handle_));
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
    case FinishedReason::kNoHttpInfoError:
    case FinishedReason::kResponseReaderError:
      net_error = net::ERR_FILE_NOT_FOUND;
      break;
    case FinishedReason::kConnectionError:
    case FinishedReason::kMetaDataSenderError:
      net_error = net::ERR_FAILED;
      break;
    case FinishedReason::kNotFinished:
      NOTREACHED();
      break;
  }
  client_->OnComplete(network::URLLoaderCompletionStatus(net_error));
}

void ServiceWorkerInstalledScriptLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  // This class never returns a redirect response to its client, so should never
  // be asked to follow one.
  NOTREACHED();
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
