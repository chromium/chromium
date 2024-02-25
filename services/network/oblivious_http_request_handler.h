// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_OBLIVIOUS_HTTP_REQUEST_HANDLER_H_
#define SERVICES_NETWORK_OBLIVIOUS_HTTP_REQUEST_HANDLER_H_

#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/mojom/oblivious_http_request.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace net {
class HttpRequestHeaders;
}

namespace network {

class NetworkContext;
class TrustTokenStatusOrRequestHelper;

// Handles the request based on the OHTTP specification:
// https://ietf-wg-ohai.github.io/oblivious-http/draft-ietf-ohai-ohttp.html
//
// The basic steps are:
// 1. Construct the inner binary HTTP request based on the provided parameters
// (leveraging binary HTTP support in quiche).
// 2. Encrypt the inner request based on the Key Configuration (leveraging
// upcoming OHTTP support in quiche)
// 3. Create a network::ResourceRequest to perform an HTTP POST to the relay
// URL with the provided IsolationInfo. The request payload will be set to the
// encrypted inner request.
// 4. The ResourceRequest should be passed to a SimpleURLLoader to complete
// the request.
// 5. When the callback from the SimpleURLLoader is received and there is no
// error, decrypt the response body to get the inner response.
// 6. Check the inner binary HTTP response for error. Finish any pending trust
// token operations related to this request. Then perform the callback,
// specifying the inner response body if there was any.
class COMPONENT_EXPORT(NETWORK_SERVICE) ObliviousHttpRequestHandler {
 public:
  // The network context must outlive this object.
  explicit ObliviousHttpRequestHandler(NetworkContext* context);

  ~ObliviousHttpRequestHandler();

  // Completes steps 1-4 of the request procedure above.
  void StartRequest(mojom::ObliviousHttpRequestPtr request,
                    mojo::PendingRemote<mojom::ObliviousHttpClient> client);

  void SetURLLoaderFactoryForTesting(
      mojo::PendingRemote<mojom::URLLoaderFactory> factory) {
    url_loader_factory_.reset();
    url_loader_factory_.Bind(std::move(factory));
  }

 private:
  class RequestState;

  // Callback from TrustTokenRequestHelperFactory during HandleRequest. Verifies
  // trust token helper was created correctly and calls Begin() on it to start
  // the trust token operation with headers stored in the RequestState.
  void OnDoneConstructingTrustTokenHelper(
      mojo::RemoteSetElementId id,
      TrustTokenStatusOrRequestHelper status_or_helper);

  // Callback from TrustTokenRequestHelper::Begin. Verifies the trust token
  // operation started successfully and then calls ContinueHandlingRequest.
  void OnDoneBeginningTrustTokenOperation(
      mojo::RemoteSetElementId id,
      std::optional<net::HttpRequestHeaders> headers,
      mojom::TrustTokenOperationStatus status);

  // Constructs the binary HTTP request including any trust token headers in the
  // RequestState, encrypts the request, and starts the outer request's
  // SimpleURLLoader.
  void ContinueHandlingRequest(std::optional<net::HttpRequestHeaders> headers,
                               mojo::RemoteSetElementId id);

  // Calls the completed event with the specified net error code and HTTP
  // response error code on the corresponding client. The client with the
  // specified id must be in the `clients_` set and the `client_state_` map.
  void RespondWithError(mojo::RemoteSetElementId id,
                        int error_code,
                        std::optional<int> outer_response_error_code);

  // Called by the SimpleURLLoader when the outer request has completed.
  // Performs steps 5 and 6 of the OHTTP request procedure above.
  void OnRequestComplete(mojo::RemoteSetElementId id,
                         std::unique_ptr<std::string> response);

  // Callback from TrustTokenRequestHelper::Finalize. Checks that the trust
  // token operation completed successfully and calls the client with the
  // result.
  void OnDoneFinalizingTrustTokenOperation(
      mojo::RemoteSetElementId id,
      int inner_response_code,
      scoped_refptr<net::HttpResponseHeaders> headers,
      std::string body,
      mojom::TrustTokenOperationStatus status);

  // Notifies the client that the request completed successfully with the
  // provided response headers and body.
  void NotifyComplete(mojo::RemoteSetElementId id,
                      int inner_response_code,
                      scoped_refptr<net::HttpResponseHeaders> headers,
                      std::string body);

  // Handles cleaning up when an ObliviousHttpClient disconnects.
  void OnClientDisconnect(mojo::RemoteSetElementId id);

  mojom::URLLoaderFactory* GetURLLoaderFactory();

  // The NetworkContext which owns this ObliviousHttpRequestHandler.
  raw_ptr<NetworkContext> owner_network_context_;
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_;

  // Clients and matching state.
  mojo::RemoteSet<mojom::ObliviousHttpClient> clients_;
  std::map<mojo::RemoteSetElementId, std::unique_ptr<RequestState>>
      client_state_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_OBLIVIOUS_HTTP_REQUEST_HANDLER_H_
