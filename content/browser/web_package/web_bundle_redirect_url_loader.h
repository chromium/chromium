// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_REDIRECT_URL_LOADER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_REDIRECT_URL_LOADER_H_

#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace content {

// A class to provide a network::mojom::URLLoader interface to redirect a
// request to the Web Bundle to the main resource url.
class WebBundleRedirectURLLoader final : public network::mojom::URLLoader {
 public:
  explicit WebBundleRedirectURLLoader(
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);
  WebBundleRedirectURLLoader(const WebBundleRedirectURLLoader&) = delete;
  WebBundleRedirectURLLoader& operator=(const WebBundleRedirectURLLoader&) =
      delete;

  ~WebBundleRedirectURLLoader() override;

  void OnReadyToRedirect(const network::ResourceRequest& resource_request,
                         const GURL& url);

 private:
  // mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Remote<network::mojom::URLLoaderClient> client_;
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_REDIRECT_URL_LOADER_H_
