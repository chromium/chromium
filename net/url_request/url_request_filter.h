// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NET_URL_REQUEST_URL_REQUEST_FILTER_H_
#define NET_URL_REQUEST_URL_REQUEST_FILTER_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "net/url_request/url_request_interceptor.h"

class GURL;

namespace net {
class URLRequest;
class URLRequestJob;
class URLRequestInterceptor;

// A class to help filter URLRequest jobs based on the URL of the request
// rather than just the scheme.  Example usage:
//
// // Intercept "scheme://host/" requests.
// URLRequestFilter::GetInstance()->AddHostnameInterceptor(
//     "scheme", "host", std::move(interceptor));
// // Add special handling for the URL http://foo.com/
// URLRequestFilter::GetInstance()->AddUrlInterceptor(
//     GURL("http://foo.com/"), std::move(interceptor));
//
// The URLRequestFilter is implemented as a singleton that is not thread-safe,
// and hence must only be used in test code where the network stack is used
// from a single thread. It must only be accessed on that networking thread.
// One exception is that during startup, before any message loops have been
// created, interceptors may be added (the session restore tests rely on this).
// If the URLRequestFilter::MaybeInterceptRequest can't find a handler for a
// request, it returns NULL and lets the configured ProtocolHandler handle the
// request.
class URLRequestFilter : public URLRequestInterceptor {
 public:
  // Singleton instance for use.
  static URLRequestFilter* GetInstance();

  URLRequestFilter(const URLRequestFilter&) = delete;
  URLRequestFilter& operator=(const URLRequestFilter&) = delete;

  void AddHostnameInterceptor(
      const std::string& scheme,
      const std::string& hostname,
      std::unique_ptr<URLRequestInterceptor> interceptor);
  void RemoveHostnameHandler(const std::string& scheme,
                             const std::string& hostname);

  // Returns true if we successfully added the URL handler.  This will replace
  // old handlers for the URL if one existed.
  bool AddUrlInterceptor(const GURL& url,
                         std::unique_ptr<URLRequestInterceptor> interceptor);

  void RemoveUrlHandler(const GURL& url);

  // Clear all the existing URL and hostname handlers.  Resets the hit count.
  void ClearHandlers();

  // Returns the number of times a handler was used to service a request.
  int hit_count() const { return hit_count_; }

  // URLRequestInterceptor implementation:
  std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
      URLRequest* request) const override;

 private:
  // scheme,hostname -> URLRequestInterceptor
  using HostnameInterceptorMap =
      std::map<std::pair<std::string, std::string>,
               std::unique_ptr<URLRequestInterceptor>>;
  // URL -> URLRequestInterceptor
  using URLInterceptorMap =
      std::unordered_map<std::string, std::unique_ptr<URLRequestInterceptor>>;

  URLRequestFilter();
  ~URLRequestFilter() override;

  // Maps hostnames to interceptors.  Hostnames take priority over URLs.
  HostnameInterceptorMap hostname_interceptor_map_;

  // Maps URLs to interceptors.
  URLInterceptorMap url_interceptor_map_;

  mutable int hit_count_ = 0;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_FILTER_H_
