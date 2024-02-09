// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_DELEGATE_H_
#define CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_DELEGATE_H_

#include <memory>
#include <optional>

#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace base {
class RunLoop;
}

namespace content {

// Test implementation of NavigationURLLoaderDelegate to monitor navigation
// progress in the network stack.
class TestNavigationURLLoaderDelegate : public NavigationURLLoaderDelegate {
 public:
  TestNavigationURLLoaderDelegate();

  TestNavigationURLLoaderDelegate(const TestNavigationURLLoaderDelegate&) =
      delete;
  TestNavigationURLLoaderDelegate& operator=(
      const TestNavigationURLLoaderDelegate&) = delete;

  ~TestNavigationURLLoaderDelegate() override;

  const net::RedirectInfo& redirect_info() const { return redirect_info_; }
  int net_error() const { return net_error_; }
  const net::SSLInfo& ssl_info() const { return ssl_info_; }
  int on_request_handled_counter() const { return on_request_handled_counter_; }

  // Waits for various navigation events.
  // Note: if the event already happened, the functions will hang.
  // TODO(clamy): Make the functions not hang if they are called after the
  // event happened.
  void WaitForRequestRedirected();
  void WaitForResponseStarted();
  void WaitForRequestFailed();

  // NavigationURLLoaderDelegate implementation.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::URLResponseHeadPtr response) override;
  void OnResponseStarted(
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      GlobalRequestID request_id,
      bool is_download,
      net::NetworkAnonymizationKey network_anonymization_key,
      SubresourceLoaderParams subresource_loader_params,
      EarlyHints early_hints) override;
  void OnRequestFailed(
      const network::URLLoaderCompletionStatus& status) override;
  std::optional<NavigationEarlyHintsManagerParams>
  CreateNavigationEarlyHintsManagerParams(
      const network::mojom::EarlyHints& early_hints) override;

 private:
  net::RedirectInfo redirect_info_;
  network::mojom::URLResponseHeadPtr redirect_response_;
  network::mojom::URLResponseHeadPtr response_head_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  int net_error_;
  net::SSLInfo ssl_info_;
  int on_request_handled_counter_;

  std::unique_ptr<base::RunLoop> request_redirected_;
  std::unique_ptr<base::RunLoop> response_started_;
  std::unique_ptr<base::RunLoop> request_failed_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_DELEGATE_H_
