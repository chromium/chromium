// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_ERROR_HANDLER_H_
#define CONTENT_BROWSER_SSL_SSL_ERROR_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

class WebContents;

// SSLErrorHandler is the UI-thread class for handling SSL certificate
// errors. Users of this class can call CancelRequest(),
// ContinueRequest(), or DenyRequest() when a decision about how to
// handle the error has been made. Users of this class must
// call exactly one of those methods exactly once.
class SSLErrorHandler {
 public:
  class Delegate {
   public:
    // Called when SSLErrorHandler decides to cancel the request because of
    // the SSL error.
    virtual void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) = 0;

    // Called when SSLErrorHandler decides to continue the request despite the
    // SSL error.
    virtual void ContinueSSLRequest() = 0;

   protected:
    virtual ~Delegate() {}
  };

  SSLErrorHandler(WebContents* web_contents,
                  const base::WeakPtr<Delegate>& delegate,
                  bool is_primary_main_frame_request,
                  const GURL& url,
                  int net_error,
                  const net::SSLInfo& ssl_info,
                  bool fatal);

  SSLErrorHandler(const SSLErrorHandler&) = delete;
  SSLErrorHandler& operator=(const SSLErrorHandler&) = delete;

  virtual ~SSLErrorHandler();

  const net::SSLInfo& ssl_info() const { return ssl_info_; }

  const GURL& request_url() const { return request_url_; }

  bool is_primary_main_frame_request() const {
    return is_primary_main_frame_request_;
  }

  WebContents* web_contents() const { return web_contents_; }

  int cert_error() const { return cert_error_; }

  bool fatal() const { return fatal_; }

  // Cancels the associated net::URLRequest.
  void CancelRequest();

  // Continue the net::URLRequest ignoring any previous errors.  Note that some
  // errors cannot be ignored, in which case this will result in the request
  // being canceled.
  void ContinueRequest();

  // Cancels the associated net::URLRequest and mark it as denied.  The renderer
  // processes such request in a special manner, optionally replacing them
  // with alternate content (typically frames content is replaced with a
  // warning message).
  void DenyRequest();

 private:
  base::WeakPtr<Delegate> delegate_;

  // The URL for the request that generated the error.
  const GURL request_url_;

  // Whether this request is for the primary main frame's html.
  const bool is_primary_main_frame_request_;

  // The net::SSLInfo associated with the request that generated the error.
  const net::SSLInfo ssl_info_;

  // A net error code describing the error that occurred.
  const int cert_error_;

  // True if the error is from a host requiring certificate errors to be fatal.
  const bool fatal_;

  // The WebContents associated with the request that generated the error.
  raw_ptr<WebContents> web_contents_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_ERROR_HANDLER_H_
