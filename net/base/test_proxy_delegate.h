// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_PROXY_DELEGATE_H_
#define NET_BASE_TEST_PROXY_DELEGATE_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"

class GURL;

namespace net {

class ProxyInfo;

class TestProxyDelegate : public ProxyDelegate {
 public:
  TestProxyDelegate();
  ~TestProxyDelegate() override;

  bool on_before_tunnel_request_called() const {
    return on_before_tunnel_request_called_;
  }

  void VerifyOnTunnelHeadersReceived(
      const ProxyServer& proxy_server,
      const std::string& response_header_name,
      const std::string& response_header_value) const;

  // ProxyDelegate implementation:
  void OnResolveProxy(const GURL& url,
                      const GURL& top_frame_url,
                      const std::string& method,
                      const ProxyRetryInfoMap& proxy_retry_info,
                      ProxyInfo* result) override;
  void OnFallback(const ProxyServer& bad_proxy, int net_error) override;
  void OnBeforeTunnelRequest(const ProxyServer& proxy_server,
                             HttpRequestHeaders* extra_headers) override;
  Error OnTunnelHeadersReceived(
      const ProxyServer& proxy_server,
      const HttpResponseHeaders& response_headers) override;

 private:
  bool on_before_tunnel_request_called_ = false;
  ProxyServer on_tunnel_headers_received_proxy_server_;
  scoped_refptr<HttpResponseHeaders> on_tunnel_headers_received_headers_;
};

}  // namespace net

#endif  // NET_BASE_TEST_PROXY_DELEGATE_H_
