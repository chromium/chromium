// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_LOADER_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_LOADER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/public/browser/service_worker_client_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_timing_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace net {
class IsolationInfo;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class BrowserContext;
class NavigationLoaderInterceptor;
class ServiceWorkerMainResourceHandle;
class ServiceWorkerMainResourceLoaderInterceptor;

// The URLLoader for loading a shared worker script. Only used for the main
// script request.
//
// This acts much like NavigationURLLoaderImpl. It allows a
// NavigationLoaderInterceptor to intercept the request with its own loader, and
// goes to |default_loader_factory| otherwise. Once a loader is started, this
// class acts as the URLLoaderClient for it, forwarding messages to the outer
// client. On redirects, it starts over with the new request URL, possibly
// starting a new loader and becoming the client of that.
//
// Lives on the UI thread.
class CONTENT_EXPORT WorkerScriptLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient {
 public:
  // Returns the browser context, or nullptr during shutdown. Must be called on
  // the UI thread.
  using BrowserContextGetter = base::RepeatingCallback<BrowserContext*(void)>;

  // |default_loader_factory| is used to load the script if the load is not
  // intercepted by a service worker. Typically it will load the script from the
  // NetworkService. However, it may internally contain non-NetworkService
  // factories used for non-http(s) URLs, e.g., a chrome-extension:// URL.
  WorkerScriptLoader(
      int process_id,
      const DedicatedOrSharedWorkerToken& worker_token,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      base::WeakPtr<ServiceWorkerMainResourceHandle> service_worker_handle,
      const BrowserContextGetter& browser_context_getter,
      scoped_refptr<network::SharedURLLoaderFactory> default_loader_factory,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  WorkerScriptLoader(const WorkerScriptLoader&) = delete;
  WorkerScriptLoader& operator=(const WorkerScriptLoader&) = delete;

  ~WorkerScriptLoader() override;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void OnFetcherCallbackCalled();

  base::WeakPtr<WorkerScriptLoader> GetWeakPtr();

 private:
  void Abort();
  void Start();
  void MaybeStartLoader(
      ServiceWorkerMainResourceLoaderInterceptor* interceptor,
      std::optional<NavigationLoaderInterceptor::Result> interceptor_result);
  void LoadFromNetwork();
  void CommitCompleted();

  std::unique_ptr<ServiceWorkerMainResourceLoaderInterceptor> interceptor_;

  const int32_t request_id_;
  const uint32_t options_;
  network::ResourceRequest resource_request_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  base::WeakPtr<ServiceWorkerMainResourceHandle> service_worker_handle_;
  BrowserContextGetter browser_context_getter_;
  scoped_refptr<network::SharedURLLoaderFactory> default_loader_factory_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  std::optional<net::RedirectInfo> redirect_info_;
  int redirect_limit_ = net::URLRequest::kMaxRedirects;

  mojo::Remote<network::mojom::URLLoader> url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_client_receiver_{
      this};
  // The factory used to request the script. This is the same as
  // |default_loader_factory_| if a service worker didn't elect to handle the
  // request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Valid transitions:
  // - kInitial -> kFetcherCallbackCalled or kOnCompleteCalled -> kCompleted
  // - kInitial -> kCompleted (failure cases only)
  // See the comment at the `CommitCompleted()` definition for more context.
  enum class State {
    kInitial,

    // `WorkerScriptFetcher::callback_` was invoked.
    kFetcherCallbackCalled,

    // `WorkerScriptLoader::OnComplete()` was called.
    kOnCompleteCalled,

    // `WorkerScriptLoader::CommitCompleted()` was called.
    kCompleted,
  } state_{State::kInitial};
  std::optional<network::URLLoaderCompletionStatus> complete_status_;

  base::WeakPtrFactory<WorkerScriptLoader> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_LOADER_H_
