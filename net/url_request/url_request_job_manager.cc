// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_manager.h"

#include <algorithm>

#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/net_buildflags.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {

// The built-in set of protocol factories
namespace {

struct SchemeToFactory {
  const char* scheme;
  URLRequest::ProtocolFactory* factory;
};

}  // namespace

static const SchemeToFactory kBuiltinFactories[] = {
    {"http", URLRequestHttpJob::Factory},
    {"https", URLRequestHttpJob::Factory},

#if BUILDFLAG(ENABLE_WEBSOCKETS)
    {"ws", URLRequestHttpJob::Factory},
    {"wss", URLRequestHttpJob::Factory},
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
};

// static
URLRequestJobManager* URLRequestJobManager::GetInstance() {
  return base::Singleton<URLRequestJobManager>::get();
}

URLRequestJob* URLRequestJobManager::CreateJob(
    URLRequest* request, NetworkDelegate* network_delegate) const {
  DCHECK(IsAllowedThread());

  // If we are given an invalid URL, then don't even try to inspect the scheme.
  if (!request->url().is_valid())
    return new URLRequestErrorJob(request, network_delegate, ERR_INVALID_URL);

  // We do this here to avoid asking interceptors about unsupported schemes.
  const URLRequestJobFactory* job_factory =
      request->context()->job_factory();

  const std::string& scheme = request->url().scheme();  // already lowercase
  if (!job_factory->IsHandledProtocol(scheme)) {
    return new URLRequestErrorJob(
        request, network_delegate, ERR_UNKNOWN_URL_SCHEME);
  }

  // THREAD-SAFETY NOTICE:
  //   We do not need to acquire the lock here since we are only reading our
  //   data structures.  They should only be modified on the current thread.

  // See if the request should be intercepted.
  //
  URLRequestJob* job = job_factory->MaybeCreateJobWithProtocolHandler(
      scheme, request, network_delegate);
  if (job)
    return job;

  // See if the request should be handled by a built-in protocol factory.
  for (size_t i = 0; i < base::size(kBuiltinFactories); ++i) {
    if (scheme == kBuiltinFactories[i].scheme) {
      URLRequestJob* new_job =
          (kBuiltinFactories[i].factory)(request, network_delegate, scheme);
      DCHECK(new_job);  // The built-in factories are not expected to fail!
      return new_job;
    }
  }

  // If we reached here, then it means that a registered protocol factory
  // wasn't interested in handling the URL.  That is fairly unexpected, and we
  // don't have a specific error to report here :-(
  LOG(WARNING) << "Failed to map: " << request->url().spec();
  return new URLRequestErrorJob(request, network_delegate, ERR_FAILED);
}

// static
bool URLRequestJobManager::SupportsScheme(const std::string& scheme) {
  for (size_t i = 0; i < base::size(kBuiltinFactories); ++i) {
    if (base::LowerCaseEqualsASCII(scheme, kBuiltinFactories[i].scheme))
      return true;
  }

  return false;
}

URLRequestJobManager::URLRequestJobManager() = default;

URLRequestJobManager::~URLRequestJobManager() = default;

}  // namespace net
