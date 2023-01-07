// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SLOW_DOWNLOAD_HTTP_RESPONSE_H_
#define CONTENT_PUBLIC_TEST_SLOW_DOWNLOAD_HTTP_RESPONSE_H_

#include "base/strings/string_split.h"
#include "content/public/test/slow_http_response.h"

namespace content {

// A subclass of SlowHttpResponse that serves a download.
class SlowDownloadHttpResponse : public SlowHttpResponse {
 public:
  // Test URLs.
  static const char kSlowResponseHostName[];
  static const char kUnknownSizeUrl[];
  static const char kKnownSizeUrl[];

  // Helper to handle requests that should reply with SlowDownloadHttpResponse.
  // NOTE: This helper makes use of a global so only one such request/response
  // in flight at a time will be able to be finished by navigating to
  // `kFinishSlowResponseUrl`.
  static std::unique_ptr<net::test_server::HttpResponse>
  HandleSlowDownloadRequest(const net::test_server::HttpRequest& request);

  SlowDownloadHttpResponse(const std::string& url,
                           GotRequestCallback got_request);
  ~SlowDownloadHttpResponse() override;

  SlowDownloadHttpResponse(const SlowDownloadHttpResponse&) = delete;
  SlowDownloadHttpResponse& operator=(const SlowDownloadHttpResponse&) = delete;

  // SlowHttpResponse:
  base::StringPairs ResponseHeaders() override;

 private:
  std::string url_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SLOW_DOWNLOAD_HTTP_RESPONSE_H_
