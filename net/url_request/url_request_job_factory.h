// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_H_

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

class URLRequest;
class URLRequestInterceptor;
class URLRequestJob;

// Creates URLRequestJobs for URLRequests. Internally uses a mapping of schemes
// to ProtocolHandlers, which handle the actual requests.
class NET_EXPORT URLRequestJobFactory {
 public:
  class NET_EXPORT ProtocolHandler {
   public:
    virtual ~ProtocolHandler();

    // Creates a URLRequestJob for the particular protocol. Never returns
    // nullptr.
    virtual std::unique_ptr<URLRequestJob> CreateJob(
        URLRequest* request) const = 0;

    // Indicates if it should be safe to redirect to |location|. Should handle
    // protocols handled by MaybeCreateJob().
    virtual bool IsSafeRedirectTarget(const GURL& location) const;
  };

  URLRequestJobFactory();

  URLRequestJobFactory(const URLRequestJobFactory&) = delete;
  URLRequestJobFactory& operator=(const URLRequestJobFactory&) = delete;

  virtual ~URLRequestJobFactory();

  // Sets the ProtocolHandler for a scheme. Returns true on success, false on
  // failure (a ProtocolHandler already exists for |scheme|).
  bool SetProtocolHandler(const std::string& scheme,
                          std::unique_ptr<ProtocolHandler> protocol_handler);

  // Creates a URLRequestJob for |request|. Returns a URLRequestJob that fails
  // with net::Error code if unable to handle request->url().
  //
  // Virtual for tests.
  virtual std::unique_ptr<URLRequestJob> CreateJob(URLRequest* request) const;

  // Returns true if it's safe to redirect to |location|.
  //
  // Virtual for tests.
  virtual bool IsSafeRedirectTarget(const GURL& location) const;

 protected:
  // Protected for (test-only) subclasses.
  THREAD_CHECKER(thread_checker_);

 private:
  // For testing only.
  friend class URLRequestFilter;

  using ProtocolHandlerMap =
      std::map<std::string, std::unique_ptr<ProtocolHandler>>;

  // Sets a global URLRequestInterceptor for testing purposes.  The interceptor
  // is given the chance to intercept any request before the corresponding
  // ProtocolHandler. If an interceptor is set, the old interceptor must be
  // cleared before setting a new one.
  static void SetInterceptorForTesting(URLRequestInterceptor* interceptor);

  ProtocolHandlerMap protocol_handler_map_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_H_
