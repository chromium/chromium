// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "url/origin.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace blink {
class ThrottlingURLLoader;
class URLLoaderThrottle;
}  // namespace blink

namespace content {

class SignedExchangeLoader;

class SignedExchangeRequestHandler final : public NavigationLoaderInterceptor {
 public:
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>()>;

  static bool IsSupportedMimeType(const std::string& mime_type);

  SignedExchangeRequestHandler(
      uint32_t url_loader_options,
      FrameTreeNodeId frame_tree_node_id,
      const base::UnguessableToken& devtools_navigation_token,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      std::string accept_langs);

  SignedExchangeRequestHandler(const SignedExchangeRequestHandler&) = delete;
  SignedExchangeRequestHandler& operator=(const SignedExchangeRequestHandler&) =
      delete;

  ~SignedExchangeRequestHandler() override;

  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override;
  bool MaybeCreateLoaderForResponse(
      const network::URLLoaderCompletionStatus& status,
      const network::ResourceRequest& request,
      network::mojom::URLResponseHeadPtr* response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      mojo::PendingRemote<network::mojom::URLLoader>* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors) override;

 private:
  void StartResponse(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Valid after MaybeCreateLoaderForResponse intercepts the request and until
  // the loader is re-bound to the new client for the redirected request in
  // StartResponse.
  std::unique_ptr<SignedExchangeLoader> signed_exchange_loader_;

  const uint32_t url_loader_options_;
  const FrameTreeNodeId frame_tree_node_id_;
  const base::UnguessableToken devtools_navigation_token_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  URLLoaderThrottlesGetter url_loader_throttles_getter_;
  const std::string accept_langs_;

  base::WeakPtrFactory<SignedExchangeRequestHandler> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_HANDLER_H_
