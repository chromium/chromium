// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A URLRequestJob class that pulls the net and http headers from disk.

#ifndef NET_TEST_URL_REQUEST_URL_REQUEST_MOCK_HTTP_JOB_H_
#define NET_TEST_URL_REQUEST_URL_REQUEST_MOCK_HTTP_JOB_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "net/test/url_request/url_request_test_job_backed_by_file.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace net {
class URLRequestInterceptor;
}

namespace net {

class URLRequestMockHTTPJob : public URLRequestTestJobBackedByFile {
 public:
  // Note that all file I/O is done using ThreadPool.
  URLRequestMockHTTPJob(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const base::FilePath& file_path);

  // URLRequestJob overrides.
  void Start() override;
  int64_t GetTotalReceivedBytes() const override;
  bool GetMimeType(std::string* mime_type) const override;
  bool GetCharset(std::string* charset) override;
  void GetResponseInfo(HttpResponseInfo* info) override;
  bool IsRedirectResponse(GURL* location,
                          int* http_status_code,
                          bool* insecure_scheme_was_upgraded) override;

  // URLRequestFileJob overridess.
  void OnReadComplete(net::IOBuffer* buffer, int result) override;

  // Adds the testing URLs to the URLRequestFilter, both under HTTP and HTTPS.
  static void AddUrlHandlers(const base::FilePath& base_path);

  // Given the path to a file relative to the path passed to AddUrlHandler(),
  // construct a mock URL.
  static GURL GetMockUrl(const std::string& path);
  static GURL GetMockHttpsUrl(const std::string& path);

  // Returns a URLRequestJobFactory::ProtocolHandler that serves
  // URLRequestMockHTTPJob's responding like an HTTP server. |base_path| is the
  // file path leading to the root of the directory to use as the root of the
  // HTTP server.
  static std::unique_ptr<URLRequestInterceptor> CreateInterceptor(
      const base::FilePath& base_path);

  // Returns a URLRequestJobFactory::ProtocolHandler that serves
  // URLRequestMockHTTPJob's responding like an HTTP server. It responds to all
  // requests with the contents of |file|.
  static std::unique_ptr<URLRequestInterceptor> CreateInterceptorForSingleFile(
      const base::FilePath& file);

 protected:
  ~URLRequestMockHTTPJob() override;

 private:
  void GetResponseInfoConst(HttpResponseInfo* info) const;
  void SetHeadersAndStart(const std::string& raw_headers);

  std::string raw_headers_;
  int64_t total_received_bytes_ = 0;

  base::WeakPtrFactory<URLRequestMockHTTPJob> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(URLRequestMockHTTPJob);
};

}  // namespace net

#endif  // NET_TEST_URL_REQUEST_URL_REQUEST_MOCK_HTTP_JOB_H_
