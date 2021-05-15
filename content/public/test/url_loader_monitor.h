// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_URL_LOADER_MONITOR_H_
#define CONTENT_PUBLIC_TEST_URL_LOADER_MONITOR_H_

#include <map>
#include <memory>
#include <set>

#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/public/test/url_loader_interceptor.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class RunLoop;
}

namespace content {

// Helper class to monitor parameters passed to URLLoaderFactory calls for
// tests. Records parameters of most recent request for each requested URL.
// URLLoaderMonitor starts watching requested URLs as soon as it's constructed.
class URLLoaderMonitor {
 public:
  // If |urls_to_wait_for| to non-null, WaitForUrls() may be invoked once to
  // wait for the specified URLs to all be observed.
  explicit URLLoaderMonitor(std::set<GURL> urls_to_wait_for = {});
  URLLoaderMonitor(const URLLoaderMonitor&) = delete;
  URLLoaderMonitor& operator=(const URLLoaderMonitor&) = delete;
  ~URLLoaderMonitor();

  // Returns the network::ResourceRequest for the most recently observed request
  // to |url|. If no such request has been observed, returns nullptr.
  absl::optional<network::ResourceRequest> GetRequestInfo(const GURL& url);

  // Waits for the URLs passed in to the constructor to all be observers. All
  // URLs observed after the constructor is invoked are counted.
  void WaitForUrls();

 private:
  bool OnRequest(content::URLLoaderInterceptor::RequestParams* params);

  // This is needed to guard access to |resource_request_map_| and
  // |urls_to_wait_for_|, as content::URLLoaderInterceptor can invoke its
  // callback on both the UI and IO threads.
  base::Lock lock_;
  std::map<GURL, network::ResourceRequest> GUARDED_BY(lock_)
      resource_request_map_;
  std::set<GURL> GUARDED_BY(lock_) urls_to_wait_for_;

  base::RunLoop run_loop_;

  std::unique_ptr<URLLoaderInterceptor> interceptor_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_URL_LOADER_MONITOR_H_
