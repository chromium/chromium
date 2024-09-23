// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPT_LOADER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPT_LOADER_H_

#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_installed_script_reader.h"
#include "content/browser/service_worker/url_loader_client_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

class ServiceWorkerVersion;

// A URLLoader that loads an installed service worker script for a service
// worker that doesn't have a ServiceWorkerInstalledScriptsManager.
//
// Some cases where this happens:
// - a new (non-installed) service worker requests a script that it already
//   installed, e.g., importScripts('a.js') multiple times.
// - a service worker that was new when it started and became installed while
//   running requests an installed script, e.g., importScripts('a.js') after
//   installation.
class ServiceWorkerInstalledScriptLoader
    : public network::mojom::URLLoader,
      public ServiceWorkerInstalledScriptReader::Client,
      public mojo::DataPipeDrainer::Client {
 public:
  ServiceWorkerInstalledScriptLoader(
      uint32_t options,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      mojo::Remote<storage::mojom::ServiceWorkerResourceReader> resource_reader,
      scoped_refptr<ServiceWorkerVersion>
          version_for_main_script_http_response_info,
      const GURL& request_url);

  ServiceWorkerInstalledScriptLoader(
      const ServiceWorkerInstalledScriptLoader&) = delete;
  ServiceWorkerInstalledScriptLoader& operator=(
      const ServiceWorkerInstalledScriptLoader&) = delete;

  ~ServiceWorkerInstalledScriptLoader() override;

  // ServiceWorkerInstalledScriptReader::Client overrides:
  void OnStarted(network::mojom::URLResponseHeadPtr response_head,
                 std::optional<mojo_base::BigBuffer> metadata,
                 mojo::ScopedDataPipeConsumerHandle body_handle,
                 mojo::ScopedDataPipeConsumerHandle meta_data_handle) override;
  void OnFinished(
      ServiceWorkerInstalledScriptReader::FinishedReason reason) override;

  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

 private:
  // mojo::DataPipeDrainer::Client overrides:
  // These just do nothing.
  void OnDataAvailable(base::span<const uint8_t> data) override {}
  void OnDataComplete() override {}

  URLLoaderClientCheckedRemote client_;
  scoped_refptr<ServiceWorkerVersion>
      version_for_main_script_http_response_info_;
  base::TimeTicks request_start_time_;
  std::unique_ptr<ServiceWorkerInstalledScriptReader> reader_;

  std::string encoding_;
  uint64_t body_size_ = 0;
  std::unique_ptr<mojo::DataPipeDrainer> metadata_drainer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPT_LOADER_H_
