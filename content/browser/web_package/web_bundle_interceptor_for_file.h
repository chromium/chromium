// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_FILE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_FILE_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace blink {
class ThrottlingURLLoader;
}  // namespace blink

namespace network {
struct ResourceRequest;
}  // namespace network

namespace content {

class WebBundleReader;
class WebBundleURLLoaderFactory;

// A class to inherit NavigationLoaderInterceptor for a navigation to an
// untrustable Web Bundle file (eg: "file:///tmp/a.wbn").
// The overridden methods of NavigationLoaderInterceptor are called in the
// following sequence:
// [1] MaybeCreateLoader() is called for all navigation requests. It calls the
//     |callback| with a null RequestHandler.
// [2] MaybeCreateLoaderForResponse() is called for all navigation responses.
//     If the response mime type is not "application/webbundle", returns false.
//     Otherwise starts reading the metadata and returns true. Once the metadata
//     is read, OnMetadataReady() is called, and a redirect loader is
//     created to redirect the navigation request to the Bundle's synthesized
//     primary URL ("file:///tmp/a.wbn?https://example.com/a.html").
// [3] MaybeCreateLoader() is called again for the redirect. It continues on
//     StartResponse() to create the loader for the main resource.
class WebBundleInterceptorForFile final : public NavigationLoaderInterceptor {
 public:
  WebBundleInterceptorForFile(WebBundleDoneCallback done_callback,
                              int frame_tree_node_id);
  WebBundleInterceptorForFile(const WebBundleInterceptorForFile&) = delete;
  WebBundleInterceptorForFile& operator=(const WebBundleInterceptorForFile&) =
      delete;

  ~WebBundleInterceptorForFile() override;

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
      bool* will_return_unsafe_redirect) override;

  void OnMetadataReady(
      const network::ResourceRequest& request,
      web_package::mojom::BundleMetadataParseErrorPtr metadata_error);

  void StartResponse(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  WebBundleDoneCallback done_callback_;
  const int frame_tree_node_id_;
  scoped_refptr<WebBundleReader> reader_;
  GURL primary_url_;
  std::unique_ptr<WebBundleURLLoaderFactory> url_loader_factory_;

  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebBundleInterceptorForFile> weak_factory_{this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_FILE_H_
