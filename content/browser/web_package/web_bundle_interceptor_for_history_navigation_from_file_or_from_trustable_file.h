// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_FROM_FILE_OR_FROM_TRUSTABLE_FILE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_FROM_FILE_OR_FROM_TRUSTABLE_FILE_H_

#include "base/memory/weak_ptr.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "content/browser/web_package/web_bundle_interceptor_for_history_navigation.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"

class GURL;

namespace content {

class WebBundleReader;
class WebBundleSource;

// A class to inherit NavigationLoaderInterceptor for the history navigation to
// a Web Bundle file or a trustable Web Bundle file when the previous
// WebBundleReader was deleted.
// - MaybeCreateLoader() is called for the history navigation request. It
//   continues on CreateURLLoader() to create the loader for the main resource.
//   - If OnMetadataReady() has not been called yet:
//       Wait for OnMetadataReady() to be called.
//   - If OnMetadataReady() was called with an error:
//       Completes the request with ERR_INVALID_WEB_BUNDLE.
//   - If OnMetadataReady() was called whthout errors:
//       Creates the loader for the main resource.
class WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile final
    : public WebBundleInterceptorForHistoryNavigation {
 public:
  WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile(
      std::unique_ptr<WebBundleSource> source,
      const GURL& target_inner_url,
      WebBundleDoneCallback done_callback,
      int frame_tree_node_id);
  WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile(
      const WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile&) =
      delete;
  WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile&
  operator=(
      const WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile&) =
      delete;

  ~WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile()
      override;

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

  scoped_refptr<WebBundleReader> reader_;

  network::ResourceRequest pending_resource_request_;
  mojo::PendingReceiver<network::mojom::URLLoader> pending_receiver_;
  mojo::PendingRemote<network::mojom::URLLoaderClient> pending_client_;

  absl::optional<std::string> metadata_error_;

  base::WeakPtrFactory<
      WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile>
      weak_factory_{this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_FROM_FILE_OR_FROM_TRUSTABLE_FILE_H_
