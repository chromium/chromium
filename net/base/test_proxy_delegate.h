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
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_list.h"

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
  // Allow setting the proxy list directly, rather than via `set_proxy_chain()`.
  void set_proxy_list(const ProxyList& proxy_list);
  ProxyList proxy_list() const;

  // Setter for the name of a header to add to the tunnel request. The value of
  // the header will be based on the `OnBeforeTunnelRequest()` `proxy_chain` and
  // `proxy_index` parameters. If no extra header name is provided, no extra
  // header will be added to the tunnel request.
  void set_extra_header_name(std::string_view extra_header_name) {
    extra_header_name_ = extra_header_name;
  }

  // Returns the header value that may be added to a tunnel request for a given
  // proxy server. For more info, see `set_extra_header_name()`.
  static std::string GetExtraHeaderValue(const ProxyServer& proxy_server);

  // Returns the number of times `CanFalloverToNextProxyOverride()` was called.
  size_t on_can_fallover_to_next_proxy_override_count() const {
    return on_can_fallover_to_next_proxy_override_count_;
  }

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

  // Makes subsequent calls to `OnBeforeTunnelRequest()` return ERR_IO_PENDING.
  // Test code should then call `ResumeOnBeforeTunnelRequest()` to continue from
  // where it left off.
  void MakeOnBeforeTunnelRequestCompleteAsync();
  // Resumes the execution of `OnBeforeTunnelRequest()` from where it was left
  // off. Callers must have called `MakeOnBeforeTunnelRequestCompleteAsync()`
  // first.
  void ResumeOnBeforeTunnelRequest();
  // Allows blocking until the `OnBeforeTunnelRequest()` has completed
  // asynchronously. Callers must have called
  // `MakeOnBeforeTunnelRequestCompleteAsync` first.
  void WaitForOnBeforeTunnelRequestAsyncCompletion();

  // Makes subsequent calls to `OnTunnelHeadersReceived()` return
  // ERR_IO_PENDING. Test code should then call
  // `ResumeOnTunnelHeadersReceived()` to continue from where it left off.
  void MakeOnTunnelHeadersReceivedCompleteAsync();
  // Resumes the execution of `OnTunnelHeadersReceived()` from where it was left
  // off. Callers must have called `MakeOnTunnelHeadersReceivedCompleteAsync()`
  // first.
  void ResumeOnTunnelHeadersReceived();
  // Allows blocking until the `OnTunnelHeadersReceived()` has completed
  // asynchronously. Callers must have called
  // `MakeOnTunnelHeadersReceivedCompleteAsync()` first.
  void WaitForOnTunnelHeadersReceivedAsyncCompletion();

  // Checks whether the provided proxy chain, chain index, response header name,
  // and response header value were passed to a given
  // `OnTunnelHeadersReceived()` call.
  void VerifyOnTunnelHeadersReceived(const ProxyChain& proxy_chain,
                                     size_t proxy_index,
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
  std::optional<bool> CanFalloverToNextProxyOverride(
      const net::ProxyChain& proxy_chain,
      int net_error) override;
  void OnFallback(const ProxyChain& bad_chain, int net_error) override;
  base::expected<HttpRequestHeaders, Error> OnBeforeTunnelRequest(
      const ProxyChain& proxy_chain,
      size_t proxy_index,
      OnBeforeTunnelRequestCallback callback) override;
  Error OnTunnelHeadersReceived(const ProxyChain& proxy_chain,
                                size_t proxy_index,
                                const HttpResponseHeaders& response_headers,
                                CompletionOnceCallback callback) override;
  void SetProxyResolutionService(
      ProxyResolutionService* proxy_resolution_service) override;
  bool AliasRequiresProxyOverride(
      const std::string scheme,
      const std::vector<std::string>& dns_aliases,
      const net::NetworkAnonymizationKey& network_anonymization_key) override;

 private:
  // Creates an internal run loop to allow waiting for the asynchronous
  // completion of `OnBeforeTunnelCallback()`. Callers must have called
  // `MakeOnBeforeTunnelRequestCompleteAsync()` first. We need to create the run
  // loop multiple times to handle proxy chains: if multiple proxies are nested,
  // `OnBeforeTunnelRequest()` will be called multiple times. As such, test code
  // might want to wait for each of these calls.
  void MaybeCreateOnBeforeTunnelRequestRunLoop();

  // Creates an internal run loop to allow waiting for the asynchronous
  // completion of `OnTunnelHeadersReceived()`. Callers must have called
  // `MakeOnTunnelHeadersReceivedCompleteAsync()` first. We need to create the
  // run loop multiple times to handle proxy chains: if multiple proxies are
  // nested, `OnTunnelHeadersReceived()` will be called multiple times. As such,
  // test code might want to wait for each of these calls.
  void MaybeCreateOnTunnelHeadersReceivedRunLoop();

  std::optional<ProxyChain> proxy_chain_;
  std::optional<ProxyList> proxy_list_;
  std::optional<std::string> extra_header_name_;

  size_t on_can_fallover_to_next_proxy_override_count_ = 0;

  size_t on_before_tunnel_request_call_count_ = 0;

  Error on_tunnel_headers_received_result_ = OK;
  std::vector<ProxyChain> on_tunnel_headers_received_proxy_chains_;
  std::vector<size_t> on_tunnel_headers_received_chain_indices_;
  std::vector<scoped_refptr<HttpResponseHeaders>>
      on_tunnel_headers_received_headers_;

  bool on_before_tunnel_request_returns_async_ = false;
  std::unique_ptr<base::RunLoop> on_before_tunnel_request_run_loop_;
  base::OnceClosure on_before_tunnel_request_callback_;

  bool on_tunnel_headers_received_returns_async_ = false;
  std::unique_ptr<base::RunLoop> on_tunnel_headers_received_run_loop_;
  net::CompletionOnceCallback on_tunnel_headers_received_callback_;
};

}  // namespace net

#endif  // NET_BASE_TEST_PROXY_DELEGATE_H_
