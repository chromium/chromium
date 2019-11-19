// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_NET_FETCHER_H_
#define NET_CERT_CERT_NET_FETCHER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

// CertNetFetcher is a synchronous interface for fetching AIA URLs and CRL
// URLs. It is shared between a caller thread (which starts and waits for
// fetches), and a network thread (which does the actual fetches). It can be
// shutdown from the network thread to cancel outstanding requests.
//
// A Request object is returned when starting a fetch. The consumer can
// use this as a handle for aborting the request (by freeing it), or reading
// the result of the request (WaitForResult)
class NET_EXPORT CertNetFetcher
    : public base::RefCountedThreadSafe<CertNetFetcher> {
 public:
  class Request {
   public:
    virtual ~Request() {}

    // WaitForResult() can be called at most once.
    //
    // It will block and wait for the (network) request to complete, and
    // then write the result into the provided out-parameters.
    virtual void WaitForResult(Error* error, std::vector<uint8_t>* bytes) = 0;
  };

  // This value can be used in place of timeout or max size limits.
  enum { DEFAULT = -1 };

  CertNetFetcher() {}

  // Shuts down the CertNetFetcher and cancels outstanding network requests. It
  // is not guaranteed that any outstanding or subsequent
  // Request::WaitForResult() calls will be completed. Shutdown() must be called
  // from the network thread. It can be called more than once, but must be
  // called before the CertNetFetcher is destroyed.
  virtual void Shutdown() = 0;

  // The Fetch*() methods start a request which can be cancelled by
  // deleting the returned Request. Here is the meaning of the common
  // parameters:
  //
  //   * url -- The http:// URL to fetch.
  //   * timeout_seconds -- The maximum allowed duration for the fetch job. If
  //         this delay is exceeded then the request will fail. To use a default
  //         timeout pass DEFAULT.
  //   * max_response_bytes -- The maximum size of the response body. If this
  //     size is exceeded then the request will fail. To use a default timeout
  //     pass DEFAULT.

  virtual WARN_UNUSED_RESULT std::unique_ptr<Request> FetchCaIssuers(
      const GURL& url,
      int timeout_milliseconds,
      int max_response_bytes) = 0;

  virtual WARN_UNUSED_RESULT std::unique_ptr<Request> FetchCrl(
      const GURL& url,
      int timeout_milliseconds,
      int max_response_bytes) = 0;

  virtual WARN_UNUSED_RESULT std::unique_ptr<Request> FetchOcsp(
      const GURL& url,
      int timeout_milliseconds,
      int max_response_bytes) = 0;

 protected:
  virtual ~CertNetFetcher() {}

 private:
  friend class base::RefCountedThreadSafe<CertNetFetcher>;
  DISALLOW_COPY_AND_ASSIGN(CertNetFetcher);
};

}  // namespace net

#endif  // NET_CERT_CERT_NET_FETCHER_H_
