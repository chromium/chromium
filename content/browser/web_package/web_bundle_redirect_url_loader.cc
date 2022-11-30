// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_redirect_url_loader.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

WebBundleRedirectURLLoader::WebBundleRedirectURLLoader(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
    : client_(std::move(client)) {}

WebBundleRedirectURLLoader::~WebBundleRedirectURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebBundleRedirectURLLoader::OnReadyToRedirect(
    const network::ResourceRequest& resource_request,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_.is_connected());
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->encoded_data_length = 0;
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          base::StringPrintf("HTTP/1.1 %d %s\r\n", 303, "See Other")));

  net::RedirectInfo redirect_info = net::RedirectInfo::ComputeRedirectInfo(
      "GET", resource_request.url, resource_request.site_for_cookies,
      resource_request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL,
      resource_request.referrer_policy, resource_request.referrer.spec(), 303,
      url, /*referrer_policy_header=*/absl::nullopt,
      /*insecure_scheme_was_upgraded=*/false, /*copy_fragment=*/true,
      /*is_signed_exchange_fallback_redirect=*/false);
  client_->OnReceiveRedirect(redirect_info, std::move(response_head));
}

}  // namespace content
