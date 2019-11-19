// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PacFileFetcher is an async interface for fetching a proxy auto config
// script. It is specific to fetching a PAC script; enforces timeout, max-size,
// status code.

#ifndef NET_PROXY_RESOLUTION_PAC_FILE_FETCHER_H_
#define NET_PROXY_RESOLUTION_PAC_FILE_FETCHER_H_

#include "base/strings/string16.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class GURL;

namespace net {

class URLRequestContext;

// Interface for downloading a PAC script. Implementations can enforce
// timeouts, maximum size constraints, content encoding, etc..
class NET_EXPORT_PRIVATE PacFileFetcher {
 public:
  // Destruction should cancel any outstanding requests.
  virtual ~PacFileFetcher() {}

  // Downloads the given PAC URL, and invokes |callback| on completion.
  // Returns OK on success, otherwise the error code. If the return code is
  // ERR_IO_PENDING, then the request completes asynchronously, and |callback|
  // will be invoked later with the final error code.
  // After synchronous or asynchronous completion with a result code of OK,
  // |*utf16_text| is filled with the response. On failure, the result text is
  // an empty string, and the result code is a network error. Some special
  // network errors that may occur are:
  //
  //    ERR_TIMED_OUT         -- the fetch took too long to complete.
  //    ERR_FILE_TOO_BIG      -- the response's body was too large.
  //    ERR_HTTP_RESPONSE_CODE_FAILURE -- non-200 HTTP status code.
  //    ERR_NOT_IMPLEMENTED   -- the response required authentication.
  //
  // If the request is cancelled (either using the "Cancel()" method or by
  // deleting |this|), then no callback is invoked.
  //
  // Only one fetch is allowed to be outstanding at a time.
  virtual int Fetch(const GURL& url,
                    base::string16* utf16_text,
                    CompletionOnceCallback callback,
                    const NetworkTrafficAnnotationTag traffic_annotation) = 0;

  // Aborts the in-progress fetch (if any).
  virtual void Cancel() = 0;

  // Returns the request context that this fetcher uses to issue downloads,
  // or NULL.
  virtual URLRequestContext* GetRequestContext() const = 0;

  // Fails the in-progress fetch (if any) and future requests will fail
  // immediately. GetRequestContext() will always return nullptr after this is
  // called.  Must be called before the URLRequestContext the fetcher was
  // created with is torn down.
  virtual void OnShutdown() = 0;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PAC_FILE_FETCHER_H_
