// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_H_
#define CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/common/navigation_params.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class NavigationURLLoaderDelegate;

// Test implementation of NavigationURLLoader to simulate the network stack
// response.
class TestNavigationURLLoader
    : public NavigationURLLoader,
      public base::SupportsWeakPtr<TestNavigationURLLoader> {
 public:
  TestNavigationURLLoader(std::unique_ptr<NavigationRequestInfo> request_info,
                          NavigationURLLoaderDelegate* delegate,
                          bool is_served_from_back_forward_cache);

  // NavigationURLLoader implementation.
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      blink::PreviewsState new_previews_state) override;

  NavigationRequestInfo* request_info() const { return request_info_.get(); }

  void SimulateServerRedirect(const GURL& redirect_url);

  void SimulateError(int error_code);
  void SimulateErrorWithStatus(
      const network::URLLoaderCompletionStatus& status);

  void CallOnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head);
  void CallOnResponseStarted(network::mojom::URLResponseHeadPtr response_head);

  int redirect_count() { return redirect_count_; }

 private:
  ~TestNavigationURLLoader() override;

  std::unique_ptr<NavigationRequestInfo> request_info_;
  NavigationURLLoaderDelegate* delegate_;
  int redirect_count_;

  bool is_served_from_back_forward_cache_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_H_
