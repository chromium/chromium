// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_H_
#define IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/time/time.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_response_info.h"

namespace net {
class URLRequestContextGetter;

class HTTPProtocolHandlerDelegate {
 public:
  // Sets the global instance of the HTTPProtocolHandlerDelegate.
  static void SetInstance(HTTPProtocolHandlerDelegate* delegate);

  virtual ~HTTPProtocolHandlerDelegate() {}

  // Returns true if CRNHTTPProtocolHandler should handle the request.
  // Returns false if the request should be passed down the NSURLProtocol chain.
  virtual bool CanHandleRequest(NSURLRequest* request) = 0;

  // If IsRequestSupported returns true, |request| will be processed, otherwise
  // a NSURLErrorUnsupportedURL error is generated.
  virtual bool IsRequestSupported(NSURLRequest* request) = 0;

  // Returns the request context used. Must not return null.
  virtual URLRequestContextGetter* GetDefaultURLRequestContext() = 0;
};

// Delegate class which supplies a metrics callback that is invoked when a net
// request is stopped.
class MetricsDelegate {
 public:
  // Simple struct collecting all of the metrics data that is passed through to
  // the MetricsDelegate callback.
  struct Metrics {
    Metrics();

    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;

    ~Metrics();

    NSURLSessionTask* task;
    LoadTimingInfo load_timing_info;
    HttpResponseInfo response_info;
    base::Time response_end_time;
  };

  // Set the global instance of the MetricsDelegate.
  static void SetInstance(MetricsDelegate* delegate);

  virtual ~MetricsDelegate() = default;

  // This is invoked once when the request begins, in order to set up things
  // which may be needed by OnStopNetRequest.
  virtual void OnStartNetRequest(NSURLSessionTask* task) = 0;

  // This is invoked once the request is finally stopped, with metrics data
  // collect by net passed in as an argument.
  virtual void OnStopNetRequest(std::unique_ptr<Metrics> metrics) = 0;
};

}  // namespace net

// Custom NSURLProtocol handling HTTP and HTTPS requests.
// The HttpProtocolHandler is registered as a NSURLProtocol in the iOS system.
// This protocol is called for each NSURLRequest. This allows handling the
// requests issued by UIWebView using our own network stack.
@interface CRNHTTPProtocolHandler : NSURLProtocol
@end

#endif  // IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_H_
