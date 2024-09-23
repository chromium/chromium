// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_LOADER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_LOADER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/origin.h"

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace net {
class SourceStream;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class SourceStreamToDataPipe;
}  // namespace network

namespace content {

class PrefetchedSignedExchangeCacheEntry;
class SignedExchangeDevToolsProxy;
class SignedExchangeHandler;
class SignedExchangeHandlerFactory;
class SignedExchangeReporter;

// SignedExchangeLoader handles an origin-signed HTTP exchange response. It is
// created when a SignedExchangeRequestHandler recieves an origin-signed HTTP
// exchange response, and is owned by the handler until the StartLoaderCallback
// of SignedExchangeRequestHandler::StartResponse is called. After that, it is
// owned by the URLLoader mojo endpoint.
class CONTENT_EXPORT SignedExchangeLoader final
    : public network::mojom::URLLoaderClient,
      public network::mojom::URLLoader {
 public:
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>()>;

  // If |should_redirect_on_failure| is true, verification failure causes a
  // redirect to the fallback URL.
  SignedExchangeLoader(
      const network::ResourceRequest& outer_request,
      network::mojom::URLResponseHeadPtr outer_response_head,
      mojo::ScopedDataPipeConsumerHandle outer_response_body,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client,
      network::mojom::URLLoaderClientEndpointsPtr endpoints,
      uint32_t url_loader_options,
      bool should_redirect_on_failure,
      std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy,
      std::unique_ptr<SignedExchangeReporter> reporter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      FrameTreeNodeId frame_tree_node_id,
      const std::string& accept_langs,
      bool keep_entry_for_prefetch_cache);

  SignedExchangeLoader(const SignedExchangeLoader&) = delete;
  SignedExchangeLoader& operator=(const SignedExchangeLoader&) = delete;

  ~SignedExchangeLoader() override;

  // network::mojom::URLLoaderClient implementation
  // Only OnComplete() is called.
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

  // network::mojom::URLLoader implementation
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void ConnectToClient(
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  const std::optional<GURL>& fallback_url() const { return fallback_url_; }

  const std::optional<GURL>& inner_request_url() const {
    return inner_request_url_;
  }

  // Called to get the information about the loaded signed exchange. To call
  // this method, |keep_entry_for_prefetch_cache| constructor argument must be
  // set.
  std::unique_ptr<PrefetchedSignedExchangeCacheEntry>
  TakePrefetchedSignedExchangeCacheEntry();

  // Set nullptr to reset the mocking.
  static void SetSignedExchangeHandlerFactoryForTest(
      SignedExchangeHandlerFactory* factory);

 private:
  // Called from |signed_exchange_handler_| when it finds an origin-signed HTTP
  // exchange.
  void OnHTTPExchangeFound(SignedExchangeLoadResult result,
                           net::Error error,
                           const GURL& request_url,
                           network::mojom::URLResponseHeadPtr resource_response,
                           std::unique_ptr<net::SourceStream> payload_stream);

  void FinishReadingBody(int result);
  void NotifyClientOnCompleteIfReady();
  void ReportLoadResult(SignedExchangeLoadResult result);

  const network::ResourceRequest outer_request_;

  // The outer response of signed HTTP exchange which was received from network.
  network::mojom::URLResponseHeadPtr outer_response_head_;

  // This client is alive until OnHTTPExchangeFound() is called.
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  // This |url_loader_| is the remote of the network URL loader.
  mojo::Remote<network::mojom::URLLoader> url_loader_;
  // This receiver connects |this| with the network URL loader.
  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_client_receiver_{
      this};

  // This is pending until connected by ConnectToClient().
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  // This pending receiver is used by ConnectToClient() to connect |client_|.
  mojo::PendingReceiver<network::mojom::URLLoaderClient>
      pending_client_receiver_;

  std::unique_ptr<SignedExchangeReporter> reporter_;

  // `signed_exchange_handler_` borrows reference from `reporter_`, so it needs
  // to be declared last, so that it is destroyed first.
  std::unique_ptr<SignedExchangeHandler> signed_exchange_handler_;
  std::unique_ptr<network::SourceStreamToDataPipe> body_data_pipe_adapter_;

  const uint32_t url_loader_options_;
  const bool should_redirect_on_failure_;

  std::optional<net::SSLInfo> ssl_info_;

  std::optional<GURL> fallback_url_;
  std::optional<GURL> inner_request_url_;

  struct OuterResponseLengthInfo {
    int64_t encoded_data_length;
    int64_t decoded_body_length;
  };
  // Set when URLLoaderClient::OnComplete() is called.
  std::optional<OuterResponseLengthInfo> outer_response_length_info_;

  // Set when |body_data_pipe_adapter_| finishes loading the decoded body.
  std::optional<int> decoded_body_read_result_;

  // Keep the signed exchange info to be stored to
  // PrefetchedSignedExchangeCache.
  std::unique_ptr<PrefetchedSignedExchangeCacheEntry> cache_entry_;

  base::WeakPtrFactory<SignedExchangeLoader> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_LOADER_H_
