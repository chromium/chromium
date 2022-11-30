// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_FROM_NETWORK_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_FROM_NETWORK_H_

#include "base/memory/weak_ptr.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "content/browser/web_package/web_bundle_interceptor_for_history_navigation.h"
#include "content/browser/web_package/web_bundle_url_loader_factory.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"

class GURL;

namespace network {
struct ResourceRequest;
}  // namespace network

namespace content {

class WebBundleReader;
class WebBundleSource;

// A class to inherit NavigationLoaderInterceptor for the history navigation to
// a Web Bundle from network when the previous WebBundleReader was deleted.
// The overridden methods of NavigationLoaderInterceptor are called in the
// following sequence:
// [1] MaybeCreateLoader() is called for the history navigation request. It
//     continues on StartRedirectResponse() to redirect to the Web Bundle URL.
// [2] MaybeCreateLoader() is called again for the Web Bundle request. It calls
//     the |callback| with a null RequestHandler.
//     If server returned a redirect response instead of the Web Bundle,
//     MaybeCreateLoader() is called again for the redirected new URL, in such
//     it continues on StartErrorResponseForUnexpectedRedirect() and returns an
//     error.
//     Note that the Web Bundle URL should be loaded as cache-preferring load,
//     but it may still go to the server if the Bundle is served with
//     "no-store".
// [3] MaybeCreateLoaderForResponse() is called for all navigation responses.
//     - If the response mime type is not "application/webbundle", this means
//       the server response is not a Web Bundle. So handles as an error.
//     - If the Content-Length header is not a positive value, handles as an
//       error.
//     - Otherwise starts reading the metadata and returns true. Once the
//       metadata is read, OnMetadataReady() is called, and a redirect loader is
//       created to redirect the navigation request to the target inner URL.
// [4] MaybeCreateLoader() is called again for target inner URL. It continues
//     on StartResponse() to create the loader for the main resource.
class WebBundleInterceptorForHistoryNavigationFromNetwork final
    : public WebBundleInterceptorForHistoryNavigation {
 public:
  WebBundleInterceptorForHistoryNavigationFromNetwork(
      std::unique_ptr<WebBundleSource> source,
      const GURL& target_inner_url,
      WebBundleDoneCallback done_callback,
      int frame_tree_node_id);
  WebBundleInterceptorForHistoryNavigationFromNetwork(
      const WebBundleInterceptorForHistoryNavigationFromNetwork&) = delete;
  WebBundleInterceptorForHistoryNavigationFromNetwork& operator=(
      const WebBundleInterceptorForHistoryNavigationFromNetwork&) = delete;

  ~WebBundleInterceptorForHistoryNavigationFromNetwork() override;

 private:
  enum class State {
    kInitial,
    kRedirectedToWebBundle,
    kWebBundleRecieved,
    kMetadataReady,
  };

  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override;

  bool MaybeCreateLoaderForResponse(
      const network::ResourceRequest& request,
      network::mojom::URLResponseHeadPtr* response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      mojo::PendingRemote<network::mojom::URLLoader>* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors,
      bool* will_return_and_handle_unsafe_redirect) override;

  void OnMetadataReady(network::ResourceRequest request,
                       web_package::mojom::BundleMetadataParseErrorPtr error);

  void StartRedirectResponse(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  void StartErrorResponseForUnexpectedRedirect(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  void StartResponse(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  State state_ = State::kInitial;
  std::unique_ptr<WebBundleSource> source_;
  scoped_refptr<WebBundleReader> reader_;

  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  base::WeakPtrFactory<WebBundleInterceptorForHistoryNavigationFromNetwork>
      weak_factory_{this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_FROM_NETWORK_H_
