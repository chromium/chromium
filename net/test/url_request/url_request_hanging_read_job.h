// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_URL_REQUEST_URL_REQUEST_HANGING_READ_JOB_H_
#define NET_TEST_URL_REQUEST_URL_REQUEST_HANGING_READ_JOB_H_

#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_job.h"

namespace net {

class URLRequest;

// A URLRequestJob that hangs when try to read response body.
class URLRequestHangingReadJob : public URLRequestJob {
 public:
  explicit URLRequestHangingReadJob(URLRequest* request);

  URLRequestHangingReadJob(const URLRequestHangingReadJob&) = delete;
  URLRequestHangingReadJob& operator=(const URLRequestHangingReadJob&) = delete;

  ~URLRequestHangingReadJob() override;

  void Start() override;
  int ReadRawData(IOBuffer* buf, int buf_size) override;
  void GetResponseInfo(HttpResponseInfo* info) override;

  // Adds the testing URLs to the URLRequestFilter.
  static void AddUrlHandler();

  static GURL GetMockHttpUrl();
  static GURL GetMockHttpsUrl();

 private:
  void GetResponseInfoConst(HttpResponseInfo* info) const;

  void StartAsync();

  const int content_length_ = 10;  // non-zero content-length
  base::WeakPtrFactory<URLRequestHangingReadJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_TEST_URL_REQUEST_URL_REQUEST_HANGING_READ_JOB_H_
