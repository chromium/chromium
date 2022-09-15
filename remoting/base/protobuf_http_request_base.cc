// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_request_base.h"

#include "net/base/net_errors.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/scoped_protobuf_http_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace remoting {

ProtobufHttpRequestBase::ProtobufHttpRequestBase(
    std::unique_ptr<ProtobufHttpRequestConfig> config)
    : config_(std::move(config)) {
  config_->Validate();
}

ProtobufHttpRequestBase::~ProtobufHttpRequestBase() {
#if DCHECK_IS_ON()
  DCHECK(request_deadline_.is_null() ||
         request_deadline_ >= base::TimeTicks::Now())
      << "The request must have been deleted before the deadline.";
#endif  // DCHECK_IS_ON()
}

std::unique_ptr<ScopedProtobufHttpRequest>
ProtobufHttpRequestBase::CreateScopedRequest() {
  return std::make_unique<ScopedProtobufHttpRequest>(base::BindOnce(
      &ProtobufHttpRequestBase::Invalidate, weak_factory_.GetWeakPtr()));
}

ProtobufHttpStatus ProtobufHttpRequestBase::GetUrlLoaderStatus() const {
  net::Error net_error = static_cast<net::Error>(url_loader_->NetError());
  if (net_error != net::Error::OK &&
      net_error != net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    return ProtobufHttpStatus(net_error);
  }
  // Depending on the configuration, url_loader_->NetError() can be OK even if
  // the error code is 4xx or 5xx.
  if (!url_loader_->ResponseInfo() || !url_loader_->ResponseInfo()->headers ||
      url_loader_->ResponseInfo()->headers->response_code() <= 0) {
    return ProtobufHttpStatus(
        ProtobufHttpStatus::Code::INTERNAL,
        "Failed to get HTTP status from the response header.");
  }
  return ProtobufHttpStatus(static_cast<net::HttpStatusCode>(
      url_loader_->ResponseInfo()->headers->response_code()));
}

void ProtobufHttpRequestBase::StartRequest(
    network::mojom::URLLoaderFactory* loader_factory,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    base::OnceClosure invalidator) {
  DCHECK(!url_loader_);
  DCHECK(!invalidator_);

  url_loader_ = std::move(url_loader);
  invalidator_ = std::move(invalidator);
  StartRequestInternal(loader_factory);

#if DCHECK_IS_ON()
  base::TimeDelta timeout_duration = GetRequestTimeoutDuration();
  if (!timeout_duration.is_zero()) {
    // Add a 500ms fuzz to account for task dispatching delay and other stuff.
    request_deadline_ =
        base::TimeTicks::Now() + timeout_duration + base::Milliseconds(500);
  }
#endif  // DCHECK_IS_ON()
}

void ProtobufHttpRequestBase::Invalidate() {
  // This is not necessarily true if the request has never been added to a
  // client.
  if (invalidator_) {
    std::move(invalidator_).Run();
  }
}

}  // namespace remoting
