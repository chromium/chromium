// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/url_fetcher_block_adapter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

URLFetcherBlockAdapter::URLFetcherBlockAdapter(
    const GURL& url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    web::URLFetcherBlockAdapterCompletion completion_handler)
    : url_(url),
      url_loader_factory_(std::move(url_loader_factory)),
      completion_handler_([completion_handler copy]) {}

URLFetcherBlockAdapter::~URLFetcherBlockAdapter() {
}

void URLFetcherBlockAdapter::Start() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 NO_TRAFFIC_ANNOTATION_YET);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&URLFetcherBlockAdapter::OnURLLoadComplete,
                     base::Unretained(this)));
}

void URLFetcherBlockAdapter::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  std::string response;
  if (!response_body) {
    DLOG(WARNING) << "String for resource URL not found "
                  << url_loader_->GetFinalURL();
  } else {
    response = *response_body;
  }

  url_loader_.reset();

  NSData* data =
      [NSData dataWithBytes:response.c_str() length:response.length()];
  completion_handler_.get()(data, this);
}

}  // namespace web
