// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_PROXY_DELEGATE_H_
#define NET_BASE_TEST_PROXY_DELEGATE_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"

class GURL;

namespace net {

class ProxyInfo;
class ProxyResolutionService;

class TestProxyDelegate : public ProxyDelegate {
 public:
  TestProxyDelegate();
  ~TestProxyDelegate() override;

  // Setter and getter for the proxy chain to use for a given URL when
  // `OnResolveProxy()` is called. Attempting to get the proxy chain when one
  // hasn't been set will result in a crash.
  void set_proxy_chain(const ProxyChain& proxy_chain);
  ProxyChain proxy_chain() const;

  // Setter for the name of a header to add to the tunnel request. The value of
  // the header will be based on the `OnBeforeTunnelRequest()` `proxy_chain` and
  // `chain_index` parameters. If no extra header name is provided, no extra
  // header will be added to the tunnel request.
  void set_extra_header_name(std::string_view extra_header_name) {
    extra_header_name_ = extra_header_name;
  }

  // Returns the header value that may be added to a tunnel request for a given
  // proxy server. For more info, see `set_extra_header_name()`.
  static std::string GetExtraHeaderValue(const ProxyServer& proxy_server);

  // Returns the number of times `OnBeforeTunnelRequest()` was called.
  size_t on_before_tunnel_request_call_count() const {
    return on_before_tunnel_request_call_count_;
  }

  // Returns the number of times `OnTunnelHeadersReceived()` was called.
  size_t on_tunnel_headers_received_call_count() {
    return on_tunnel_headers_received_headers_.size();
  }

  // Make subsequent calls to `OnTunnelHeadersReceived()` fail with the given
  // value.
  void MakeOnTunnelHeadersReceivedFail(Error result);

  // Checks whether the provided proxy chain, chain index, response header name,
  // and response header value were passed to a given
  // `OnTunnelHeadersReceived()` call.
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
  void OnSuccessfulRequestAfterFailures(
      const ProxyRetryInfoMap& proxy_retry_info) override;
  void OnFallback(const ProxyChain& bad_chain, int net_error) override;
  Error OnBeforeTunnelRequest(const ProxyChain& proxy_chain,
                              size_t chain_index,
                              HttpRequestHeaders* extra_headers) override;
  Error OnTunnelHeadersReceived(
      const ProxyChain& proxy_chain,
      size_t chain_index,
      const HttpResponseHeaders& response_headers) override;
  void SetProxyResolutionService(
      ProxyResolutionService* proxy_resolution_service) override;

 private:
  std::optional<ProxyChain> proxy_chain_;
  std::optional<std::string> extra_header_name_;

  size_t on_before_tunnel_request_call_count_ = 0;

  Error on_tunnel_headers_received_result_ = OK;
  std::vector<ProxyChain> on_tunnel_headers_received_proxy_chains_;
  std::vector<size_t> on_tunnel_headers_received_chain_indices_;
  std::vector<scoped_refptr<HttpResponseHeaders>>
      on_tunnel_headers_received_headers_;
};

}  // namespace net

#endif  // NET_BASE_TEST_PROXY_DELEGATE_H_
