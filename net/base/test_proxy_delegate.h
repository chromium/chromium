// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_PROXY_DELEGATE_H_
#define NET_BASE_TEST_PROXY_DELEGATE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"

class GURL;

namespace net {

class ProxyInfo;

class TestProxyDelegate : public ProxyDelegate {
 public:
  TestProxyDelegate();
  ~TestProxyDelegate() override;

  constexpr static char kTestHeaderName[] = "Foo";
  // Note: `kTestSpdyHeaderName` should be a lowercase version of
  // `kTestHeaderName`.
  constexpr static char kTestSpdyHeaderName[] = "foo";

  bool on_before_tunnel_request_called() const {
    return on_before_tunnel_request_called_;
  }

  size_t on_tunnel_headers_received_call_count() {
    return on_tunnel_headers_received_headers_.size();
  }

  void VerifyOnTunnelHeadersReceived(const ProxyChain& proxy_chain,
                                     size_t chain_index,
                                     const std::string& response_header_name,
                                     const std::string& response_header_value,
                                     size_t call_index = 0) const;

  // ProxyDelegate implementation:
  void OnResolveProxy(const GURL& url,
                      const NetworkAnonymizationKey& network_anonymization_key,
                      const std::string& method,
                      const ProxyRetryInfoMap& proxy_retry_info,
                      ProxyInfo* result) override;
  void OnFallback(const ProxyChain& bad_chain, int net_error) override;
  void OnBeforeTunnelRequest(const ProxyChain& proxy_chain,
                             size_t chain_index,
                             HttpRequestHeaders* extra_headers) override;
  Error OnTunnelHeadersReceived(
      const ProxyChain& proxy_chain,
      size_t chain_index,
      const HttpResponseHeaders& response_headers) override;

 private:
  bool on_before_tunnel_request_called_ = false;
  std::vector<ProxyChain> on_tunnel_headers_received_proxy_chains_;
  std::vector<size_t> on_tunnel_headers_received_chain_indices_;
  std::vector<scoped_refptr<HttpResponseHeaders>>
      on_tunnel_headers_received_headers_;
};

}  // namespace net

#endif  // NET_BASE_TEST_PROXY_DELEGATE_H_
