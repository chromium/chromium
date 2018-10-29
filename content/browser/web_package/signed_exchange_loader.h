// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_LOADER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_LOADER_H_

#include "base/callback.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/origin.h"

namespace net {
class SourceStream;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class SignedExchangeDevToolsProxy;
class SignedExchangeHandler;
class SignedExchangeHandlerFactory;
class SignedExchangePrefetchMetricRecorder;
class URLLoaderThrottle;
class SourceStreamToDataPipe;

// SignedExchangeLoader handles an origin-signed HTTP exchange response. It is
// created when a SignedExchangeRequestHandler recieves an origin-signed HTTP
// exchange response, and is owned by the handler until the StartLoaderCallback
// of SignedExchangeRequestHandler::StartResponse is called. After that, it is
// owned by the URLLoader mojo endpoint.
class SignedExchangeLoader final : public network::mojom::URLLoaderClient,
                                   public network::mojom::URLLoader {
 public:
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<content::URLLoaderThrottle>>()>;

  // If |should_redirect_on_failure| is true, verification failure causes a
  // redirect to the fallback URL.
  SignedExchangeLoader(
      const GURL& outer_request_url,
      const network::ResourceResponseHead& outer_response,
      network::mojom::URLLoaderClientPtr forwarding_client,
      network::mojom::URLLoaderClientEndpointsPtr endpoints,
      url::Origin request_initiator,
      uint32_t url_loader_options,
      int load_flags,
      bool should_redirect_on_failure,
      const base::Optional<base::UnguessableToken>& throttling_profile_id,
      std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
      scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder);
  ~SignedExchangeLoader() override;


  // network::mojom::URLLoaderClient implementation
  // Only OnStartLoadingResponseBody() and OnComplete() are called.
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // network::mojom::URLLoader implementation
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void ConnectToClient(network::mojom::URLLoaderClientPtr client);

  const base::Optional<GURL>& fallback_url() const { return fallback_url_; };

  const base::Optional<GURL>& inner_request_url() const {
    return inner_request_url_;
  }

  // Set nullptr to reset the mocking.
  CONTENT_EXPORT static void SetSignedExchangeHandlerFactoryForTest(
      SignedExchangeHandlerFactory* factory);

 private:
  class ResponseTimingInfo;

  // Called from |signed_exchange_handler_| when it finds an origin-signed HTTP
  // exchange.
  void OnHTTPExchangeFound(
      SignedExchangeLoadResult result,
      net::Error error,
      const GURL& request_url,
      const std::string& request_method,
      const network::ResourceResponseHead& resource_response,
      std::unique_ptr<net::SourceStream> payload_stream);

  void FinishReadingBody(int result);

  const GURL outer_request_url_;

  // This timing info is used to create a dummy redirect response.
  std::unique_ptr<const ResponseTimingInfo> outer_response_timing_info_;

  // The outer response of signed HTTP exchange which was received from network.
  const network::ResourceResponseHead outer_response_;

  // This client is alive until OnHTTPExchangeFound() is called.
  network::mojom::URLLoaderClientPtr forwarding_client_;

  // This |url_loader_| is the pointer of the network URL loader.
  network::mojom::URLLoaderPtr url_loader_;
  // This binding connects |this| with the network URL loader.
  mojo::Binding<network::mojom::URLLoaderClient> url_loader_client_binding_;

  // This is pending until connected by ConnectToClient().
  network::mojom::URLLoaderClientPtr client_;

  // This URLLoaderClientRequest is used by ConnectToClient() to connect
  // |client_|.
  network::mojom::URLLoaderClientRequest pending_client_request_;

  std::unique_ptr<SignedExchangeHandler> signed_exchange_handler_;
  std::unique_ptr<SourceStreamToDataPipe> body_data_pipe_adapter_;

  // Kept around until ProceedWithResponse is called.
  mojo::ScopedDataPipeConsumerHandle pending_body_consumer_;

  url::Origin request_initiator_;
  const uint32_t url_loader_options_;
  const int load_flags_;
  const bool should_redirect_on_failure_;
  const base::Optional<base::UnguessableToken> throttling_profile_id_;
  std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  URLLoaderThrottlesGetter url_loader_throttles_getter_;
  base::RepeatingCallback<int(void)> frame_tree_node_id_getter_;
  scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder_;

  base::Optional<net::SSLInfo> ssl_info_;

  std::string content_type_;

  base::Optional<GURL> fallback_url_;
  base::Optional<GURL> inner_request_url_;

  base::WeakPtrFactory<SignedExchangeLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_LOADER_H_
