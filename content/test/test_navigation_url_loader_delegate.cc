// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_navigation_url_loader_delegate.h"

#include "base/run_loop.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/global_request_id.h"
#include "services/network/public/cpp/resource_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TestNavigationURLLoaderDelegate::TestNavigationURLLoaderDelegate()
    : net_error_(0), on_request_handled_counter_(0) {}

TestNavigationURLLoaderDelegate::~TestNavigationURLLoaderDelegate() {}

void TestNavigationURLLoaderDelegate::WaitForRequestRedirected() {
  request_redirected_.reset(new base::RunLoop);
  request_redirected_->Run();
  request_redirected_.reset();
}

void TestNavigationURLLoaderDelegate::WaitForResponseStarted() {
  response_started_.reset(new base::RunLoop);
  response_started_->Run();
  response_started_.reset();
}

void TestNavigationURLLoaderDelegate::WaitForRequestFailed() {
  request_failed_.reset(new base::RunLoop);
  request_failed_->Run();
  request_failed_.reset();
}

void TestNavigationURLLoaderDelegate::WaitForRequestStarted() {
  request_started_.reset(new base::RunLoop);
  request_started_->Run();
  request_started_.reset();
}

void TestNavigationURLLoaderDelegate::ReleaseURLLoaderClientEndpoints() {
  url_loader_client_endpoints_ = nullptr;
  response_body_.reset();
}

void TestNavigationURLLoaderDelegate::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<network::ResourceResponse>& response_head) {
  redirect_info_ = redirect_info;
  redirect_response_ = response_head;
  ASSERT_TRUE(request_redirected_);
  request_redirected_->Quit();
}

void TestNavigationURLLoaderDelegate::OnResponseStarted(
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    const scoped_refptr<network::ResourceResponse>& response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    const GlobalRequestID& request_id,
    bool is_download,
    NavigationDownloadPolicy download_policy,
    base::Optional<SubresourceLoaderParams> subresource_loader_params) {
  response_head_ = response_head;
  response_body_ = std::move(response_body);
  url_loader_client_endpoints_ = std::move(url_loader_client_endpoints);
  if (response_head->head.ssl_info.has_value())
    ssl_info_ = *response_head->head.ssl_info;
  is_download_ = is_download && download_policy.IsDownloadAllowed();
  if (response_started_)
    response_started_->Quit();
}

void TestNavigationURLLoaderDelegate::OnRequestFailed(
    const network::URLLoaderCompletionStatus& status) {
  net_error_ = status.error_code;
  if (status.ssl_info.has_value())
    ssl_info_ = status.ssl_info.value();
  if (request_failed_)
    request_failed_->Quit();
}

void TestNavigationURLLoaderDelegate::OnRequestStarted(
    base::TimeTicks timestamp) {
  ASSERT_FALSE(timestamp.is_null());
  ++on_request_handled_counter_;
  if (request_started_)
    request_started_->Quit();
}

}  // namespace content
