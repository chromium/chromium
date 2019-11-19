// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_REDIRECT_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_REDIRECT_JOB_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_job.h"

class GURL;

namespace net {

// A URLRequestJob that will redirect the request to the specified URL. This is
// useful to restart a request at a different URL based on the result of another
// job. The redirect URL could be visible to scripts if the redirect points to
// a same-origin URL, or if the redirection target is served with CORS response
// headers.
class NET_EXPORT URLRequestRedirectJob : public URLRequestJob {
 public:
  // Valid status codes for the redirect job. Other 30x codes are theoretically
  // valid, but unused so far.  Both 302 and 307 are temporary redirects, with
  // the difference being that 302 converts POSTs to GETs and removes upload
  // data.
  enum ResponseCode {
    REDIRECT_302_FOUND = 302,
    REDIRECT_307_TEMPORARY_REDIRECT = 307,
  };

  // Constructs a job that redirects to the specified URL.  |redirect_reason| is
  // logged for debugging purposes, and must not be an empty string.
  URLRequestRedirectJob(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const GURL& redirect_destination,
                        ResponseCode response_code,
                        const std::string& redirect_reason);

  ~URLRequestRedirectJob() override;

  // URLRequestJob implementation:
  void GetResponseInfo(HttpResponseInfo* info) override;
  void GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  void Start() override;
  void Kill() override;
  bool CopyFragmentOnRedirect(const GURL& location) const override;
  void SetRequestHeadersCallback(RequestHeadersCallback callback) override;

 private:
  void StartAsync();

  const GURL redirect_destination_;
  const ResponseCode response_code_;
  base::TimeTicks receive_headers_end_;
  base::Time response_time_;
  std::string redirect_reason_;

  scoped_refptr<HttpResponseHeaders> fake_headers_;

  RequestHeadersCallback request_headers_callback_;

  base::WeakPtrFactory<URLRequestRedirectJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_REDIRECT_JOB_H_
