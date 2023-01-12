// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_MOCK_PROXY_HOST_RESOLVER_H_
#define SERVICES_PROXY_RESOLVER_MOCK_PROXY_HOST_RESOLVER_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "net/base/ip_address.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "services/proxy_resolver/proxy_host_resolver.h"

namespace net {
class NetworkAnonymizationKey;
}  // namespace net

namespace proxy_resolver {

// Mock of ProxyHostResolver that resolves by default to 127.0.0.1, except for
// hostnames with more specific results set using SetError() or SetResult().
// Also allows returning failure for all results with FailAll().
class MockProxyHostResolver : public ProxyHostResolver {
 public:
  // If |synchronous_mode| set to |true|, all results will be returned
  // synchronously.  Otherwise, all results will be asynchronous.
  explicit MockProxyHostResolver(bool synchronous_mode = false);
  ~MockProxyHostResolver() override;

  std::unique_ptr<Request> CreateRequest(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkAnonymizationKey& network_anonymization_key) override;

  void SetError(const std::string& hostname,
                net::ProxyResolveDnsOperation operation,
                const net::NetworkAnonymizationKey& network_anonymization_key);

  void SetResult(const std::string& hostname,
                 net::ProxyResolveDnsOperation operation,
                 const net::NetworkAnonymizationKey& network_anonymization_key,
                 std::vector<net::IPAddress> result);

  void FailAll();

  unsigned num_resolve() const { return num_resolve_; }

 private:
  using ResultKey = std::tuple<std::string,
                               net::ProxyResolveDnsOperation,
                               net::NetworkAnonymizationKey>;

  class RequestImpl;

  // Any entry with an empty value signifies an ERR_NAME_NOT_RESOLVED result.
  std::map<ResultKey, std::vector<net::IPAddress>> results_;
  unsigned num_resolve_;
  bool fail_all_;
  bool synchronous_mode_;
};

// Mock of ProxyHostResolver that always hangs until cancelled.
class HangingProxyHostResolver : public ProxyHostResolver {
 public:
  // If not null, |hang_callback| will be invoked whenever a request is started.
  explicit HangingProxyHostResolver(
      base::RepeatingClosure hang_callback = base::RepeatingClosure());
  ~HangingProxyHostResolver() override;

  std::unique_ptr<Request> CreateRequest(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkAnonymizationKey& network_anonymization_key) override;

  int num_cancelled_requests() const { return num_cancelled_requests_; }

  void set_hang_callback(base::RepeatingClosure hang_callback) {
    hang_callback_ = hang_callback;
  }

 private:
  class RequestImpl;

  int num_cancelled_requests_;
  base::RepeatingClosure hang_callback_;
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_MOCK_PROXY_HOST_RESOLVER_H_
