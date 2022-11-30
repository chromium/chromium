// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/mock_cert_net_fetcher.h"

#include "net/cert/x509_util.h"

namespace net {

MockCertNetFetcher::MockCertNetFetcher() = default;
MockCertNetFetcher::~MockCertNetFetcher() = default;

MockCertNetFetcherRequest::MockCertNetFetcherRequest(Error error,
                                                     std::vector<uint8_t> bytes)
    : error_(error), bytes_(std::move(bytes)) {}
MockCertNetFetcherRequest::~MockCertNetFetcherRequest() = default;

// static
std::unique_ptr<CertNetFetcher::Request> MockCertNetFetcherRequest::Create(
    Error error) {
  return std::make_unique<MockCertNetFetcherRequest>(error,
                                                     std::vector<uint8_t>());
}

// static
std::unique_ptr<CertNetFetcher::Request> MockCertNetFetcherRequest::Create(
    std::vector<uint8_t> bytes) {
  return std::make_unique<MockCertNetFetcherRequest>(OK, std::move(bytes));
}

// static
std::unique_ptr<CertNetFetcher::Request> MockCertNetFetcherRequest::Create(
    const CRYPTO_BUFFER* buffer) {
  auto bytes = x509_util::CryptoBufferAsSpan(buffer);
  return Create(std::vector<uint8_t>(bytes.begin(), bytes.end()));
}

void MockCertNetFetcherRequest::WaitForResult(Error* error,
                                              std::vector<uint8_t>* bytes) {
  DCHECK(!did_consume_result_);
  *error = error_;
  *bytes = std::move(bytes_);
  did_consume_result_ = true;
}

}  // namespace net
