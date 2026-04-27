// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/url_fetcher_block_adapter.h"

#import <optional>
#import <string>

#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "net/http/http_response_headers.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "services/network/public/mojom/url_response_head.mojom.h"

namespace web {

URLFetcherBlockAdapter::URLFetcherBlockAdapter(
    const GURL& url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    web::URLFetcherBlockAdapterCompletion completion_handler)
    : url_(url),
      url_loader_factory_(std::move(url_loader_factory)),
      completion_handler_(completion_handler) {}

URLFetcherBlockAdapter::~URLFetcherBlockAdapter() {}

void URLFetcherBlockAdapter::Start() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 NO_TRAFFIC_ANNOTATION_YET);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&URLFetcherBlockAdapter::OnURLLoadComplete,
                     base::Unretained(this)));
}

void URLFetcherBlockAdapter::OnURLLoadComplete(
    std::optional<std::string> response_body) {
  std::string response;
  NSDictionary* headers;
  if (!response_body) {
    DLOG(WARNING) << "String for resource URL not found "
                  << url_loader_->GetFinalURL();
  } else {
    response = std::move(response_body).value();

    const network::mojom::URLResponseHead* response_head =
        url_loader_->ResponseInfo();
    NSMutableDictionary* headers_dict = [NSMutableDictionary dictionary];
    if (response_head && response_head->headers) {
      size_t iter = 0;
      std::string name, value;
      while (
          response_head->headers->EnumerateHeaderLines(&iter, &name, &value)) {
        headers_dict[base::SysUTF8ToNSString(name)] =
            base::SysUTF8ToNSString(value);
      }
      headers = headers_dict;
    }
  }

  url_loader_.reset();

  NSData* data = [NSData dataWithBytes:response.c_str()
                                length:response.length()];
  completion_handler_(data, headers, this);
}

}  // namespace web
