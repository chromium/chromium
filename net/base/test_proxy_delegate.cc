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
    const ProxyServer& proxy_server,
    const std::string& response_header_name,
    const std::string& response_header_value) const {
  EXPECT_EQ(proxy_server, on_tunnel_headers_received_proxy_server_);
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

void TestProxyDelegate::OnFallback(const ProxyServer& bad_proxy,
                                   int net_error) {}

void TestProxyDelegate::OnBeforeTunnelRequest(
    const ProxyServer& proxy_server,
    HttpRequestHeaders* extra_headers) {
  on_before_tunnel_request_called_ = true;
  if (extra_headers)
    extra_headers->SetHeader("Foo", ProxyServerToProxyUri(proxy_server));
}

Error TestProxyDelegate::OnTunnelHeadersReceived(
    const ProxyServer& proxy_server,
    const HttpResponseHeaders& response_headers) {
  EXPECT_EQ(on_tunnel_headers_received_headers_.get(), nullptr);
  on_tunnel_headers_received_headers_ =
      base::MakeRefCounted<HttpResponseHeaders>(response_headers.raw_headers());

  on_tunnel_headers_received_proxy_server_ = proxy_server;
  return OK;
}

}  // namespace net
