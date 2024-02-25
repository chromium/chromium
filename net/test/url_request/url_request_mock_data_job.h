// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_URL_REQUEST_URL_REQUEST_MOCK_DATA_JOB_H_
#define NET_TEST_URL_REQUEST_URL_REQUEST_MOCK_DATA_JOB_H_

#include <stddef.h>

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_job.h"

namespace net {

class URLRequest;

// Mock data job, which synchronously returns data repeated multiple times. If
// |request_client_certificate| is true, then this job will request client
// certificate before proceeding.
class URLRequestMockDataJob : public URLRequestJob {
 public:
  URLRequestMockDataJob(URLRequest* request,
                        const std::string& data,
                        int data_repeat_count,
                        bool request_client_certificate);
  ~URLRequestMockDataJob() override;

  void Start() override;
  int ReadRawData(IOBuffer* buf, int buf_size) override;
  void GetResponseInfo(HttpResponseInfo* info) override;
  void ContinueWithCertificate(
      scoped_refptr<X509Certificate> client_cert,
      scoped_refptr<SSLPrivateKey> client_private_key) override;

  // Adds the testing URLs to the URLRequestFilter.
  static void AddUrlHandler();
  static void AddUrlHandlerForHostname(const std::string& hostname);

  // Given data and repeat cound, constructs a mock URL that will return that
  // data repeated |repeat_count| times when started.  |data| must be safe for
  // URL.
  static GURL GetMockHttpUrl(const std::string& data, int repeat_count);
  static GURL GetMockHttpsUrl(const std::string& data, int repeat_count);

  // Constructs Mock URL which will request client certificate and return the
  // word "data" as the response.
  static GURL GetMockUrlForClientCertificateRequest();

  // URLRequestFailedJob must be added as a handler for |hostname| for
  // the returned URL to return |net_error|.
  static GURL GetMockHttpUrlForHostname(const std::string& hostname,
                                        const std::string& data,
                                        int repeat_count);
  static GURL GetMockHttpsUrlForHostname(const std::string& hostname,
                                         const std::string& data,
                                         int repeat_count);

  // Overrides response headers in the mocked response.
  void OverrideResponseHeaders(const std::string& headers);

 private:
  void GetResponseInfoConst(HttpResponseInfo* info) const;

  void StartAsync();

  std::optional<std::string> headers_;
  std::string data_;
  size_t data_offset_ = 0;
  bool request_client_certificate_;
  base::WeakPtrFactory<URLRequestMockDataJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_TEST_URL_REQUEST_URL_REQUEST_MOCK_DATA_JOB_H_
