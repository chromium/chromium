// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_HANDLE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_HANDLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;
class WebBundleHandleTracker;
class WebBundleNavigationInfo;
class WebBundleReader;
class WebBundleSource;
class WebBundleURLLoaderFactory;
class NavigationLoaderInterceptor;

// A class to provide interfaces to communicate with a Web Bundle for loading.
// Running on the UI thread.
class WebBundleHandle {
 public:
  static std::unique_ptr<WebBundleHandle> CreateForFile(int frame_tree_node_id);
  static std::unique_ptr<WebBundleHandle> CreateForTrustableFile(
      std::unique_ptr<WebBundleSource> source,
      int frame_tree_node_id);
  static std::unique_ptr<WebBundleHandle> CreateForNetwork(
      BrowserContext* browser_context,
      int frame_tree_node_id);
  static std::unique_ptr<WebBundleHandle> CreateForTrackedNavigation(
      scoped_refptr<WebBundleReader> reader,
      int frame_tree_node_id);
  static std::unique_ptr<WebBundleHandle> MaybeCreateForNavigationInfo(
      std::unique_ptr<WebBundleNavigationInfo> navigation_info,
      int frame_tree_node_id);

  WebBundleHandle(const WebBundleHandle&) = delete;
  WebBundleHandle& operator=(const WebBundleHandle&) = delete;

  ~WebBundleHandle();

  // Takes a NavigationLoaderInterceptor instance to handle the request for
  // a Web Bundle, to redirect to the entry URL of the Web Bundle, and to load
  // the main exchange from the Web Bundle.
  std::unique_ptr<NavigationLoaderInterceptor> TakeInterceptor();

  // Creates a URLLoaderFactory to load resources from the Web Bundle.
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory);

  // Creates a WebBundleHandleTracker to track navigations within the
  // Web Bundle file. Returns null if not yet succeeded to load the
  // exchanges file.
  std::unique_ptr<WebBundleHandleTracker> MaybeCreateTracker();

  // Checks if a valid Web Bundle is attached, opened, and ready for use.
  bool IsReadyForLoading();

  // The claimed URL inside Web Bundle file from which the document is loaded.
  // This URL is used for window.location and document.URL and relative path
  // computation in the document.
  const GURL& claimed_url() const { return claimed_url_; }

  const WebBundleNavigationInfo* navigation_info() const {
    return navigation_info_.get();
  }

 private:
  WebBundleHandle();

  void SetInterceptor(std::unique_ptr<NavigationLoaderInterceptor> interceptor);

  // Called when succeeded to load the Web Bundle file. |target_inner_url| is
  // the URL of the resource in the Web Bundle file, which are used for the
  // navigation.
  void OnWebBundleFileLoaded(
      const GURL& target_inner_url,
      std::unique_ptr<WebBundleURLLoaderFactory> url_loader_factory);

  std::unique_ptr<NavigationLoaderInterceptor> interceptor_;

  GURL claimed_url_;
  std::unique_ptr<WebBundleNavigationInfo> navigation_info_;

  std::unique_ptr<WebBundleURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<WebBundleHandle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_HANDLE_H_
