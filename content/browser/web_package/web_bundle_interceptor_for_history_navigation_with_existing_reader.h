// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_WITH_EXISTING_READER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_WITH_EXISTING_READER_H_

#include "base/memory/weak_ptr.h"
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

// A class to inherit NavigationLoaderInterceptor for the history navigation to
// a Web Bundle when the previous WebBundleReader is still alive.
// - MaybeCreateLoader() is called for the history navigation request. It
//   continues on CreateURLLoader() to create the loader for the main resource.
class WebBundleInterceptorForHistoryNavigationWithExistingReader final
    : public WebBundleInterceptorForHistoryNavigation {
 public:
  WebBundleInterceptorForHistoryNavigationWithExistingReader(
      scoped_refptr<WebBundleReader> reader,
      const GURL& target_inner_url,
      WebBundleDoneCallback done_callback,
      int frame_tree_node_id);
  WebBundleInterceptorForHistoryNavigationWithExistingReader(
      const WebBundleInterceptorForHistoryNavigationWithExistingReader&) =
      delete;
  WebBundleInterceptorForHistoryNavigationWithExistingReader& operator=(
      const WebBundleInterceptorForHistoryNavigationWithExistingReader&) =
      delete;

  ~WebBundleInterceptorForHistoryNavigationWithExistingReader() override;

 private:
  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override;

  void CreateURLLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  base::WeakPtrFactory<
      WebBundleInterceptorForHistoryNavigationWithExistingReader>
      weak_factory_{this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_WITH_EXISTING_READER_H_
