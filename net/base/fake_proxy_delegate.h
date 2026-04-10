// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FAKE_PROXY_DELEGATE_H_
#define NET_BASE_FAKE_PROXY_DELEGATE_H_

#include <string>

#include "base/types/expected.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "url/gurl.h"

namespace net {

// A minimal fake ProxyDelegate for testing that tracks call counts and last
// arguments for OnResolveProxy, OnFallback, and
// OnSuccessfulRequestAfterFailures, and optionally modifies proxy results.
//
// Unlike TestProxyDelegate (which supports async tunnel request/response
// flows, header injection, and proxy chain overrides), FakeProxyDelegate
// focuses on verifying proxy retry info interactions in
// SystemProxyResolutionService typed tests.
class FakeProxyDelegate : public ProxyDelegate {
 public:
  FakeProxyDelegate();
  ~FakeProxyDelegate() override;

  // ProxyDelegate implementation:
  void OnResolveProxy(const GURL& url,
                      const NetworkAnonymizationKey& network_anonymization_key,
                      const std::string& method,
                      const ProxyRetryInfoMap& proxy_retry_info,
                      ProxyInfo* result) override;
  void OnSuccessfulRequestAfterFailures(
      const ProxyRetryInfoMap& proxy_retry_info) override;
  void OnFallback(const ProxyChain& bad_chain, int net_error) override;
  base::expected<HttpRequestHeaders, Error> OnBeforeTunnelRequest(
      const ProxyChain& proxy_chain,
      size_t proxy_index,
      OnBeforeTunnelRequestCallback callback) override;
  Error OnTunnelHeadersReceived(
      const ProxyChain& proxy_chain,
      size_t proxy_index,
      const HttpResponseHeaders& response_headers,
      CompletionOnceCallback callback) override;
  void SetProxyResolutionService(
      ProxyResolutionService* proxy_resolution_service) override;

  // Configuration methods for testing.
  void set_should_modify_result(bool should_modify) {
    should_modify_result_ = should_modify;
  }
  void set_override_proxy_list(const ProxyList& proxy_list) {
    override_proxy_list_ = proxy_list;
  }

  // Test accessors.
  size_t on_resolve_proxy_call_count() const {
    return on_resolve_proxy_call_count_;
  }
  size_t on_successful_request_after_failures_call_count() const {
    return on_successful_request_after_failures_call_count_;
  }
  size_t on_fallback_call_count() const { return on_fallback_call_count_; }
  const ProxyRetryInfoMap& last_proxy_retry_info() const {
    return last_proxy_retry_info_;
  }
  const ProxyRetryInfoMap& last_successful_request_retry_info() const {
    return last_successful_request_retry_info_;
  }
  const ProxyChain& last_fallback_chain() const { return last_fallback_chain_; }
  int last_fallback_net_error() const { return last_fallback_net_error_; }

 private:
  size_t on_resolve_proxy_call_count_ = 0;
  size_t on_successful_request_after_failures_call_count_ = 0;
  size_t on_fallback_call_count_ = 0;
  ProxyRetryInfoMap last_proxy_retry_info_;
  ProxyRetryInfoMap last_successful_request_retry_info_;
  ProxyChain last_fallback_chain_;
  int last_fallback_net_error_ = OK;

  bool should_modify_result_ = false;
  ProxyList override_proxy_list_;
};

}  // namespace net

#endif  // NET_BASE_FAKE_PROXY_DELEGATE_H_
