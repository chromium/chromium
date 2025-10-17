// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_proxy_delegate.h"

#include <optional>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TestProxyDelegate::TestProxyDelegate() = default;

TestProxyDelegate::~TestProxyDelegate() = default;

void TestProxyDelegate::set_proxy_chain(const ProxyChain& proxy_chain) {
  CHECK(proxy_chain.IsValid());
  proxy_chain_ = proxy_chain;
}

ProxyChain TestProxyDelegate::proxy_chain() const {
  CHECK(proxy_chain_) << "No proxy chain has been set via 'set_proxy_chain()'";
  return *proxy_chain_;
}
void TestProxyDelegate::set_proxy_list(const ProxyList& proxy_list) {
  proxy_list_ = proxy_list;
}

ProxyList TestProxyDelegate::proxy_list() const {
  CHECK(proxy_list_) << "No proxy list has been set via 'set_proxy_list()'";
  return *proxy_list_;
}

void TestProxyDelegate::MakeOnTunnelHeadersReceivedFail(Error result) {
  on_tunnel_headers_received_result_ = result;
}

void TestProxyDelegate::MaybeCreateOnBeforeTunnelRequestRunLoop() {
  if (on_before_tunnel_request_run_loop_) {
    return;
  }
  on_before_tunnel_request_run_loop_ = std::make_unique<base::RunLoop>();
}

void TestProxyDelegate::MakeOnBeforeTunnelRequestCompleteAsync() {
  CHECK(!on_before_tunnel_request_returns_async_);
  on_before_tunnel_request_returns_async_ = true;
}

void TestProxyDelegate::ResumeOnBeforeTunnelRequest() {
  CHECK(on_before_tunnel_request_returns_async_);
  CHECK(on_before_tunnel_request_callback_);
  std::move(on_before_tunnel_request_callback_).Run();
}

void TestProxyDelegate::WaitForOnBeforeTunnelRequestAsyncCompletion() {
  CHECK(on_before_tunnel_request_returns_async_);
  // We don't know whether WaitForOnBeforeTunnelRequestAsyncCompletion or
  // OnBeforeTunnelRequest will execute first. Allow creating the run loop in
  // both places to account for that.
  MaybeCreateOnBeforeTunnelRequestRunLoop();
  on_before_tunnel_request_run_loop_->Run();
  on_before_tunnel_request_run_loop_.reset();
}

void TestProxyDelegate::MaybeCreateOnTunnelHeadersReceivedRunLoop() {
  if (on_tunnel_headers_received_run_loop_) {
    return;
  }
  on_tunnel_headers_received_run_loop_ = std::make_unique<base::RunLoop>();
}

void TestProxyDelegate::MakeOnTunnelHeadersReceivedCompleteAsync() {
  CHECK(!on_tunnel_headers_received_returns_async_);
  on_tunnel_headers_received_returns_async_ = true;
}
void TestProxyDelegate::ResumeOnTunnelHeadersReceived() {
  CHECK(on_tunnel_headers_received_returns_async_);
  CHECK(on_tunnel_headers_received_callback_);
  std::move(on_tunnel_headers_received_callback_)
      .Run(on_tunnel_headers_received_result_);
}
void TestProxyDelegate::WaitForOnTunnelHeadersReceivedAsyncCompletion() {
  CHECK(on_tunnel_headers_received_returns_async_);
  // We don't know whether WaitForOnTunnelHeadersReceivedAsyncCompletion or
  // OnTunnelHeadersReceived will execute first. Allow creating the run loop in
  // both places to account for that.
  MaybeCreateOnTunnelHeadersReceivedRunLoop();
  on_tunnel_headers_received_run_loop_->Run();
  on_tunnel_headers_received_run_loop_.reset();
}

void TestProxyDelegate::VerifyOnTunnelHeadersReceived(
    const ProxyChain& proxy_chain,
    size_t proxy_index,
    const std::string& response_header_name,
    const std::string& response_header_value,
    size_t call_index) const {
  ASSERT_LT(call_index, on_tunnel_headers_received_proxy_chains_.size());
  ASSERT_EQ(on_tunnel_headers_received_proxy_chains_.size(),
            on_tunnel_headers_received_chain_indices_.size());
  ASSERT_EQ(on_tunnel_headers_received_proxy_chains_.size(),
            on_tunnel_headers_received_headers_.size());

  EXPECT_EQ(proxy_chain,
            on_tunnel_headers_received_proxy_chains_.at(call_index));
  EXPECT_EQ(proxy_index,
            on_tunnel_headers_received_chain_indices_.at(call_index));

  scoped_refptr<HttpResponseHeaders> response_headers =
      on_tunnel_headers_received_headers_.at(call_index);
  ASSERT_NE(response_headers.get(), nullptr);
  EXPECT_TRUE(response_headers->HasHeaderValue(response_header_name,
                                               response_header_value));
}

void TestProxyDelegate::OnResolveProxy(
    const GURL& url,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::string& method,
    const ProxyRetryInfoMap& proxy_retry_info,
    ProxyInfo* result) {
  if (proxy_list_) {
    CHECK(!proxy_chain_) << "Cannot set both a proxy list and a proxy chain.";
    result->OverrideProxyList(*proxy_list_);
  } else if (proxy_chain_) {
    result->UseProxyChain(*proxy_chain_);
  }
}

void TestProxyDelegate::OnSuccessfulRequestAfterFailures(
    const ProxyRetryInfoMap& proxy_retry_info) {}

std::optional<bool> TestProxyDelegate::CanFalloverToNextProxyOverride(
    const net::ProxyChain& proxy_chain,
    int net_error) {
  ++on_can_fallover_to_next_proxy_override_count_;
  return std::nullopt;
}

void TestProxyDelegate::OnFallback(const ProxyChain& bad_chain, int net_error) {
}

// static
std::string TestProxyDelegate::GetExtraHeaderValue(
    const ProxyServer& proxy_server) {
  return ProxyServerToProxyUri(proxy_server);
}

base::expected<HttpRequestHeaders, Error>
TestProxyDelegate::OnBeforeTunnelRequest(
    const ProxyChain& proxy_chain,
    size_t proxy_index,
    OnBeforeTunnelRequestCallback callback) {
  on_before_tunnel_request_call_count_++;

  HttpRequestHeaders extra_headers;
  if (extra_header_name_) {
    extra_headers.SetHeader(
        *extra_header_name_,
        GetExtraHeaderValue(proxy_chain.GetProxyServer(proxy_index)));
  }

  if (on_before_tunnel_request_returns_async_) {
    // We don't know whether OnBeforeTunnelRequest or
    // WaitForOnBeforeTunnelRequestAsyncCompletion will execute first. Allow
    // creating the run loop in both places to account for that.
    MaybeCreateOnBeforeTunnelRequestRunLoop();
    on_before_tunnel_request_run_loop_->Quit();
    on_before_tunnel_request_callback_ =
        base::BindOnce(std::move(callback), std::move(extra_headers));
    return base::unexpected(ERR_IO_PENDING);
  }

  return extra_headers;
}

Error TestProxyDelegate::OnTunnelHeadersReceived(
    const ProxyChain& proxy_chain,
    size_t proxy_index,
    const HttpResponseHeaders& response_headers,
    CompletionOnceCallback callback) {
  on_tunnel_headers_received_headers_.push_back(
      base::MakeRefCounted<HttpResponseHeaders>(
          response_headers.raw_headers()));

  on_tunnel_headers_received_proxy_chains_.push_back(proxy_chain);
  on_tunnel_headers_received_chain_indices_.push_back(proxy_index);

  if (on_tunnel_headers_received_returns_async_) {
    // We don't know whether OnBeforeTunnelRequest or
    // WaitForOnBeforeTunnelRequestAsyncCompletion will execute first. Allow
    // creating the run loop in both places to account for that.
    MaybeCreateOnTunnelHeadersReceivedRunLoop();
    on_tunnel_headers_received_run_loop_->Quit();
    on_tunnel_headers_received_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  return on_tunnel_headers_received_result_;
}

void TestProxyDelegate::SetProxyResolutionService(
    ProxyResolutionService* proxy_resolution_service) {}

bool TestProxyDelegate::AliasRequiresProxyOverride(
    const std::string scheme,
    const std::vector<std::string>& dns_aliases,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  return false;
}

}  // namespace net
