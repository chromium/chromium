// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_TRACKED_NAVIGATION_FROM_TRUSTABLE_FILE_OR_FROM_NETWORK_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_TRACKED_NAVIGATION_FROM_TRUSTABLE_FILE_OR_FROM_NETWORK_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/web_package/web_bundle_url_loader_factory.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

class WebBundleReader;

// A class to inherit NavigationLoaderInterceptor for a navigation within a
// trustable Web Bundle file or within a Web Bundle from network.
// For example:
//   A user opened a trustable Web Bundle file "file:///tmp/a.wbn", and
//   InterceptorForTrustableFile redirected to "https://example.com/a.html" and
//   "a.html" in "a.wbn" was loaded. Or, a user opened a Web Bundle
//   "https://example.com/a.wbn", and InterceptorForNetwork redirected to
//   "https://example.com/a.html" and "a.html" in "a.wbn" was loaded. And the
//   user clicked a link to "https://example.com/b.html" from "a.html".
//
// In this case, this interceptor intecepts the navigation request to "b.html",
// and creates a URLLoader using the WebBundleURLLoaderFactory to load
// the response of "b.html" in "a.wbn".
class WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork
    final : public NavigationLoaderInterceptor {
 public:
  WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork(
      scoped_refptr<WebBundleReader> reader,
      WebBundleDoneCallback done_callback,
      int frame_tree_node_id);
  WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork(
      const WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork&) =
      delete;
  WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork&
  operator=(
      const WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork&) =
      delete;

  ~WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork()
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

  std::unique_ptr<WebBundleURLLoaderFactory> url_loader_factory_;
  WebBundleDoneCallback done_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<
      WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork>
      weak_factory_{this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_TRACKED_NAVIGATION_FROM_TRUSTABLE_FILE_OR_FROM_NETWORK_H_
