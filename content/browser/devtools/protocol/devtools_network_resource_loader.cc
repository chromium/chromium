// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_network_resource_loader.h"

#include <cstddef>
#include <string_view>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {
namespace protocol {

DevToolsNetworkResourceLoader::DevToolsNetworkResourceLoader(
    network::ResourceRequest resource_request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory,
    CompletionCallback completion_callback)
    : resource_request_(std::move(resource_request)),
      traffic_annotation_(traffic_annotation),
      url_loader_factory_(std::move(url_loader_factory)),
      completion_callback_(std::move(completion_callback)) {
  DownloadAsStream();
}

DevToolsNetworkResourceLoader::~DevToolsNetworkResourceLoader() = default;

// We can trust the |origin| parameter here, as it is the last committed origin
// of a RenderFrameHost identified by a DevTools frame token. Note that there
// is a potential race condition when DevTools sends a request while the frame
// already navigates away. This is difficult to fix before the
// RenderDocumentHost refactoring is done.
// static
std::unique_ptr<DevToolsNetworkResourceLoader>
DevToolsNetworkResourceLoader::Create(
    mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory,
    GURL gurl,
    const url::Origin& origin,
    net::SiteForCookies site_for_cookies,
    Caching caching,
    Credentials include_credentials,
    CompletionCallback completion_callback) {
  network::ResourceRequest resource_request;
  resource_request.url = std::move(gurl);
  resource_request.request_initiator = origin;
  resource_request.site_for_cookies = site_for_cookies;
  if (caching == Caching::kBypass) {
    resource_request.load_flags |= net::LOAD_BYPASS_CACHE;
  }
  resource_request.mode = network::mojom::RequestMode::kNoCors;
  resource_request.credentials_mode =
      include_credentials == Credentials::kInclude
          ? network::mojom::CredentialsMode::kInclude
          : network::mojom::CredentialsMode::kSameOrigin;
  resource_request.redirect_mode = network::mojom::RedirectMode::kFollow;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("devtools_cdp_network_resource", R"(
        semantics {
          sender: "Developer Tools via CDP"
          description:
            "When user opens Developer Tools, the browser may fetch additional "
            "resources from the network to enrich the debugging experience "
            "(e.g. source map resources)."
          trigger: "User opens Developer Tools to debug a web page."
          data: "Any resources requested by Developer Tools."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "It's not possible to disable this feature from settings."
          chrome_policy {
            DeveloperToolsAvailability {
              policy_options {mode: MANDATORY}
              DeveloperToolsAvailability: 2
            }
          }
        })");

  return base::WrapUnique(new DevToolsNetworkResourceLoader(
      std::move(resource_request), traffic_annotation,
      std::move(url_loader_factory), std::move(completion_callback)));
}

void DevToolsNetworkResourceLoader::OnRetry(base::OnceClosure start_retry) {
  NOTREACHED_IN_MIGRATION();
}

void DevToolsNetworkResourceLoader::DownloadAsStream() {
  content_.erase();
  loader_ = network::SimpleURLLoader::Create(
      std::make_unique<network::ResourceRequest>(resource_request_),
      traffic_annotation_);
  loader_->DownloadAsStream(url_loader_factory_.get(), this);
}

void DevToolsNetworkResourceLoader::OnDataReceived(std::string_view chunk,
                                                   base::OnceClosure resume) {
  content_.append(chunk);
  std::move(resume).Run();
}

void DevToolsNetworkResourceLoader::OnComplete(bool success) {
  const network::mojom::URLResponseHead* info = loader_->ResponseInfo();
  const net::HttpResponseHeaders* response_headers = nullptr;
  std::string mime_type;
  if (info && info->headers) {
    response_headers = info->headers.get();
  }

  std::move(completion_callback_)
      .Run(this, response_headers, success, loader_->NetError(),
           std::move(content_));
}

}  // namespace protocol
}  // namespace content
