// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_

#include "base/callback.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_worker_client_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

namespace net {
class SiteForCookies;
}  // namespace net

namespace network {
struct ResourceRequest;
}  // namespace network

namespace blink {
class PendingURLLoaderFactoryBundle;
class StorageKey;
class ThrottlingURLLoader;
class URLLoaderThrottle;
}  // namespace blink

namespace content {

class DevToolsAgentHostImpl;
class RenderFrameHost;
class ServiceWorkerContextWrapper;
class ServiceWorkerMainResourceHandle;
class ServiceWorkerObjectHost;
class StoragePartitionImpl;
class WorkerScriptLoaderFactory;

struct SubresourceLoaderParams;

// NetworkService (PlzWorker):
// This is an implementation of the URLLoaderClient for web worker's main script
// fetch. The loader and client bounded with this class are to be unbound and
// forwarded to the renderer process on OnStartLoadingResponseBody, and the
// resource loader in the renderer process will take them over.
//
// WorkerScriptFetcher deletes itself when the ownership of the loader and
// client is passed to the renderer, or on failure. It lives on the UI
// thread.
// TODO(asamidoi): Remove the manual memory management like `delete this` and
// use `unique_ptr` to create WorkerScriptFetcher in a caller side.
class WorkerScriptFetcher : public network::mojom::URLLoaderClient {
 public:
  using CreateAndStartCallback =
      base::OnceCallback<void(blink::mojom::WorkerMainScriptLoadParamsPtr,
                              absl::optional<SubresourceLoaderParams>,
                              bool /* success */)>;
  using CompletionCallback = base::OnceCallback<void(
      bool success,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>,
      blink::mojom::WorkerMainScriptLoadParamsPtr,
      blink::mojom::ControllerServiceWorkerInfoPtr,
      base::WeakPtr<ServiceWorkerObjectHost>,
      const GURL& final_response_url)>;

  // Used for specifying how URLLoaderFactoryBundle is used.
  enum class LoaderType { kMainResource, kSubResource };

  // PlzWorker:
  // WorkerScriptFetcher::CreateAndStart() is the entry point of browser-side
  // script fetch. Creates a worker script fetcher and starts it. Must be called
  // on the UI thread. `callback` will be called with the result on the UI
  // thread.
  static void CreateAndStart(
      int worker_process_id,
      const DedicatedOrSharedWorkerToken& worker_token,
      const GURL& initial_request_url,
      RenderFrameHost* ancestor_render_frame_host,
      RenderFrameHost* creator_render_frame_host,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& request_initiator,
      const blink::StorageKey& request_initiator_storage_key,
      const net::IsolationInfo& trusted_isolation_info,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      network::mojom::RequestDestination request_destination,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      ServiceWorkerMainResourceHandle* service_worker_handle,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      scoped_refptr<network::SharedURLLoaderFactory>
          url_loader_factory_override,
      StoragePartitionImpl* storage_partition,
      const std::string& storage_domain,
      ukm::SourceId worker_source_id,
      DevToolsAgentHostImpl* devtools_agent_host,
      const base::UnguessableToken& devtools_worker_token,
      CompletionCallback callback);

  // Creates a loader factory bundle. Must be called on the UI thread. For
  // nested workers, |creator_render_frame_host| can be null.
  static std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
  CreateFactoryBundle(LoaderType loader_type,
                      int worker_process_id,
                      StoragePartitionImpl* storage_partition,
                      const std::string& storage_domain,
                      bool file_support,
                      bool filesystem_url_support,
                      RenderFrameHost* creator_render_frame_host,
                      const blink::StorageKey& request_initiator_storage_key);

  // Calculates the final response URL from the redirect chain, URLs fetched by
  // the service worker and the initial request URL. The logic is mostly based
  // on what blink::ResourceResponse::ResponseUrl() does.
  //
  // Exposed for testing.
  CONTENT_EXPORT static GURL DetermineFinalResponseUrl(
      const GURL& initial_request_url,
      blink::mojom::WorkerMainScriptLoadParams* main_script_load_params);

 private:
  WorkerScriptFetcher(
      std::unique_ptr<WorkerScriptLoaderFactory> script_loader_factory,
      std::unique_ptr<network::ResourceRequest> resource_request,
      CreateAndStartCallback callback);

  ~WorkerScriptFetcher() override;

  static void CreateScriptLoader(
      int worker_process_id,
      const DedicatedOrSharedWorkerToken& worker_token,
      const GURL& initial_request_url,
      RenderFrameHost* creator_render_frame_host,
      const net::IsolationInfo& trusted_isolation_info,
      std::unique_ptr<network::ResourceRequest> resource_request,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          factory_bundle_for_browser_info,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      ServiceWorkerMainResourceHandle* service_worker_handle,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      scoped_refptr<network::SharedURLLoaderFactory>
          url_loader_factory_override,
      ukm::SourceId worker_source_id,
      DevToolsAgentHostImpl* devtools_agent_host,
      const base::UnguessableToken& devtools_worker_token,
      WorkerScriptFetcher::CompletionCallback callback);

  void Start(std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles);

  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void DidParseHeaders(network::mojom::ParsedHeadersPtr parsed_headers);

  std::unique_ptr<WorkerScriptLoaderFactory> script_loader_factory_;

  // Request ID for a browser-initiated request.
  const int request_id_;

  std::unique_ptr<network::ResourceRequest> resource_request_;
  CreateAndStartCallback callback_;

  // URLLoader instance backed by a request interceptor or the network service.
  std::unique_ptr<blink::ThrottlingURLLoader> url_loader_;

  // URLLoader instance for handling a response received from the default
  // network loader. This can be provided by an interceptor.
  mojo::PendingRemote<network::mojom::URLLoader> response_url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> response_url_loader_receiver_{
      this};

  blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params_;
  absl::optional<SubresourceLoaderParams> subresource_loader_params_;

  std::vector<net::RedirectInfo> redirect_infos_;
  std::vector<network::mojom::URLResponseHeadPtr> redirect_response_heads_;
  network::mojom::URLResponseHeadPtr response_head_;

  base::WeakPtrFactory<WorkerScriptFetcher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_
