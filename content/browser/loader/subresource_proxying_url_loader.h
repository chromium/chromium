// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SUBRESOURCE_PROXYING_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_SUBRESOURCE_PROXYING_URL_LOADER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

// A URLLoader for handling proxied subresource request. Note that prefetch is
// also proxied but uses a separate loader.
//
// This loader intercepts requests and responses (including the redirected
// ones), and forwards the potentially modified requests and responses to the
// originally intended endpoints (with `loader_` and `forwarding_client_`).
class CONTENT_EXPORT SubresourceProxyingURLLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient {
 public:
  class Interceptor {
   public:
    virtual void WillStartRequest(net::HttpRequestHeaders& headers) = 0;

    // `removed_headers` and `modified_headers` can be modified by other
    // interceptors, and registration order would matter.
    virtual void WillFollowRedirect(
        const std::optional<GURL>& new_url,
        std::vector<std::string>& removed_headers,
        net::HttpRequestHeaders& modified_headers) = 0;

    // `head` can be modified by other interceptors, and registration order
    // would matter.
    virtual void OnReceiveRedirect(
        const net::RedirectInfo& redirect_info,
        network::mojom::URLResponseHeadPtr& head) = 0;

    // `head` can be modified by other interceptors, and registration order
    // would matter.
    virtual void OnReceiveResponse(
        network::mojom::URLResponseHeadPtr& head) = 0;

    virtual ~Interceptor() = default;
  };

  SubresourceProxyingURLLoader(
      WeakDocumentPtr document,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory);

  SubresourceProxyingURLLoader(const SubresourceProxyingURLLoader&) = delete;
  SubresourceProxyingURLLoader& operator=(const SubresourceProxyingURLLoader&) =
      delete;

  ~SubresourceProxyingURLLoader() override;

 private:
  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void OnNetworkConnectionError();

  // The initial request state without any modification.
  const network::ResourceRequest resource_request_;

  // For the actual request.
  mojo::Remote<network::mojom::URLLoader> loader_;

  // The client to forward the response to.
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  std::vector<std::unique_ptr<Interceptor>> interceptors_;

  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SUBRESOURCE_PROXYING_URL_LOADER_H_
