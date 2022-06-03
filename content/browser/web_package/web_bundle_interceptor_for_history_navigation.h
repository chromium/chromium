// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_H_

#include "base/sequence_checker.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace content {

class WebBundleReader;
class WebBundleURLLoaderFactory;

// A base class of the following NavigationLoaderInterceptor classes. They are
// used to intercept history navigation to a Web Bundle.
//   - WebBundleInterceptorForHistoryNavigationWithExistingReader:
//       This class is used when the WebBundleReader for the Web Bundle is still
//       alive.
//   - WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile:
//       This class is used when the WebBundleReader was already deleted, and
//       the Web Bundle is a file or a trustable file.
//   - WebBundleInterceptorForHistoryNavigationFromNetwork:
//       This class is used when the WebBundleReader was already deleted, and
//       the Web Bundle was from network.
class WebBundleInterceptorForHistoryNavigation
    : public NavigationLoaderInterceptor {
 public:
  WebBundleInterceptorForHistoryNavigation(
      const WebBundleInterceptorForHistoryNavigation&) = delete;
  WebBundleInterceptorForHistoryNavigation& operator=(
      const WebBundleInterceptorForHistoryNavigation&) = delete;

 protected:
  WebBundleInterceptorForHistoryNavigation(const GURL& target_inner_url,
                                           WebBundleDoneCallback done_callback,
                                           int frame_tree_node_id);

  ~WebBundleInterceptorForHistoryNavigation() override;

  void CreateWebBundleURLLoaderFactory(scoped_refptr<WebBundleReader> reader);

  void CreateLoaderAndStartAndDone(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  const GURL target_inner_url_;
  const int frame_tree_node_id_;
  std::unique_ptr<WebBundleURLLoaderFactory> url_loader_factory_;
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  WebBundleDoneCallback done_callback_;
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_INTERCEPTOR_FOR_HISTORY_NAVIGATION_H_
