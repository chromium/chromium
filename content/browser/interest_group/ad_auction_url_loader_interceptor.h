// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_URL_LOADER_INTERCEPTOR_H_

#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "content/browser/loader/subresource_proxying_url_loader.h"
#include "content/public/browser/weak_document_ptr.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// A loader interceptor for handling an ad auction subresource request,
// including fetch(<url>, {adAuctionHeaders: true}).
//
// This loader interceptor works as follows:
//   1. Before making a network request (i.e. WillStartRequest()), if the
//      request is eligible for ad auction headers, add the
//      `Sec-Ad-Auction-Fetch: ?1` header.
//   2. If any redirect is encountered, skip handling the response; otherwise,
//      for the response (i.e. OnReceiveResponse()), if the previous request was
//      eligible for ad auction headers, and if the response header contains the
//      auction result, signals, or additional bids, associate them with the
//      top-level page.
class CONTENT_EXPORT AdAuctionURLLoaderInterceptor
    : public SubresourceProxyingURLLoader::Interceptor {
 public:
  AdAuctionURLLoaderInterceptor(
      WeakDocumentPtr document,
      const network::ResourceRequest& resource_request);

  AdAuctionURLLoaderInterceptor(const AdAuctionURLLoaderInterceptor&) = delete;
  AdAuctionURLLoaderInterceptor& operator=(
      const AdAuctionURLLoaderInterceptor&) = delete;

  ~AdAuctionURLLoaderInterceptor() override;

  // SubresourceProxyingURLLoader::Interceptor
  void WillStartRequest(net::HttpRequestHeaders& headers) override;
  void WillFollowRedirect(const std::optional<GURL>& new_url,
                          std::vector<std::string>& removed_headers,
                          net::HttpRequestHeaders& modified_headers) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr& head) override;
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr& head) override;

 private:
  // Upon NavigationRequest::DidCommitNavigation(), `document_` will be set to
  // the document that this request is associated with. It will become null
  // whenever the document navigates away.
  WeakDocumentPtr document_;

  // The initial request state. This will be used to derive the opt-in
  // permissions policy features.
  const raw_ref<const network::ResourceRequest> resource_request_;

  // The request URL's origin.
  url::Origin request_origin_;

  // Whether the ongoing request or redirect is eligible for ad auction headers.
  // Set to the desired state when a request/redirect is made. Reset to false
  // when the corresponding response is received.
  bool ad_auction_headers_eligible_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_URL_LOADER_INTERCEPTOR_H_
