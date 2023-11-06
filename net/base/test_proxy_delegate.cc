// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_proxy_delegate.h"

#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TestProxyDelegate::TestProxyDelegate() = default;

TestProxyDelegate::~TestProxyDelegate() = default;

void TestProxyDelegate::VerifyOnTunnelHeadersReceived(
    const ProxyChain& proxy_chain,
    size_t chain_index,
    const std::string& response_header_name,
    const std::string& response_header_value,
    size_t call_index) const {
  ASSERT_LT(call_index, on_tunnel_headers_received_proxy_chains_.size());
  ASSERT_LT(call_index, on_tunnel_headers_received_chain_indices_.size());
  ASSERT_LT(call_index, on_tunnel_headers_received_headers_.size());

  EXPECT_EQ(proxy_chain,
            on_tunnel_headers_received_proxy_chains_.at(call_index));
  EXPECT_EQ(chain_index,
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
    ProxyInfo* result) {}

void TestProxyDelegate::OnFallback(const ProxyChain& bad_chain, int net_error) {
}

void TestProxyDelegate::OnBeforeTunnelRequest(
    const ProxyChain& proxy_chain,
    size_t chain_index,
    HttpRequestHeaders* extra_headers) {
  on_before_tunnel_request_called_ = true;
  if (extra_headers) {
    extra_headers->SetHeader(
        kTestHeaderName,
        ProxyServerToProxyUri(proxy_chain.GetProxyServer(chain_index)));
  }
}

Error TestProxyDelegate::OnTunnelHeadersReceived(
    const ProxyChain& proxy_chain,
    size_t chain_index,
    const HttpResponseHeaders& response_headers) {
  on_tunnel_headers_received_headers_.push_back(
      base::MakeRefCounted<HttpResponseHeaders>(
          response_headers.raw_headers()));

  on_tunnel_headers_received_proxy_chains_.push_back(proxy_chain);
  on_tunnel_headers_received_chain_indices_.push_back(chain_index);
  return OK;
}

}  // namespace net
