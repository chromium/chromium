// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;
class BundledExchangesHandleTracker;
class BundledExchangesNavigationInfo;
class BundledExchangesReader;
class BundledExchangesSource;
class BundledExchangesURLLoaderFactory;
class NavigationLoaderInterceptor;

// A class to provide interfaces to communicate with a BundledExchanges for
// loading. Running on the UI thread.
class BundledExchangesHandle {
 public:
  static std::unique_ptr<BundledExchangesHandle> CreateForFile(
      int frame_tree_node_id);
  static std::unique_ptr<BundledExchangesHandle> CreateForTrustableFile(
      std::unique_ptr<BundledExchangesSource> source,
      int frame_tree_node_id);
  static std::unique_ptr<BundledExchangesHandle> CreateForNetwork(
      BrowserContext* browser_context,
      int frame_tree_node_id);
  static std::unique_ptr<BundledExchangesHandle> CreateForTrackedNavigation(
      scoped_refptr<BundledExchangesReader> reader,
      int frame_tree_node_id);
  static std::unique_ptr<BundledExchangesHandle> MaybeCreateForNavigationInfo(
      std::unique_ptr<BundledExchangesNavigationInfo> navigation_info,
      int frame_tree_node_id);

  ~BundledExchangesHandle();

  // Takes a NavigationLoaderInterceptor instance to handle the request for
  // a BundledExchanges, to redirect to the entry URL of the BundledExchanges,
  // and to load the main exchange from the BundledExchanges.
  std::unique_ptr<NavigationLoaderInterceptor> TakeInterceptor();

  // Creates a URLLoaderFactory to load resources from the BundledExchanges.
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory);

  // Creates a BundledExchangesHandleTracker to track navigations within the
  // bundled exchanges file. Returns null if not yet succeeded to load the
  // exchanges file.
  std::unique_ptr<BundledExchangesHandleTracker> MaybeCreateTracker();

  // Checks if a valid BundledExchanges is attached, opened, and ready for use.
  bool IsReadyForLoading();

  // The base URL which will be set for the document to support relative path
  // subresource loading in unsigned bundled exchanges file.
  const GURL& base_url_override() const { return base_url_override_; }

  const BundledExchangesNavigationInfo* navigation_info() const {
    return navigation_info_.get();
  }

 private:
  BundledExchangesHandle();

  void SetInterceptor(std::unique_ptr<NavigationLoaderInterceptor> interceptor);

  // Called when succeeded to load the bundled exchanges file.
  // |target_inner_url| is the URL of the resource in the bundled exchanges
  // file, which are used for the navigation.
  void OnBundledExchangesFileLoaded(
      const GURL& target_inner_url,
      std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory);

  std::unique_ptr<NavigationLoaderInterceptor> interceptor_;

  GURL base_url_override_;
  std::unique_ptr<BundledExchangesNavigationInfo> navigation_info_;

  std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<BundledExchangesHandle> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_H_
