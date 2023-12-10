// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_

#include "base/functional/callback.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_worker_client_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/url_request/redirect_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

namespace net {
class IsolationInfo;
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
class RenderFrameHostImpl;
class ServiceWorkerContextWrapper;
class ServiceWorkerMainResourceHandle;
class ServiceWorkerObjectHost;
class StoragePartitionImpl;
class WorkerScriptLoaderFactory;

struct SubresourceLoaderParams;

// NetworkService (PlzWorker):
// This is an implementation of the URLLoaderClient for web worker's main script
// fetch. The loader and client bounded with this class are to be unbound and
// forwarded to the renderer process on OnReceiveResponse, and the
// resource loader in the renderer process will take them over.
//
// WorkerScriptFetcher deletes itself when the ownership of the loader and
// client is passed to the renderer, or on failure. It lives on the UI
// thread.
// TODO(asamidoi): Remove the manual memory management like `delete this` and
// use `unique_ptr` to create WorkerScriptFetcher in a caller side.
class WorkerScriptFetcher : public network::mojom::URLLoaderClient {
 public:
  // Called with the result of fetching a script upon completion.
  //
  // - `subresource_loader_factories` is never nullptr.
  // - `main_script_load_params` is nullptr iff the fetch failed. Otherwise, it
  //    always contains `response_head` and `response_head->parsed_headers`.
  // - `controller` and `controller_service_worker_object_host` may be nullptr.
  // - `final_response_url` specifies the script response URL.
  using CompletionCallback = base::OnceCallback<void(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host,
      const GURL& final_response_url)>;

  // Used for specifying how URLLoaderFactoryBundle is used.
  enum class LoaderType { kMainResource, kSubResource };

  // Initiates a browser-side worker script fetch.
  // Creates a `WorkerScriptFetcher` and starts it.
  //
  // Must be called on the UI thread.
  //
  // - `ancestor_render_frame_host` points to the ancestor frame, if any. If
  //   the worker being created is nested, then this is the ancestor of the
  //   creator worker. Otherwise, this is the creator frame. May be nullptr.
  //   For dedicated workers, `ancestor_render_frame_host` *should* always exist
  //   though due to the fact that `DedicatedWorkerHost` lifetimes do not align
  //   exactly with their parents (they are destroyed asynchronously via mojo),
  //   the ancestor frame might have been destroyed when the fetch starts.
  //   TODO(https://crbug.com/1177652): Amend the above comment once
  //   `DedicatedWorkerHost` lifetimes align with their creators'.
  // - `creator_render_frame_host` points to the creator frame, if any. May
  //   be nullptr if the worker being created is a nested dedicated worker.
  //   Since nested shared workers are not supported, for shared workers
  //   `ancestor_render_frame_host` and `creator_render_frame_host` are always
  //   equal.
  // - `client_security_state` specifies parameters to be passed to the network
  //   service `URLLoaderFactory`, for use when loading the script. It must not
  //   be nullptr.
  // - `callback` will be called with the result on the UI thread.
  static void CreateAndStart(
      int worker_process_id,
      const DedicatedOrSharedWorkerToken& worker_token,
      const GURL& initial_request_url,
      RenderFrameHostImpl* ancestor_render_frame_host,
      RenderFrameHostImpl* creator_render_frame_host,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& request_initiator,
      const blink::StorageKey& request_initiator_storage_key,
      const net::IsolationInfo& trusted_isolation_info,
      network::mojom::ClientSecurityStatePtr client_security_state,
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
                      RenderFrameHostImpl* creator_render_frame_host,
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
  // Callback invoked by this instance when the load ends, successfully or not.
  //
  // In case of success:
  //
  // - `main_script_load_params` is not nullptr.
  // - `subresource_loader_params` may be nullopt.
  // - `completion_status` is nullptr.
  //
  // In case of error:
  //
  // - `main_script_load_params` is nullptr.
  // - `subresource_loader_params` is nullopt.
  // - `completion_status` is not nullptr.
  using CreateAndStartCallback = base::OnceCallback<void(
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      absl::optional<SubresourceLoaderParams> subresource_loader_params,
      const network::URLLoaderCompletionStatus* completion_status)>;

  WorkerScriptFetcher(
      std::unique_ptr<WorkerScriptLoaderFactory> script_loader_factory,
      std::unique_ptr<network::ResourceRequest> resource_request,
      CreateAndStartCallback callback);

  ~WorkerScriptFetcher() override;

  // Helper for `CreateAndStart()`.
  static void CreateScriptLoader(
      int worker_process_id,
      const DedicatedOrSharedWorkerToken& worker_token,
      const GURL& initial_request_url,
      RenderFrameHostImpl* ancestor_render_frame_host,
      RenderFrameHostImpl* creator_render_frame_host,
      const net::IsolationInfo& trusted_isolation_info,
      network::mojom::ClientSecurityStatePtr client_security_state,
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
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void DidParseHeaders(network::mojom::ParsedHeadersPtr parsed_headers);

  std::unique_ptr<WorkerScriptLoaderFactory> script_loader_factory_;

  // Request ID for a browser-initiated request.
  const int request_id_;

  std::unique_ptr<network::ResourceRequest> resource_request_;
  CreateAndStartCallback callback_;

  // URLLoader instance backed by a request interceptor or the network service.
  std::unique_ptr<blink::ThrottlingURLLoader> url_loader_;

  blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params_;
  absl::optional<SubresourceLoaderParams> subresource_loader_params_;

  std::vector<net::RedirectInfo> redirect_infos_;
  std::vector<network::mojom::URLResponseHeadPtr> redirect_response_heads_;

  base::WeakPtrFactory<WorkerScriptFetcher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_
