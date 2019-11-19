// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_filter.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/stl_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace net {

namespace {

// When adding interceptors, DCHECK that this function returns true.
bool OnMessageLoopForInterceptorAddition() {
  // Return true if called on a MessageLoopForIO or if there is no MessageLoop.
  // Checking for a MessageLoopForIO is a best effort at determining whether the
  // current thread is a networking thread.  Allowing cases without a
  // MessageLoop is required for some tests where there is no chance to insert
  // an interceptor between a networking thread being started and a resource
  // request being issued.
  return base::MessageLoopCurrentForIO::IsSet() ||
         !base::MessageLoopCurrent::IsSet();
}

// When removing interceptors, DCHECK that this function returns true.
bool OnMessageLoopForInterceptorRemoval() {
  // Checking for a MessageLoopCurrentForIO is a best effort at determining
  // whether the current thread is a networking thread.
  return base::MessageLoopCurrentForIO::IsSet();
}

}  // namespace

URLRequestFilter* URLRequestFilter::shared_instance_ = nullptr;

// static
URLRequestFilter* URLRequestFilter::GetInstance() {
  DCHECK(OnMessageLoopForInterceptorAddition());
  if (!shared_instance_)
    shared_instance_ = new URLRequestFilter;
  return shared_instance_;
}

void URLRequestFilter::AddHostnameInterceptor(
    const std::string& scheme,
    const std::string& hostname,
    std::unique_ptr<URLRequestInterceptor> interceptor) {
  DCHECK(OnMessageLoopForInterceptorAddition());
  DCHECK_EQ(0u, hostname_interceptor_map_.count(make_pair(scheme, hostname)));
  hostname_interceptor_map_[make_pair(scheme, hostname)] =
      std::move(interceptor);

#ifndef NDEBUG
  // Check to see if we're masking URLs in the url_interceptor_map_.
  for (const auto& pair : url_interceptor_map_) {
    const GURL& url = GURL(pair.first);
    HostnameInterceptorMap::const_iterator host_it =
        hostname_interceptor_map_.find(make_pair(url.scheme(), url.host()));
    if (host_it != hostname_interceptor_map_.end())
      NOTREACHED();
  }
#endif  // !NDEBUG
}

void URLRequestFilter::RemoveHostnameHandler(const std::string& scheme,
                                             const std::string& hostname) {
  DCHECK(OnMessageLoopForInterceptorRemoval());
  int removed = hostname_interceptor_map_.erase(make_pair(scheme, hostname));
  DCHECK(removed);

  // Note that we don't unregister from the URLRequest ProtocolFactory as
  // this would leave no protocol factory for the remaining hostname and URL
  // handlers.
}

bool URLRequestFilter::AddUrlInterceptor(
    const GURL& url,
    std::unique_ptr<URLRequestInterceptor> interceptor) {
  DCHECK(OnMessageLoopForInterceptorAddition());
  if (!url.is_valid())
    return false;
  DCHECK_EQ(0u, url_interceptor_map_.count(url.spec()));
  url_interceptor_map_[url.spec()] = std::move(interceptor);

  // Check to see if this URL is masked by a hostname handler.
  DCHECK_EQ(0u, hostname_interceptor_map_.count(make_pair(url.scheme(),
                                                          url.host())));

  return true;
}

void URLRequestFilter::RemoveUrlHandler(const GURL& url) {
  DCHECK(OnMessageLoopForInterceptorRemoval());
  size_t removed = url_interceptor_map_.erase(url.spec());
  DCHECK(removed);
  // Note that we don't unregister from the URLRequest ProtocolFactory as
  // this would leave no protocol factory for the remaining hostname and URL
  // handlers.
}

void URLRequestFilter::ClearHandlers() {
  DCHECK(OnMessageLoopForInterceptorRemoval());
  url_interceptor_map_.clear();
  hostname_interceptor_map_.clear();
  hit_count_ = 0;
}

URLRequestJob* URLRequestFilter::MaybeInterceptRequest(
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  DCHECK(base::MessageLoopCurrentForIO::Get());
  URLRequestJob* job = nullptr;
  if (!request->url().is_valid())
    return nullptr;

  // Check the hostname map first.
  const std::string hostname = request->url().host();
  const std::string scheme = request->url().scheme();

  {
    auto it = hostname_interceptor_map_.find(make_pair(scheme, hostname));
    if (it != hostname_interceptor_map_.end())
      job = it->second->MaybeInterceptRequest(request, network_delegate);
  }

  if (!job) {
    // Not in the hostname map, check the url map.
    const std::string& url = request->url().spec();
    auto it = url_interceptor_map_.find(url);
    if (it != url_interceptor_map_.end())
      job = it->second->MaybeInterceptRequest(request, network_delegate);
  }
  if (job) {
    DVLOG(1) << "URLRequestFilter hit for " << request->url().spec();
    hit_count_++;
  }
  return job;
}

URLRequestFilter::URLRequestFilter() : hit_count_(0) {
  DCHECK(OnMessageLoopForInterceptorAddition());
  URLRequestJobFactoryImpl::SetInterceptorForTesting(this);
}

URLRequestFilter::~URLRequestFilter() {
  DCHECK(OnMessageLoopForInterceptorRemoval());
  URLRequestJobFactoryImpl::SetInterceptorForTesting(nullptr);
}

}  // namespace net
