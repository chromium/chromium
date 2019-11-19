// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CLIENT_H_
#define NET_DNS_DNS_CLIENT_H_

#include <memory>

#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/rand_callback.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_overrides.h"
#include "net/dns/dns_hosts.h"
#include "net/url_request/url_request_context.h"

namespace net {

class AddressSorter;
class ClientSocketFactory;
class DnsTransactionFactory;
class NetLog;

// Entry point for HostResolverManager to interact with the built-in async
// resolver, as implemented by DnsTransactionFactory. Manages configuration and
// status of the resolver.
class NET_EXPORT DnsClient {
 public:
  static const int kMaxInsecureFallbackFailures = 16;

  virtual ~DnsClient() {}

  // Returns true if the DnsClient is able and allowed to make secure DNS
  // transactions. If false, secure transactions should not be created.
  virtual bool CanUseSecureDnsTransactions() const = 0;

  // Returns true if the DnsClient is able and allowed to make insecure DNS
  // transactions. If false, insecure transactions should not be created. Will
  // always be false unless SetInsecureEnabled(true) has been called.
  virtual bool CanUseInsecureDnsTransactions() const = 0;
  virtual void SetInsecureEnabled(bool enabled) = 0;

  // When true, DoH should not be used in AUTOMATIC mode since no DoH servers
  // have a successful probe state.
  virtual bool FallbackFromSecureTransactionPreferred() const = 0;

  // When true, insecure DNS transactions should not be used when reasonable
  // fallback alternatives, e.g. system resolution can be used instead.
  virtual bool FallbackFromInsecureTransactionPreferred() const = 0;

  // Updates DNS config.  If effective config has changed, destroys the current
  // DnsTransactionFactory and creates a new one according to the effective
  // config, unless it is invalid or has |unhandled_options|.
  //
  // Returns whether or not the effective config changed.
  virtual bool SetSystemConfig(base::Optional<DnsConfig> system_config) = 0;
  virtual bool SetConfigOverrides(DnsConfigOverrides config_overrides) = 0;

  // Retrieve the current DNS configuration that would be used if transactions
  // were otherwise currently allowed. Returns null if configuration is
  // invalid or a configuration has not yet been read from the system.
  virtual const DnsConfig* GetEffectiveConfig() const = 0;
  virtual const DnsHosts* GetHosts() const = 0;

  // Enables DoH probes to be sent using |url_request_context| whenever the DNS
  // configuration contains DoH servers. Currently only allows one probe
  // activation at a time. Must be cancelled before activating another.
  virtual void ActivateDohProbes(URLRequestContext* url_request_context) = 0;
  virtual void CancelDohProbes() = 0;

  // Returns null if the current config is not valid.
  virtual DnsTransactionFactory* GetTransactionFactory() = 0;

  virtual AddressSorter* GetAddressSorter() = 0;

  virtual void IncrementInsecureFallbackFailures() = 0;
  virtual void ClearInsecureFallbackFailures() = 0;

  virtual base::Optional<DnsConfig> GetSystemConfigForTesting() const = 0;
  virtual DnsConfigOverrides GetConfigOverridesForTesting() const = 0;

  virtual void SetProbeSuccessForTest(unsigned index, bool success) = 0;

  virtual void SetTransactionFactoryForTesting(
      std::unique_ptr<DnsTransactionFactory> factory) = 0;

  // Creates default client.
  static std::unique_ptr<DnsClient> CreateClient(NetLog* net_log);

  // Creates a client for testing.  Allows using a mock ClientSocketFactory and
  // a deterministic random number generator. |socket_factory| must outlive
  // the returned DnsClient.
  static std::unique_ptr<DnsClient> CreateClientForTesting(
      NetLog* net_log,
      ClientSocketFactory* socket_factory,
      const RandIntCallback& rand_int_callback);
};

}  // namespace net

#endif  // NET_DNS_DNS_CLIENT_H_
