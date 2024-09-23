// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_

#include <optional>

#include "base/functional/callback.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_worker_client_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/storage_access_api/status.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
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
class StoragePartitionImpl;
class WorkerScriptLoaderFactory;

// Contains the result of successful worker script fetch. On fetch failure,
// `std::nullopt` is used instead.
struct CONTENT_EXPORT WorkerScriptFetcherResult final {
  WorkerScriptFetcherResult(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      PolicyContainerPolicies policy_container_policies,
      const GURL& final_response_url);
  ~WorkerScriptFetcherResult();

  WorkerScriptFetcherResult(WorkerScriptFetcherResult&& other);
  WorkerScriptFetcherResult& operator=(WorkerScriptFetcherResult&& other);

  // Sent to the renderer process and is to be used to request subresources
  // where applicable. For example, this allows the dedicated worker to load
  // chrome-extension:// URLs which the renderer's default loader factory can't
  // load.
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories;

  // Sent to the renderer process and to be used to load the worker main script
  // pre-requested by the browser process.
  // Always non-null and contains `response_head` and
  // `response_head->parsed_headers`.
  blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params;

  PolicyContainerPolicies policy_container_policies;

  // The script response URL.
  // https://fetch.spec.whatwg.org/#concept-response-url
  GURL final_response_url;
};

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
  // Called with the result of fetching a script upon response received.
  using CompletionCallback =
      base::OnceCallback<void(std::optional<WorkerScriptFetcherResult>)>;

  // Used for specifying how URLLoaderFactoryBundle is used.
  enum class LoaderType { kMainResource, kSubResource };

  // Initiates a browser-side worker script fetch.
  // Creates a `WorkerScriptFetcher` and starts it.
  //
  // Must be called on the UI thread.
  //
  // - `ancestor_render_frame_host` points to the ancestor frame. If
  //   the worker being created is nested, then this is the ancestor of the
  //   creator worker. Otherwise, this is the creator frame. Cannot be nullptr.
  //   For dedicated workers, when the lifetime of the `DedicatedWorkerHost`
  //   does not exactly align with the parents, and they are destroyed
  //   asynchronously via mojo by the time the fetch is about to start,
  //   this method must not be called.
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
      RenderFrameHostImpl& ancestor_render_frame_host,
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
      DevToolsAgentHostImpl* devtools_agent_host,
      const base::UnguessableToken& devtools_worker_token,
      bool require_cross_site_request_for_cookies,
      net::StorageAccessApiStatus storage_access_api_status,
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
  // - `completion_status` is nullptr.
  //
  // In case of error:
  //
  // - `main_script_load_params` is nullptr.
  // - `completion_status` is not nullptr.
  using CreateAndStartCallback = base::OnceCallback<void(
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
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
      RenderFrameHostImpl& ancestor_render_frame_host,
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
      DevToolsAgentHostImpl* devtools_agent_host,
      const base::UnguessableToken& devtools_worker_token,
      bool require_cross_site_request_for_cookies,
      WorkerScriptFetcher::CompletionCallback callback);

  void Start(std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles);

  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
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

  std::vector<net::RedirectInfo> redirect_infos_;
  std::vector<network::mojom::URLResponseHeadPtr> redirect_response_heads_;

  base::WeakPtrFactory<WorkerScriptFetcher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_
