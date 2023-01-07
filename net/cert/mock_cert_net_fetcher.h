// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_MOCK_CERT_NET_FETCHER_H_
#define NET_CERT_MOCK_CERT_NET_FETCHER_H_

#include "net/cert/cert_net_fetcher.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace net {

// MockCertNetFetcher is an implementation of CertNetFetcher for testing.
class MockCertNetFetcher : public CertNetFetcher {
 public:
  MockCertNetFetcher();

  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD3(FetchCaIssuers,
               std::unique_ptr<Request>(const GURL& url,
                                        int timeout_milliseconds,
                                        int max_response_bytes));
  MOCK_METHOD3(FetchCrl,
               std::unique_ptr<Request>(const GURL& url,
                                        int timeout_milliseconds,
                                        int max_response_bytes));

  MOCK_METHOD3(FetchOcsp,
               std::unique_ptr<Request>(const GURL& url,
                                        int timeout_milliseconds,
                                        int max_response_bytes));

 protected:
  // Protected since CertNetFetcher is refcounted.
  ~MockCertNetFetcher() override;
};

// MockCertNetFetcherRequest gives back the indicated error and bytes.
class MockCertNetFetcherRequest : public CertNetFetcher::Request {
 public:
  MockCertNetFetcherRequest(Error error, std::vector<uint8_t> bytes);
  ~MockCertNetFetcherRequest() override;

  // Creates a CertNetFetcher::Request that completes with an error.
  static std::unique_ptr<CertNetFetcher::Request> Create(Error error);

  // Creates a CertNetFetcher::Request that completes with OK error code and
  // the specified bytes.
  static std::unique_ptr<CertNetFetcher::Request> Create(
      std::vector<uint8_t> bytes);

  // Creates a CertNetFetcher::Request that completes with OK error code and
  // the specified CRYPTO_BUFFER data.
  static std::unique_ptr<CertNetFetcher::Request> Create(
      const CRYPTO_BUFFER* buffer);

  void WaitForResult(Error* error, std::vector<uint8_t>* bytes) override;

 private:
  Error error_;
  std::vector<uint8_t> bytes_;
  bool did_consume_result_ = false;
};

}  // namespace net

#endif  // NET_CERT_MOCK_CERT_NET_FETCHER_H_
