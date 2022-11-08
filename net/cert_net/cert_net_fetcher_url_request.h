// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_NET_CERT_NET_FETCHER_URL_REQUEST_H_
#define NET_CERT_NET_CERT_NET_FETCHER_URL_REQUEST_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"
#include "net/cert/cert_net_fetcher.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {

class URLRequestContext;

// A CertNetFetcher that issues requests through the provided
// URLRequestContext. The URLRequestContext must stay valid until the returned
// CertNetFetcher's Shutdown method is called. The CertNetFetcher is to be
// created and shutdown on the network thread. Its Fetch methods are to be used
// on a *different* thread, since it gives a blocking interface to URL fetching.
class NET_EXPORT CertNetFetcherURLRequest : public CertNetFetcher {
 public:
  class AsyncCertNetFetcherURLRequest;
  class RequestCore;
  struct RequestParams;

  // Creates the CertNetFetcherURLRequest. SetURLRequestContext must be called
  // before the fetcher can be used.
  CertNetFetcherURLRequest();

  // Set the URLRequestContext this fetcher should use.
  // |context_| must stay valid until Shutdown() is called.
  void SetURLRequestContext(URLRequestContext* context);

  // Returns the default timeout value. Intended for test use only.
  static base::TimeDelta GetDefaultTimeoutForTesting();

  // CertNetFetcher impl:
  void Shutdown() override;
  std::unique_ptr<Request> FetchCaIssuers(const GURL& url,
                                          int timeout_milliseconds,
                                          int max_response_bytes) override;
  std::unique_ptr<Request> FetchCrl(const GURL& url,
                                    int timeout_milliseconds,
                                    int max_response_bytes) override;
  [[nodiscard]] std::unique_ptr<Request> FetchOcsp(
      const GURL& url,
      int timeout_milliseconds,
      int max_response_bytes) override;

 private:
  ~CertNetFetcherURLRequest() override;

  void DoFetchOnNetworkSequence(std::unique_ptr<RequestParams> request_params,
                                scoped_refptr<RequestCore> request);

  std::unique_ptr<Request> DoFetch(
      std::unique_ptr<RequestParams> request_params);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Not owned. |context_| must stay valid until Shutdown() is called.
  raw_ptr<URLRequestContext> context_ = nullptr;
  std::unique_ptr<AsyncCertNetFetcherURLRequest> impl_;
};

}  // namespace net

#endif  // NET_CERT_NET_CERT_NET_FETCHER_URL_REQUEST_H_
