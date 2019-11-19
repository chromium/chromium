// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_IMPL_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {

class URLRequestInterceptor;

class NET_EXPORT URLRequestJobFactoryImpl : public URLRequestJobFactory {
 public:
  URLRequestJobFactoryImpl();
  ~URLRequestJobFactoryImpl() override;

  // Sets the ProtocolHandler for a scheme. Returns true on success, false on
  // failure (a ProtocolHandler already exists for |scheme|).
  bool SetProtocolHandler(const std::string& scheme,
                          std::unique_ptr<ProtocolHandler> protocol_handler);

  // URLRequestJobFactory implementation
  URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      URLRequest* request,
      NetworkDelegate* network_delegate) const override;

  bool IsHandledProtocol(const std::string& scheme) const override;
  bool IsSafeRedirectTarget(const GURL& location) const override;

 private:
  // For testing only.
  friend class URLRequestFilter;

  typedef std::map<std::string, std::unique_ptr<ProtocolHandler>>
      ProtocolHandlerMap;

  // Sets a global URLRequestInterceptor for testing purposes.  The interceptor
  // is given the chance to intercept any request before the corresponding
  // ProtocolHandler, but after any other URLRequestJobFactories layered on top
  // of the URLRequestJobFactoryImpl.
  // If an interceptor is set, the old interceptor must be cleared before
  // setting a new one.
  static void SetInterceptorForTesting(URLRequestInterceptor* interceptor);

  ProtocolHandlerMap protocol_handler_map_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestJobFactoryImpl);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_IMPL_H_
