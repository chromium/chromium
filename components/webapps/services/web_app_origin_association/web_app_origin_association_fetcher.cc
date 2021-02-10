// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace {

constexpr size_t kMaxJsonSize = 1000000;  // 1MB max

int g_max_retry = 3;

network::SimpleURLLoader::RetryMode g_retry_mode =
    network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE;

constexpr net::NetworkTrafficAnnotationTag
    web_app_origin_association_traffic_annotation =
        net::DefineNetworkTrafficAnnotation(
            "web_app_origin_association_download",
            R"(
      semantics {
          sender: "Web App Origin Association Fetcher"
          description:
            "PWAs can specify URL Handlers in the Manifest. To verify the "
            "handlers, we download the corresponding web app origin "
            "association files."
          trigger:
            "A PWA that has URL Handlers declared in the Manifest is "
            "installed, updated, or when DevTools displays URL Handler "
            "information to users."
          data:
            "Nothing."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
          "There is no setting to disable PWA installation."
          policy_exception_justification:
            "Not implemented, "
            "considered not necessary as no user data is sent."
    })");

constexpr char association_file_name[] =
    ".well-known/web-app-origin-association";

std::unique_ptr<network::SimpleURLLoader> CreateRequester(const GURL& url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request),
      web_app_origin_association_traffic_annotation);
  url_loader->SetRetryOptions(g_max_retry, g_retry_mode);
  url_loader->SetURLLoaderFactoryOptions(
      network::mojom::kURLLoadOptionBlockAllCookies);
  return url_loader;
}
}  // namespace

namespace webapps {

WebAppOriginAssociationFetcher::WebAppOriginAssociationFetcher() = default;

WebAppOriginAssociationFetcher::~WebAppOriginAssociationFetcher() = default;

void WebAppOriginAssociationFetcher::SetRetryOptionsForTest(
    int max_retry,
    network::SimpleURLLoader::RetryMode retry_mode) {
  g_max_retry = max_retry;
  g_retry_mode = retry_mode;
}

void WebAppOriginAssociationFetcher::FetchWebAppOriginAssociationFile(
    const apps::UrlHandlerInfo& url_handler,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    FetchFileCallback callback) {
  const GURL resource_url =
      url_handler.origin.GetURL().Resolve(association_file_name);
  if (!resource_url.is_valid() || resource_url.is_empty()) {
    // Do not proceed if |resource_url| is not valid.
    OnResponse(std::move(callback), nullptr);
    return;
  }

  SendRequest(resource_url, std::move(shared_url_loader_factory),
              std::move(callback));
}

void WebAppOriginAssociationFetcher::SendRequest(
    const GURL& url,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    FetchFileCallback callback) {
  url_loader_ = CreateRequester(url);
  url_loader_->DownloadToString(
      shared_url_loader_factory.get(),
      base::BindOnce(&WebAppOriginAssociationFetcher::OnResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      kMaxJsonSize);
}

void WebAppOriginAssociationFetcher::OnResponse(
    FetchFileCallback callback,
    std::unique_ptr<std::string> response_body) {
  std::move(callback).Run(std::move(response_body));
}

}  // namespace webapps
