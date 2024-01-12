// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/url_loader_monitor.h"

#include <map>
#include <memory>
#include <optional>
#include <set>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/public/test/url_loader_interceptor.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "url/gurl.h"

namespace content {

URLLoaderMonitor::URLLoaderMonitor(std::set<GURL> urls_to_wait_for_request)
    : interceptor_(std::make_unique<content::URLLoaderInterceptor>(
          base::BindRepeating(&URLLoaderMonitor::OnRequest,
                              base::Unretained(this)),
          base::BindRepeating(&URLLoaderMonitor::OnRequestComplete,
                              base::Unretained(this)))) {
  base::AutoLock autolock(lock_);
  urls_to_wait_for_request_ = std::move(urls_to_wait_for_request);
}

URLLoaderMonitor::~URLLoaderMonitor() {
  // This is needed because |interceptor_| is a cross-thread object that may
  // invoke the callback passed to it on the IO thread at any time until it's
  // destroyed. Therefore, it must be destroyed before |this| is.
  interceptor_.reset();
}

std::optional<network::ResourceRequest> URLLoaderMonitor::GetRequestInfo(
    const GURL& url) {
  base::AutoLock autolock(lock_);
  const auto resource_request = resource_request_map_.find(url);
  if (resource_request == resource_request_map_.end())
    return std::nullopt;
  return resource_request->second;
}

std::optional<network::URLLoaderCompletionStatus>
URLLoaderMonitor::GetCompletionStatus(const GURL& url) {
  base::AutoLock autolock(lock_);
  const auto completion_status = resource_completion_status_map_.find(url);
  if (completion_status == resource_completion_status_map_.end())
    return std::nullopt;
  return completion_status->second;
}

const network::ResourceRequest& URLLoaderMonitor::WaitForUrl(
    const GURL& url_to_wait_for_request) {
  WaitForUrls({url_to_wait_for_request});

  base::AutoLock autolock(lock_);
  DCHECK(resource_request_map_.find(url_to_wait_for_request) !=
         resource_request_map_.end());
  return resource_request_map_[url_to_wait_for_request];
}

void URLLoaderMonitor::WaitForUrls(
    const std::set<GURL>& urls_to_wait_for_request) {
  base::RunLoop run_loop;
  {
    base::AutoLock autolock(lock_);
    for (const GURL& url : urls_to_wait_for_request) {
      if (resource_request_map_.find(url) != resource_request_map_.end())
        continue;
      urls_to_wait_for_request_.insert(url);
    }

    if (urls_to_wait_for_request_.empty())
      return;

    quit_request_run_loop_callback_ = run_loop.QuitClosure();
  }

  run_loop.Run();
}

const network::URLLoaderCompletionStatus&
URLLoaderMonitor::WaitForRequestCompletion(
    const GURL& url_to_wait_for_completion) {
  WaitForRequestCompletion(std::set<GURL>{url_to_wait_for_completion});

  base::AutoLock autolock(lock_);
  DCHECK(resource_completion_status_map_.find(url_to_wait_for_completion) !=
         resource_completion_status_map_.end());
  return resource_completion_status_map_[url_to_wait_for_completion];
}

void URLLoaderMonitor::WaitForRequestCompletion(
    const std::set<GURL>& urls_to_wait_for_completion) {
  base::RunLoop run_loop;
  {
    base::AutoLock autolock(lock_);
    for (const GURL& url : urls_to_wait_for_completion) {
      if (resource_completion_status_map_.find(url) !=
          resource_completion_status_map_.end())
        continue;
      urls_to_wait_for_completion_.insert(url);
    }

    if (urls_to_wait_for_completion_.empty())
      return;
    quit_completion_run_loop_callback_ = run_loop.QuitClosure();
  }

  run_loop.Run();
}

void URLLoaderMonitor::ClearRequests() {
  base::AutoLock autolock(lock_);
  resource_request_map_.clear();
  resource_completion_status_map_.clear();
}

bool URLLoaderMonitor::OnRequest(
    content::URLLoaderInterceptor::RequestParams* params) {
  base::AutoLock autolock(lock_);
  resource_request_map_[params->url_request.url] = params->url_request;

  if (urls_to_wait_for_request_.erase(params->url_request.url) &&
      urls_to_wait_for_request_.empty()) {
    // `run_loop_` my be null if WaitForUrls() has not been invoked, and URLs
    // were passed into the constructor call.
    if (quit_request_run_loop_callback_)
      std::move(quit_request_run_loop_callback_).Run();
  }

  // Don't override default handling of the request.
  return false;
}

void URLLoaderMonitor::OnRequestComplete(
    const GURL& request_url,
    const network::URLLoaderCompletionStatus& status) {
  base::AutoLock autolock(lock_);
  resource_completion_status_map_[request_url] = status;

  if (urls_to_wait_for_completion_.erase(request_url) &&
      urls_to_wait_for_completion_.empty()) {
    std::move(quit_completion_run_loop_callback_).Run();
  }
}

}  // namespace content
