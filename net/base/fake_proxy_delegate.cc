// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/fake_proxy_delegate.h"

#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy_resolution/proxy_info.h"

namespace net {

FakeProxyDelegate::FakeProxyDelegate() = default;
FakeProxyDelegate::~FakeProxyDelegate() = default;

void FakeProxyDelegate::OnResolveProxy(
    const GURL& url,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::string& method,
    const ProxyRetryInfoMap& proxy_retry_info,
    ProxyInfo* result) {
  on_resolve_proxy_call_count_++;
  last_proxy_retry_info_ = proxy_retry_info;

  if (should_modify_result_) {
    result->UseProxyList(override_proxy_list_);
  }
}

void FakeProxyDelegate::OnSuccessfulRequestAfterFailures(
    const ProxyRetryInfoMap& proxy_retry_info) {
  on_successful_request_after_failures_call_count_++;
  last_successful_request_retry_info_ = proxy_retry_info;
}

void FakeProxyDelegate::OnFallback(const ProxyChain& bad_chain,
                                   int net_error) {
  on_fallback_call_count_++;
  last_fallback_chain_ = bad_chain;
  last_fallback_net_error_ = net_error;
}

base::expected<HttpRequestHeaders, Error>
FakeProxyDelegate::OnBeforeTunnelRequest(
    const ProxyChain& proxy_chain,
    size_t proxy_index,
    OnBeforeTunnelRequestCallback callback) {
  // Callback intentionally ignored; returns synchronously for simplicity.
  return HttpRequestHeaders();
}

Error FakeProxyDelegate::OnTunnelHeadersReceived(
    const ProxyChain& proxy_chain,
    size_t proxy_index,
    const HttpResponseHeaders& response_headers,
    CompletionOnceCallback callback) {
  // Callback intentionally ignored; returns synchronously for simplicity.
  return OK;
}

void FakeProxyDelegate::SetProxyResolutionService(
    ProxyResolutionService* proxy_resolution_service) {
  // Not tracked.
}

}  // namespace net
