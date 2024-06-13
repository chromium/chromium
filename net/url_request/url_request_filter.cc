// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_filter.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/current_thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"

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
  return base::CurrentIOThread::IsSet() || !base::CurrentThread::IsSet();
}

// When removing interceptors, DCHECK that this function returns true.
bool OnMessageLoopForInterceptorRemoval() {
  // Checking for a CurrentIOThread is a best effort at determining
  // whether the current thread is a networking thread.
  return base::CurrentIOThread::IsSet();
}

}  // namespace

// static
URLRequestFilter* URLRequestFilter::GetInstance() {
  // base::NoDestructor is not used because most tests don't use
  // URLRequestFilter, so there's no point in reserving space for it.
  static URLRequestFilter* instance = new URLRequestFilter();
  DCHECK(OnMessageLoopForInterceptorAddition());
  return instance;
}

void URLRequestFilter::AddHostnameInterceptor(
    const std::string& scheme,
    const std::string& hostname,
    std::unique_ptr<URLRequestInterceptor> interceptor) {
  DCHECK(OnMessageLoopForInterceptorAddition());
  DCHECK_EQ(0u, hostname_interceptor_map_.count(std::pair(scheme, hostname)));
  hostname_interceptor_map_[std::pair(scheme, hostname)] =
      std::move(interceptor);

#if !defined(NDEBUG)
  // Check to see if we're masking URLs in the url_interceptor_map_.
  for (const auto& [url_spec, _] : url_interceptor_map_) {
    const GURL url(url_spec);
    DCHECK(!hostname_interceptor_map_.contains({url.scheme(), url.host()}));
  }
#endif  // !NDEBUG
}

void URLRequestFilter::RemoveHostnameHandler(const std::string& scheme,
                                             const std::string& hostname) {
  DCHECK(OnMessageLoopForInterceptorRemoval());
  int removed = hostname_interceptor_map_.erase(std::pair(scheme, hostname));
  DCHECK(removed);
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
  DCHECK_EQ(
      0u, hostname_interceptor_map_.count(std::pair(url.scheme(), url.host())));

  return true;
}

void URLRequestFilter::RemoveUrlHandler(const GURL& url) {
  DCHECK(OnMessageLoopForInterceptorRemoval());
  size_t removed = url_interceptor_map_.erase(url.spec());
  DCHECK(removed);
}

void URLRequestFilter::ClearHandlers() {
  DCHECK(OnMessageLoopForInterceptorRemoval());
  url_interceptor_map_.clear();
  hostname_interceptor_map_.clear();
  hit_count_ = 0;
}

std::unique_ptr<URLRequestJob> URLRequestFilter::MaybeInterceptRequest(
    URLRequest* request) const {
  DCHECK(base::CurrentIOThread::Get());
  if (!request->url().is_valid())
    return nullptr;

  std::unique_ptr<URLRequestJob> job;

  // Check the hostname map first.
  const std::string hostname = request->url().host();
  const std::string scheme = request->url().scheme();

  {
    auto it = hostname_interceptor_map_.find(std::pair(scheme, hostname));
    if (it != hostname_interceptor_map_.end())
      job = it->second->MaybeInterceptRequest(request);
  }

  if (!job) {
    // Not in the hostname map, check the url map.
    const std::string& url = request->url().spec();
    auto it = url_interceptor_map_.find(url);
    if (it != url_interceptor_map_.end())
      job = it->second->MaybeInterceptRequest(request);
  }
  if (job) {
    DVLOG(1) << "URLRequestFilter hit for " << request->url().spec();
    hit_count_++;
  }
  return job;
}

URLRequestFilter::URLRequestFilter() {
  DCHECK(OnMessageLoopForInterceptorAddition());
  URLRequestJobFactory::SetInterceptorForTesting(this);
}

URLRequestFilter::~URLRequestFilter() {
  NOTREACHED();
}

}  // namespace net
