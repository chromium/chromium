// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/url_loader_monitor.h"

#include "base/bind.h"

namespace content {

URLLoaderMonitor::URLLoaderMonitor(std::set<GURL> urls_to_wait_for)
    : interceptor_(std::make_unique<content::URLLoaderInterceptor>(
          base::BindRepeating(&URLLoaderMonitor::OnRequest,
                              base::Unretained(this)))) {
  base::AutoLock autolock(lock_);
  urls_to_wait_for_ = std::move(urls_to_wait_for);
}

URLLoaderMonitor::~URLLoaderMonitor() {
  // This is needed because |interceptor_| is a cross-thread object that may
  // invoke the callback passed to it on the IO thread at any time until it's
  // destroyed. Therefore, it must be destroyed before |this| is.
  interceptor_.reset();
}

absl::optional<network::ResourceRequest> URLLoaderMonitor::GetRequestInfo(
    const GURL& url) {
  base::AutoLock autolock(lock_);
  const auto resource_request = resource_request_map_.find(url);
  if (resource_request == resource_request_map_.end())
    return absl::nullopt;
  return resource_request->second;
}

void URLLoaderMonitor::WaitForUrls() {
  run_loop_.Run();
}

bool URLLoaderMonitor::OnRequest(
    content::URLLoaderInterceptor::RequestParams* params) {
  base::AutoLock autolock(lock_);
  resource_request_map_[params->url_request.url] = params->url_request;

  if (urls_to_wait_for_.erase(params->url_request.url) &&
      urls_to_wait_for_.empty()) {
    run_loop_.Quit();
  }

  // Don't override default handling of the request.
  return false;
}

}  // namespace content
