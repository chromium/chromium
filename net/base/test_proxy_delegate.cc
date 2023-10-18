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
    const std::string& response_header_value) const {
  EXPECT_EQ(proxy_chain, on_tunnel_headers_received_proxy_chain_);
  EXPECT_EQ(chain_index, on_tunnel_headers_received_chain_index_);
  ASSERT_NE(on_tunnel_headers_received_headers_.get(), nullptr);
  EXPECT_TRUE(on_tunnel_headers_received_headers_->HasHeaderValue(
      response_header_name, response_header_value));
}

void TestProxyDelegate::OnResolveProxy(
    const GURL& url,
    const GURL& top_frame_url,
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
    // TODO(crbug.com/1491092): Include the entire chain in the header.
    extra_headers->SetHeader("Foo",
                             ProxyServerToProxyUri(proxy_chain.proxy_server()));
  }
}

Error TestProxyDelegate::OnTunnelHeadersReceived(
    const ProxyChain& proxy_chain,
    size_t chain_index,
    const HttpResponseHeaders& response_headers) {
  EXPECT_EQ(on_tunnel_headers_received_headers_.get(), nullptr);
  on_tunnel_headers_received_headers_ =
      base::MakeRefCounted<HttpResponseHeaders>(response_headers.raw_headers());

  on_tunnel_headers_received_proxy_chain_ = proxy_chain;
  on_tunnel_headers_received_chain_index_ = chain_index;
  return OK;
}

}  // namespace net
