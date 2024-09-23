// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_LOADER_INTERCEPTOR_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "content/browser/loader/response_head_update_params.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/load_timing_info.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace blink {
class ThrottlingURLLoader;
}  // namespace blink

namespace network {
struct ResourceRequest;
}  // namespace network

namespace content {

class BrowserContext;

// NavigationLoaderInterceptor is given a chance to create a URLLoader and
// intercept a navigation request before the request is handed off to the
// default URLLoader, e.g. the one from the network service.
// NavigationLoaderInterceptor is a per-request object and kept around during
// the lifetime of a navigation request (including multiple redirect legs).
// All methods are called on the UI thread.
class CONTENT_EXPORT NavigationLoaderInterceptor {
 public:
  NavigationLoaderInterceptor() = default;
  virtual ~NavigationLoaderInterceptor() = default;

  // When `LoaderCallback` is called with non-nullopt `Result`:
  // - Intercept the navigation request using `single_request_factory` if
  //   non-null, or otherwise fall back to the default loader factory.
  // - Intercept subresource requests using `subresource_loader_params` if
  //   non-null.
  // - Skip all subsequent interceptors.
  // When `LoaderCallback` is called with std::nullopt:
  // - The navigation/subresource requests are not intercepted.
  // - Fall back to the next interceptor if any, or otherwise to the network.
  struct CONTENT_EXPORT Result final {
    Result(
        scoped_refptr<network::SharedURLLoaderFactory> single_request_factory,
        SubresourceLoaderParams subresource_loader_params,
        ResponseHeadUpdateParams response_head_update_params = {});

    ~Result();
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result(Result&&);
    Result& operator=(Result&&);

    // When non-null, `single_request_factory` is used to handle the request.
    // When null, the fallback network-service factory is used.
    scoped_refptr<network::SharedURLLoaderFactory> single_request_factory;

    // Used for subsequent URL requests going forward.
    //
    // Note that `single_request_factory` can be nullptr while
    // `subresource_loader_params` can have non-default values if it does NOT
    // want to handle the specific request but wants to handle the subsequent
    // resource requests.
    SubresourceLoaderParams subresource_loader_params;

    // When `single_request_factory` is null, used to update the response params
    // in non-intercepted loader via
    // `NavigationURLLoaderImpl::head_update_params_`.
    ResponseHeadUpdateParams response_head_update_params;
  };

  using LoaderCallback = base::OnceCallback<void(std::optional<Result>)>;
  using FallbackCallback = base::OnceCallback<void(ResponseHeadUpdateParams)>;

  // Asks this interceptor to handle this resource load request.
  // The interceptor must always invoke `callback`.
  //
  // The `tentative_resource_request` passed to this function and the resource
  // request later passed to the loader factory given to `callback` may not be
  // exactly the same, because URLLoaderThrottles may rewrite the request
  // between the two calls. However the URL must remain constant between the
  // two, as any modifications on the URL done by URLLoaderThrottles must result
  // in an (internal) redirect, which must restart the request with a new
  // MaybeCreateLoader().
  //
  // This interceptor might initially elect to handle the request, but later
  // decide to fall back to the default behavior. In that case, it can invoke
  // `fallback_callback_for_service_worker` to do so. An example of this is when
  // a service worker decides to handle the request because it is in-scope, but
  // the service worker JavaScript execution does not result in a response
  // provided, so fallback to network is required.
  //
  // `fallback_callback_for_service_worker` is only for service workers to
  // fallback to the network after initially electing to intercept the request.
  // It must be called after `callback` is called with non-null
  // `single_request_factory` and prior to the interceptor making any
  // URLLoaderClient calls.
  //
  // `callback` and `fallback_callback_for_service_worker` must not be invoked
  // after the destruction of this interceptor.
  //
  // TODO(crbug.com/40251638): Possibly remove
  // `fallback_callback_for_service_worker` to simplify the ServiceWorker
  // interception.
  virtual void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback_for_service_worker) = 0;

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
  // intercept the inflight loading if necessary.  Note that the `url_loader`
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
      blink::ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_LOADER_INTERCEPTOR_H_
