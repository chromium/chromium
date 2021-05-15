// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_

#include "base/callback.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace blink {
class ThrottlingURLLoader;
class URLLoaderThrottle;
}  // namespace blink

namespace content {

class WorkerScriptLoaderFactory;

// NetworkService (PlzWorker):
// This is an implementation of the URLLoaderClient for web worker's main script
// fetch. The loader and client bounded with this class are to be unbound and
// forwarded to the renderer process on OnStartLoadingResponseBody, and the
// resource loader in the renderer process will take them over.
//
// WorkerScriptFetcher deletes itself when the ownership of the loader and
// client is passed to the renderer, or on failure. It lives on the UI
// thread.
class WorkerScriptFetcher : public network::mojom::URLLoaderClient {
 public:
  using CreateAndStartCallback =
      base::OnceCallback<void(blink::mojom::WorkerMainScriptLoadParamsPtr,
                              absl::optional<SubresourceLoaderParams>,
                              bool /* success */)>;

  // Called on the IO thread, and calls |callback| on the IO thread when
  // OnReceiveResponse is called on |this|.
  static void CreateAndStart(
      std::unique_ptr<WorkerScriptLoaderFactory> script_loader_factory,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
      std::unique_ptr<network::ResourceRequest> resource_request,
      CreateAndStartCallback callback);

 private:
  WorkerScriptFetcher(
      std::unique_ptr<WorkerScriptLoaderFactory> script_loader_factory,
      std::unique_ptr<network::ResourceRequest> resource_request,
      CreateAndStartCallback callback);

  ~WorkerScriptFetcher() override;

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

  std::unique_ptr<WorkerScriptLoaderFactory> script_loader_factory_;

  // Request ID for a browser-initiated request.
  const int request_id_;

  std::unique_ptr<network::ResourceRequest> resource_request_;
  CreateAndStartCallback callback_;

  // URLLoader instance backed by a request interceptor (e.g.,
  // AppCacheRequestHandler) or the network service.
  std::unique_ptr<blink::ThrottlingURLLoader> url_loader_;

  // URLLoader instance for handling a response received from the default
  // network loader. This can be provided by an interceptor. For example,
  // AppCache's interceptor creates this for AppCache's fallback case.
  mojo::PendingRemote<network::mojom::URLLoader> response_url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> response_url_loader_receiver_{
      this};

  absl::optional<SubresourceLoaderParams> subresource_loader_params_;

  std::vector<net::RedirectInfo> redirect_infos_;
  std::vector<network::mojom::URLResponseHeadPtr> redirect_response_heads_;
  network::mojom::URLResponseHeadPtr response_head_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCHER_H_
