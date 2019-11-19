// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/chromium_url_request.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace remoting {

ChromiumUrlRequest::ChromiumUrlRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    UrlRequest::Type type,
    const std::string& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : traffic_annotation_(traffic_annotation) {
  url_loader_factory_ = url_loader_factory;

  std::string request_type = "GET";
  switch (type) {
    case Type::GET:
      break;
    case Type::POST:
      request_type = "POST";
      break;
  }
  resource_request_ = std::make_unique<network::ResourceRequest>();
  resource_request_->url = GURL(url);
  resource_request_->method = request_type;
  resource_request_->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request_->referrer = GURL("https://chrome.google.com/remotedesktop");
}

ChromiumUrlRequest::~ChromiumUrlRequest() = default;

void ChromiumUrlRequest::AddHeader(const std::string& value) {
  resource_request_->headers.AddHeaderFromString(value);
}

void ChromiumUrlRequest::SetPostData(const std::string& content_type,
                                     const std::string& data) {
  post_data_content_type_ = content_type;
  post_data_ = data;
}

void ChromiumUrlRequest::Start(const OnResultCallback& on_result_callback) {
  DCHECK(!on_result_callback.is_null());
  DCHECK(on_result_callback_.is_null());

  on_result_callback_ = on_result_callback;

  std::string method = resource_request_->method;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request_),
                                                 traffic_annotation_);
  if (method == "POST")
    url_loader_->AttachStringForUpload(post_data_, post_data_content_type_);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ChromiumUrlRequest::OnURLLoadComplete,
                     base::Unretained(this)));
}

void ChromiumUrlRequest::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  Result result;
  result.success = !!response_body;
  if (result.success) {
    result.status = -1;
    result.response_body = std::move(*response_body);
  }

  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    result.status = url_loader_->ResponseInfo()->headers->response_code();
  }

  DCHECK(!on_result_callback_.is_null());
  std::move(on_result_callback_).Run(result);
}

ChromiumUrlRequestFactory::ChromiumUrlRequestFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}
ChromiumUrlRequestFactory::~ChromiumUrlRequestFactory() = default;

std::unique_ptr<UrlRequest> ChromiumUrlRequestFactory::CreateUrlRequest(
    UrlRequest::Type type,
    const std::string& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  return std::make_unique<ChromiumUrlRequest>(url_loader_factory_, type, url,
                                              traffic_annotation);
}

}  // namespace remoting
