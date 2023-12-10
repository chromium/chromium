// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_URL_LOADER_REQUEST_INTERCEPTOR_H_
#define CONTENT_PUBLIC_BROWSER_URL_LOADER_REQUEST_INTERCEPTOR_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace blink {
class ThrottlingURLLoader;
}

namespace network {
struct ResourceRequest;
}

namespace content {

class BrowserContext;

// URLLoaderRequestInterceptor is given a chance to create a URLLoader and
// intercept a navigation request before the request is handed off to the
// default URLLoader, e.g. the one from the network service.
// URLLoaderRequestInterceptor is a per-request object and kept around during
// the lifetime of a navigation request (including multiple redirect legs).
// All methods are called on the UI thread.
class CONTENT_EXPORT URLLoaderRequestInterceptor {
 public:
  URLLoaderRequestInterceptor() = default;
  virtual ~URLLoaderRequestInterceptor() = default;

  using RequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader>,
      mojo::PendingRemote<network::mojom::URLLoaderClient>)>;
  using LoaderCallback = base::OnceCallback<void(RequestHandler)>;

  // Asks this handler to handle this resource load request.
  // The handler must invoke |callback| eventually with either a non-null
  // RequestHandler indicating its willingness to handle the request, or a null
  // RequestHandler to indicate that someone else should handle the request.
  //
  // The |tentative_resource_request| passed to this function and the resource
  // request later passed to the RequestHandler given to |callback| may not be
  // exactly the same. See documentation for
  // NavigationLoaderInterceptor::MaybeCreateLoader.
  virtual void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback) = 0;

  // Returns true if the interceptor creates a loader for the `response_head`
  // and `response_body` passed.  `request` is the latest request whose request
  // URL may include URL fragment.  An example of where this is used is
  // WebBundles where the URL is used to check if the content must be
  // downloaded.  The URLLoader remote is returned in the `loader` parameter.
  // The mojo::PendingReceiver for the URLLoaderClient is returned in the
  // `client_receiver` parameter.
  // `status` is the loader completion status, allowing the interceptor to
  // handle failed loads differently from successful loads. For requests that
  // successfully received a response, this will be a URLLoaderCompletionStatus
  // with an error code of `net::OK`. For requests that failed, this will be a
  // URLLoaderCompletionStatus with the underlying net error.
  // The `url_loader` points to the ThrottlingURLLoader that currently controls
  // the request. It can be optionally consumed to get the current
  // URLLoaderClient and URLLoader so that the implementation can rebind them to
  // intercept the in-flight loading if necessary.  Note that the `url_loader`
  // will be reset after this method is called, which will also drop the
  // URLLoader held by `url_loader_` if it is not unbound yet.
  // `skip_other_interceptors` is set to true when this interceptor will
  // exclusively handle the navigation even after redirections. TODO(horo): This
  // flag was introduced to skip service worker after signed exchange redirect.
  // Remove this flag when we support service worker and signed exchange
  // integration. See crbug.com/894755#c1. Nullptr is not allowed.
  virtual bool MaybeCreateLoaderForResponse(
      const network::URLLoaderCompletionStatus& status,
      const network::ResourceRequest& request,
      network::mojom::URLResponseHeadPtr* response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      mojo::PendingRemote<network::mojom::URLLoader>* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_URL_LOADER_REQUEST_INTERCEPTOR_H_
