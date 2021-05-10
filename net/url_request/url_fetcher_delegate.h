// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_FETCHER_DELEGATE_H_
#define NET_URL_REQUEST_URL_FETCHER_DELEGATE_H_

#include <stdint.h>

#include "net/base/net_export.h"

namespace net {

class URLFetcher;

// A delegate interface for users of URLFetcher.
class NET_EXPORT URLFetcherDelegate {
 public:
  // This will be called when the URL has been fetched, successfully or not.
  // Use accessor methods on |source| to get the results.
  virtual void OnURLFetchComplete(const URLFetcher* source) = 0;

  // This will be called when some part of the response is read. |current|
  // denotes the number of bytes received up to the call, and |total| is the
  // expected total size of the response (or -1 if not determined).
  // |current_network_bytes| denotes the number of network bytes received
  // up to the call, excluding redirect bodies, SSL and proxy handshakes.
  virtual void OnURLFetchDownloadProgress(const URLFetcher* source,
                                          int64_t current,
                                          int64_t total,
                                          int64_t current_network_bytes);

  // This will be called when uploading of POST or PUT requests proceeded.
  // |current| denotes the number of bytes sent so far, and |total| is the
  // total size of uploading data (or -1 if chunked upload is enabled).
  virtual void OnURLFetchUploadProgress(const URLFetcher* source,
                                        int64_t current,
                                        int64_t total);

 protected:
  virtual ~URLFetcherDelegate();
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_FETCHER_DELEGATE_H_
