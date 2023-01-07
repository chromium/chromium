// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_TRUSTABLE_FILE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_TRUSTABLE_FILE_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/web_package/web_bundle_url_loader_factory.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

class WebBundleReader;
class WebBundleSource;

// A class to inherit NavigationLoaderInterceptor for a navigation to a
// trustable Web Bundle file (eg: "file:///tmp/a.wbn").
// The overridden methods of NavigationLoaderInterceptor are called in the
// following sequence:
// [1] MaybeCreateLoader() is called for the navigation request to the trustable
//     Web Bundle file. It continues on CreateURLLoader() to create the loader
//     for the main resource.
//     - If OnMetadataReady() has not been called yet:
//         Wait for OnMetadataReady() to be called.
//     - If OnMetadataReady() was called with an error:
//         Completes the request with ERR_INVALID_WEB_BUNDLE.
//     - If OnMetadataReady() was called whthout errors:
//         A redirect loader is created to redirect the navigation request to
//         the Bundle's primary URL ("https://example.com/a.html").
// [2] MaybeCreateLoader() is called again for the redirect. It continues on
//     CreateURLLoader() to create the loader for the main resource.
class WebBundleInterceptorForTrustableFile final
    : public NavigationLoaderInterceptor {
 public:
  WebBundleInterceptorForTrustableFile(std::unique_ptr<WebBundleSource> source,
                                       WebBundleDoneCallback done_callback,
                                       int frame_tree_node_id);
  WebBundleInterceptorForTrustableFile(
      const WebBundleInterceptorForTrustableFile&) = delete;
  WebBundleInterceptorForTrustableFile& operator=(
      const WebBundleInterceptorForTrustableFile&) = delete;

  ~WebBundleInterceptorForTrustableFile() override;

 private:
  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override;

  void CreateURLLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  void OnMetadataReady(web_package::mojom::BundleMetadataParseErrorPtr error);

  std::unique_ptr<WebBundleSource> source_;
  scoped_refptr<WebBundleReader> reader_;
  WebBundleDoneCallback done_callback_;
  const int frame_tree_node_id_;

  network::ResourceRequest pending_resource_request_;
  mojo::PendingReceiver<network::mojom::URLLoader> pending_receiver_;
  mojo::PendingRemote<network::mojom::URLLoaderClient> pending_client_;

  std::unique_ptr<WebBundleURLLoaderFactory> url_loader_factory_;

  GURL primary_url_;
  absl::optional<std::string> metadata_error_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebBundleInterceptorForTrustableFile> weak_factory_{
      this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_TRUSTABLE_FILE_H_
