// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_URL_FETCHER_H_
#define NET_DEVICE_BOUND_SESSIONS_URL_FETCHER_H_

#include "net/url_request/url_request.h"

namespace net {
class URLRequestContext;
}

namespace net::device_bound_sessions {

class URLFetcher : public URLRequest::Delegate {
 public:
  URLFetcher(const URLRequestContext* context,
             GURL url,
             std::optional<net::NetLogSource> net_log_source);
  ~URLFetcher() override;

  void Start(base::OnceClosure complete_callback);

  URLRequest& request() { return *request_; }
  const std::string& data_received() const { return data_received_; }
  int net_error() const { return net_error_; }
  const CookieAndLineAccessResultList& maybe_stored_cookies() const {
    return request_->maybe_stored_cookies();
  }

 private:
  // URLRequest::Delegate

  // TODO(crbug.com/438783632): Look into if OnAuthRequired might need to be
  // customize for DBSC

  // TODO(crbug.com/438783633): Think about what to do for DBSC with
  // OnCertificateRequested, leaning towards not supporting it but not sure.

  // Always cancel requests on SSL errors, this is the default implementation
  // of OnSSLCertificateError.

  // This is always called unless the request is deleted before it is called.
  void OnResponseStarted(URLRequest* request, int net_error) override;

  void OnReadCompleted(URLRequest* request, int bytes_read_or_error) override;

  std::unique_ptr<URLRequest> request_;
  scoped_refptr<IOBuffer> buf_;
  std::string data_received_;
  int net_error_ = OK;
  base::OnceClosure callback_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_URL_FETCHER_H_
