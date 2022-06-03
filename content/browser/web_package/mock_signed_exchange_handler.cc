// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/mock_signed_exchange_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache_entry.h"
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
    std::vector<std::pair<std::string, std::string>> response_headers,
    const net::SHA256HashValue& header_integrity,
    const base::Time& signature_expire_time)
    : outer_url(outer_url),
      result(result),
      error(error),
      inner_url(inner_url),
      mime_type(mime_type),
      response_headers(std::move(response_headers)),
      header_integrity(header_integrity),
      signature_expire_time(signature_expire_time.is_null()
                                ? base::Time::Now() + base::Days(1)
                                : signature_expire_time) {}

MockSignedExchangeHandlerParams::MockSignedExchangeHandlerParams(
    const MockSignedExchangeHandlerParams& other) = default;
MockSignedExchangeHandlerParams::~MockSignedExchangeHandlerParams() = default;

MockSignedExchangeHandler::MockSignedExchangeHandler(
    const MockSignedExchangeHandlerParams& params,
    std::unique_ptr<net::SourceStream> body,
    ExchangeHeadersCallback headers_callback)
    : header_integrity_(params.header_integrity),
      signature_expire_time_(params.signature_expire_time),
      cert_url_(params.outer_url.Resolve("mock_cert")),
      cert_server_ip_address_(net::IPAddress::IPv4Localhost()) {
  auto head = network::mojom::URLResponseHead::New();
  if (params.error == net::OK) {
    head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    head->mime_type = params.mime_type;
    head->headers->SetHeader("Content-type", params.mime_type);
    for (const auto& header : params.response_headers)
      head->headers->AddHeader(header.first, header.second);
    head->is_signed_exchange_inner_response = true;
    head->content_length = head->headers->GetContentLength();
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(headers_callback), params.result, params.error,
                     params.inner_url, std::move(head), std::move(body)));
}

bool MockSignedExchangeHandler::GetSignedExchangeInfoForPrefetchCache(
    PrefetchedSignedExchangeCacheEntry& entry) const {
  entry.SetHeaderIntegrity(
      std::make_unique<net::SHA256HashValue>(header_integrity_));
  entry.SetSignatureExpireTime(signature_expire_time_);
  entry.SetCertUrl(cert_url_);
  entry.SetCertServerIPAddress(cert_server_ip_address_);
  return true;
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
