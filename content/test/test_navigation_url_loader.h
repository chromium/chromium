// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_H_
#define CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class NavigationURLLoaderDelegate;

// Test implementation of NavigationURLLoader to simulate the network stack
// response.
class TestNavigationURLLoader final : public NavigationURLLoader {
 public:
  TestNavigationURLLoader(std::unique_ptr<NavigationRequestInfo> request_info,
                          NavigationURLLoaderDelegate* delegate,
                          NavigationURLLoader::LoaderType loader_type);

  // NavigationURLLoader implementation.
  void Start() override;
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers) override;
  bool SetNavigationTimeout(base::TimeDelta timeout) override;
  void CancelNavigationTimeout() override;

  NavigationRequestInfo* request_info() const { return request_info_.get(); }

  void SimulateServerRedirect(const GURL& redirect_url);

  void SimulateError(int error_code);
  void SimulateErrorWithStatus(
      const network::URLLoaderCompletionStatus& status);

  void SimulateEarlyHintsPreloadLinkHeaderReceived();

  void CallOnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head);
  void CallOnResponseStarted(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      std::optional<mojo_base::BigBuffer> cached_metadata);

  int redirect_count() { return redirect_count_; }

  base::WeakPtr<TestNavigationURLLoader> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  ~TestNavigationURLLoader() override;

  std::unique_ptr<NavigationRequestInfo> request_info_;
  raw_ptr<NavigationURLLoaderDelegate> delegate_;
  int redirect_count_;

  const NavigationURLLoader::LoaderType loader_type_;

  bool was_resource_hints_received_ = false;

  base::WeakPtrFactory<TestNavigationURLLoader> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_H_
