// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_navigation_url_loader_delegate.h"

#include <memory>

#include "base/run_loop.h"
#include "content/browser/loader/navigation_early_hints_manager.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/public/browser/global_request_id.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TestNavigationURLLoaderDelegate::TestNavigationURLLoaderDelegate()
    : net_error_(0), on_request_handled_counter_(0) {}

TestNavigationURLLoaderDelegate::~TestNavigationURLLoaderDelegate() {}

void TestNavigationURLLoaderDelegate::WaitForRequestRedirected() {
  request_redirected_ = std::make_unique<base::RunLoop>();
  request_redirected_->Run();
  request_redirected_.reset();
}

void TestNavigationURLLoaderDelegate::WaitForResponseStarted() {
  response_started_ = std::make_unique<base::RunLoop>();
  response_started_->Run();
  response_started_.reset();
}

void TestNavigationURLLoaderDelegate::WaitForRequestFailed() {
  request_failed_ = std::make_unique<base::RunLoop>();
  request_failed_->Run();
  request_failed_.reset();
}

void TestNavigationURLLoaderDelegate::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    network::mojom::URLResponseHeadPtr response_head) {
  redirect_info_ = redirect_info;
  redirect_response_ = std::move(response_head);
  ASSERT_TRUE(request_redirected_);
  request_redirected_->Quit();
}

void TestNavigationURLLoaderDelegate::OnResponseStarted(
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    GlobalRequestID request_id,
    bool is_download,
    net::NetworkAnonymizationKey network_anonymization_key,
    SubresourceLoaderParams subresource_loader_params,
    EarlyHints early_hints) {
  on_request_handled_counter_++;
  response_head_ = std::move(response_head);
  response_body_ = std::move(response_body);
  if (response_head_->ssl_info.has_value())
    ssl_info_ = *response_head_->ssl_info;
  if (response_started_)
    response_started_->Quit();
}

void TestNavigationURLLoaderDelegate::OnRequestFailed(
    const network::URLLoaderCompletionStatus& status) {
  on_request_handled_counter_++;
  net_error_ = status.error_code;
  if (status.ssl_info.has_value())
    ssl_info_ = status.ssl_info.value();
  if (request_failed_)
    request_failed_->Quit();
}

std::optional<NavigationEarlyHintsManagerParams>
TestNavigationURLLoaderDelegate::CreateNavigationEarlyHintsManagerParams(
    const network::mojom::EarlyHints& early_hints) {
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

}  // namespace content
