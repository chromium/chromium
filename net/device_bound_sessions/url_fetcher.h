// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_URL_FETCHER_H_
#define NET_DEVICE_BOUND_SESSIONS_URL_FETCHER_H_

#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "url/origin.h"

namespace net {
class URLRequestContext;
class SSLCertRequestInfo;
class X509Certificate;
class SSLPrivateKey;
}  // namespace net

namespace net::device_bound_sessions {

class NET_EXPORT URLFetcher : public URLRequest::Delegate {
 public:
  URLFetcher(const URLRequestContext* context,
             GURL url,
             const url::Origin& referring_origin,
             std::optional<net::NetLogSource> net_log_source,
             bool is_refresh);
  ~URLFetcher() override;

  void Start(base::OnceClosure complete_callback);
  std::string TakeDataReceived();

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

  // Always cancel requests on SSL errors, this is the default implementation
  // of OnSSLCertificateError.

  // Intercept HTTP 3xx redirects to re-synchronize W3C Fetch Metadata,
  // evaluate cross-origin/same-site origin relationships, and strictly
  // abort insecure (https -> http) protocol downgrades to prevent plaintext
  // DBSC session token leakage.
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override;

  // This is always called unless the request is deleted before it is called.
  void OnResponseStarted(URLRequest* request, int net_error) override;

  void OnReadCompleted(URLRequest* request, int bytes_read_or_error) override;

  void OnCertificateRequested(URLRequest* request,
                              SSLCertRequestInfo* cert_request_info) override;

  void ContinueWithSelectedCertificate(scoped_refptr<X509Certificate> cert,
                                       scoped_refptr<SSLPrivateKey> key,
                                       bool cancel);

  std::unique_ptr<URLRequest> request_;
  scoped_refptr<IOBuffer> buf_;
  std::string data_received_;
  int net_error_ = OK;
  base::OnceClosure callback_;
  const url::Origin referring_origin_;

  base::WeakPtrFactory<URLFetcher> weak_factory_{this};
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_URL_FETCHER_H_
