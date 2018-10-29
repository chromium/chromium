// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SCRIPT_FETCHER_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SCRIPT_FETCHER_H_

#include "base/callback.h"
#include "base/optional.h"
#include "content/common/navigation_subresource_loader_params.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/mojom/shared_worker/shared_worker_main_script_load_params.mojom.h"

namespace network {
struct ResourceResponseHead;
struct ResourceRequest;
}  // namespace network

namespace content {

class SharedWorkerScriptLoaderFactory;
class ThrottlingURLLoader;
class URLLoaderThrottle;

// NetworkService (PlzWorker):
// This is an implementation of the URLLoaderClient for shared worker's main
// script fetch. The loader and client bounded with this class are to be unbound
// and forwarded to the renderer process on OnReceiveResponse, and the resource
// loader in the renderer process will take them over.
//
// SharedWorkerScriptFetcher deletes itself when the ownership of the loader and
// client is passed to the renderer, or on failure. It lives on the IO thread.
class SharedWorkerScriptFetcher : public network::mojom::URLLoaderClient {
 public:
  using CreateAndStartCallback =
      base::OnceCallback<void(blink::mojom::SharedWorkerMainScriptLoadParamsPtr,
                              base::Optional<SubresourceLoaderParams>,
                              bool /* success */)>;

  // Called on the IO thread, and calls |callback| on the IO thread when
  // OnReceiveResponse is called on |this|.
  static void CreateAndStart(
      std::unique_ptr<SharedWorkerScriptLoaderFactory> script_loader_factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      std::unique_ptr<network::ResourceRequest> resource_request,
      CreateAndStartCallback callback);

 private:
  SharedWorkerScriptFetcher(
      std::unique_ptr<SharedWorkerScriptLoaderFactory> script_loader_factory,
      std::unique_ptr<network::ResourceRequest> resource_request,
      CreateAndStartCallback callback);

  ~SharedWorkerScriptFetcher() override;

  void Start(std::vector<std::unique_ptr<URLLoaderThrottle>> throttles);

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(const network::ResourceResponseHead& head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  std::unique_ptr<SharedWorkerScriptLoaderFactory> script_loader_factory_;

  std::unique_ptr<network::ResourceRequest> resource_request_;
  CreateAndStartCallback callback_;

  // URLLoader instance backed by a request interceptor (e.g.,
  // AppCacheRequestHandler) or the network service.
  std::unique_ptr<ThrottlingURLLoader> url_loader_;

  // URLLoader instance for handling a response received from the default
  // network loader. This can be provided by an interceptor. For example,
  // AppCache's interceptor creates this for AppCache's fallback case.
  network::mojom::URLLoaderPtr response_url_loader_;
  mojo::Binding<network::mojom::URLLoaderClient> response_url_loader_binding_;

  base::Optional<SubresourceLoaderParams> subresource_loader_params_;

  std::vector<net::RedirectInfo> redirect_infos_;
  std::vector<network::ResourceResponseHead> redirect_response_heads_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SCRIPT_FETCHER_H_
