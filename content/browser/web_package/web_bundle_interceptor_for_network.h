// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_NETWORK_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_NETWORK_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "url/gurl.h"

namespace content {

class WebBundleReader;
class WebBundleURLLoaderFactory;

// A class to inherit NavigationLoaderInterceptor for a navigation to a
// Web Bundle file on HTTPS server (eg: "https://example.com/a.wbn").
// The overridden methods of NavigationLoaderInterceptor are called in the
// following sequence:
// [1] MaybeCreateLoader() is called for all navigation requests. It calls the
//     |callback| with a null RequestHandler.
// [2] MaybeCreateLoaderForResponse() is called for all navigation responses.
//     - If the response mime type is not "application/webbundle", or attachment
//       Content-Disposition header is set, returns false.
//     - If the URL isn't HTTPS nor localhost HTTP, or the Content-Length header
//       is not a positive value, completes the requests with
//       ERR_INVALID_WEB_BUNDLE and returns true.
//     - Otherwise starts reading the metadata and returns true. Once the
//       metadata is read, OnMetadataReady() is called, and a redirect loader is
//       created to redirect the navigation request to the Bundle's primary URL
//       ("https://example.com/a.html").
// [3] MaybeCreateLoader() is called again for the redirect. It continues on
//     StartResponse() to create the loader for the main resource.
class WebBundleInterceptorForNetwork final
    : public NavigationLoaderInterceptor {
 public:
  WebBundleInterceptorForNetwork(WebBundleDoneCallback done_callback,
                                 BrowserContext* browser_context,
                                 int frame_tree_node_id);
  WebBundleInterceptorForNetwork(const WebBundleInterceptorForNetwork&) =
      delete;
  WebBundleInterceptorForNetwork& operator=(
      const WebBundleInterceptorForNetwork&) = delete;

  ~WebBundleInterceptorForNetwork() override;

 private:
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

  void StartResponse(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  WebBundleDoneCallback done_callback_;
  raw_ptr<BrowserContext> browser_context_;
  const int frame_tree_node_id_;
  scoped_refptr<WebBundleReader> reader_;
  GURL primary_url_;
  std::unique_ptr<WebBundleURLLoaderFactory> url_loader_factory_;

  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebBundleInterceptorForNetwork> weak_factory_{this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_NETWORK_H_
