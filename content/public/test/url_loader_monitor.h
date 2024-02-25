// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_URL_LOADER_MONITOR_H_
#define CONTENT_PUBLIC_TEST_URL_LOADER_MONITOR_H_

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

// Helper class to monitor parameters passed to URLLoaderFactory calls for
// tests. Records parameters of most recent request for each requested URL.
// URLLoaderMonitor starts watching requested URLs as soon as it's constructed.
//
// Since URLLoaderMonitor uses a URLLoaderInterceptor, it cannot be used in
// conjunctions with a URLLoaderInterceptor, and all monitored requests fail
// with Mojo pipe closure errors on destruction of the URLLoaderMonitor.
//
// In general, it's better to add RequestHandlers or observers to the
// EmbeddedTestServer than to use URLLoaderMonitor, for cases where that works.
// This is useful for cases where that doesn't, however - examining particular
// ResourceRequest parameters, examining requests that are blocked without
// making it to the test server, etc.
class URLLoaderMonitor {
 public:
  // If `urls_to_wait_for_request` to non-null, WaitForUrls() may be invoked
  // once to wait for the specified URLs to all be observed. It's recommended
  // that URLs be passed into WaitForUrls() instead of the constructor.
  //
  // TODO(mmenke): Now that WaitForUrls() takes a list of URLs, consider
  // removing `urls_to_wait_for_request` and updating consumers, as any possible
  // behavior involving ClearRequests() and URLs passed to the constructor is a
  // bit awkward.
  explicit URLLoaderMonitor(std::set<GURL> urls_to_wait_for_request = {});
  URLLoaderMonitor(const URLLoaderMonitor&) = delete;
  URLLoaderMonitor& operator=(const URLLoaderMonitor&) = delete;
  ~URLLoaderMonitor();

  // Returns the network::ResourceRequest for the most recently observed request
  // for `url`. If no such request has been observed, returns nullptr.
  std::optional<network::ResourceRequest> GetRequestInfo(const GURL& url);

  // Returns the network::URLLoaderCompletionStatus of the most recently
  // completed request for `url`. It's possible for GetRequestInfo() and
  // GetCompletionStatus() to return information on different requests,
  // depending on request start/completion ordering.
  std::optional<network::URLLoaderCompletionStatus> GetCompletionStatus(
      const GURL& url);

  // Waits for all the URLs in `urls_to_wait_for` and those passed in to the
  // constructor. Any URL in `urls_to_wait_for_request` requested since the last
  // invocation of ClearUrls() is counted, including URLs requested before
  // WaitForUrls() was invoked. URLs passed in to the constructor, on the other
  // hand, may have been requested before the last ClearRequests() call.
  //
  // The single URL version also returns the network::ResourceRequest of the URL
  // being waited for.
  const network::ResourceRequest& WaitForUrl(
      const GURL& url_to_wait_for_request);
  void WaitForUrls(
      const std::set<GURL>& urls_to_wait_for_request = std::set<GURL>());

  // Waits for requests for the specified URLs to complete. Any URL request that
  // completed since the last invocation of ClearUrls() is counted, including
  // URLs that requested before ClearUrls() was invoked, but only completed
  // afterwards.
  //
  // The single URL version also returns the network::URLLoaderCompletionStatus
  // of the URL being waited for.
  const network::URLLoaderCompletionStatus& WaitForRequestCompletion(
      const GURL& url_to_wait_for_completion);
  void WaitForRequestCompletion(
      const std::set<GURL>& urls_to_wait_for_completion);

  // Clears all observed URL request info and completion status.
  void ClearRequests();

 private:
  bool OnRequest(content::URLLoaderInterceptor::RequestParams* params);

  void OnRequestComplete(const GURL& request_url,
                         const network::URLLoaderCompletionStatus& status);

  // This is needed to guard access to |resource_request_map_| and
  // |urls_to_wait_for_|, as content::URLLoaderInterceptor can invoke its
  // callback on both the UI and IO threads.
  base::Lock lock_;
  std::map<GURL, network::ResourceRequest> GUARDED_BY(lock_)
      resource_request_map_;
  std::map<GURL, network::URLLoaderCompletionStatus> GUARDED_BY(lock_)
      resource_completion_status_map_;
  std::set<GURL> GUARDED_BY(lock_) urls_to_wait_for_request_;
  base::OnceClosure GUARDED_BY(lock_) quit_request_run_loop_callback_;
  std::set<GURL> GUARDED_BY(lock_) urls_to_wait_for_completion_;
  base::OnceClosure GUARDED_BY(lock_) quit_completion_run_loop_callback_;

  std::unique_ptr<URLLoaderInterceptor> interceptor_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_URL_LOADER_MONITOR_H_
