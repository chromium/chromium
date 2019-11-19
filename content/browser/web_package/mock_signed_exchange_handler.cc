// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/mock_signed_exchange_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"
#include "net/filter/source_stream.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace content {

MockSignedExchangeHandlerParams::MockSignedExchangeHandlerParams(
    const GURL& outer_url,
    SignedExchangeLoadResult result,
    net::Error error,
    const GURL& inner_url,
    const std::string& mime_type,
    std::vector<std::string> response_headers,
    base::Optional<net::SHA256HashValue> header_integrity,
    const base::Time& signature_expire_time)
    : outer_url(outer_url),
      result(result),
      error(error),
      inner_url(inner_url),
      mime_type(mime_type),
      response_headers(std::move(response_headers)),
      header_integrity(std::move(header_integrity)),
      signature_expire_time(signature_expire_time.is_null()
                                ? base::Time::Now() +
                                      base::TimeDelta::FromDays(1)
                                : signature_expire_time) {}

MockSignedExchangeHandlerParams::MockSignedExchangeHandlerParams(
    const MockSignedExchangeHandlerParams& other) = default;
MockSignedExchangeHandlerParams::~MockSignedExchangeHandlerParams() = default;

MockSignedExchangeHandler::MockSignedExchangeHandler(
    const MockSignedExchangeHandlerParams& params,
    std::unique_ptr<net::SourceStream> body,
    ExchangeHeadersCallback headers_callback)
    : header_integrity_(params.header_integrity),
      signature_expire_time_(params.signature_expire_time) {
  network::ResourceResponseHead head;
  if (params.error == net::OK) {
    head.headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    head.mime_type = params.mime_type;
    head.headers->AddHeader(
        base::StringPrintf("Content-type: %s", params.mime_type.c_str()));
    for (const auto& header : params.response_headers)
      head.headers->AddHeader(header);
    head.is_signed_exchange_inner_response = true;
    head.content_length = head.headers->GetContentLength();
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(headers_callback), params.result, params.error,
                     params.inner_url, head, std::move(body)));
}

base::Optional<net::SHA256HashValue>
MockSignedExchangeHandler::ComputeHeaderIntegrity() const {
  return header_integrity_;
}

base::Time MockSignedExchangeHandler::GetSignatureExpireTime() const {
  return signature_expire_time_;
}

MockSignedExchangeHandler::~MockSignedExchangeHandler() {}

MockSignedExchangeHandlerFactory::MockSignedExchangeHandlerFactory(
    std::vector<MockSignedExchangeHandlerParams> params_list)
    : params_list_(std::move(params_list)) {}

MockSignedExchangeHandlerFactory::~MockSignedExchangeHandlerFactory() = default;

std::unique_ptr<SignedExchangeHandler> MockSignedExchangeHandlerFactory::Create(
    const GURL& outer_url,
    std::unique_ptr<net::SourceStream> body,
    ExchangeHeadersCallback headers_callback,
    std::unique_ptr<SignedExchangeCertFetcherFactory> cert_fetcher_factory) {
  for (const auto& params : params_list_) {
    if (params.outer_url == outer_url) {
      return std::make_unique<MockSignedExchangeHandler>(
          params, std::move(body), std::move(headers_callback));
    }
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace content
