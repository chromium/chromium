// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

class NetworkDelegate;
class URLRequest;
class URLRequestJob;

class NET_EXPORT URLRequestJobFactory {
 public:
  // TODO(shalev): Move this to URLRequestJobFactoryImpl.
  class NET_EXPORT ProtocolHandler {
   public:
    virtual ~ProtocolHandler();

    virtual URLRequestJob* MaybeCreateJob(
        URLRequest* request, NetworkDelegate* network_delegate) const = 0;

    // Indicates if it should be safe to redirect to |location|. Should handle
    // protocols handled by MaybeCreateJob().  Only called when registered with
    // URLRequestJobFactoryImpl::SetProtocolHandler() not called when used with
    // ProtocolInterceptJobFactory.
    // NOTE(pauljensen): Default implementation returns true.
    virtual bool IsSafeRedirectTarget(const GURL& location) const;
  };

  URLRequestJobFactory();
  virtual ~URLRequestJobFactory();

  virtual URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      URLRequest* request,
      NetworkDelegate* network_delegate) const = 0;

  virtual bool IsHandledProtocol(const std::string& scheme) const = 0;

  virtual bool IsSafeRedirectTarget(const GURL& location) const = 0;

 protected:
  THREAD_CHECKER(thread_checker_);

 private:
  DISALLOW_COPY_AND_ASSIGN(URLRequestJobFactory);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_H_
