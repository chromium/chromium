// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_PLATFORM_ATTEMPT_FACTORY_H_
#define NET_DNS_DNS_PLATFORM_ATTEMPT_FACTORY_H_

#include <memory>
#include <string>

#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/dns/dns_attempt.h"
#include "net/log/net_log_with_source.h"

namespace net {

// An interface used to instantiate DnsAttempt for
// DnsTransactionFactory::AttemptMode::kPlatform.
class NET_EXPORT DnsPlatformAttemptFactory {
 public:
  virtual ~DnsPlatformAttemptFactory() = default;

  // Creates a DnsAttempt for DnsTransactionFactory::AttemptMode::kPlatform.
  // - `server_index`, this is unused. Platform attempts are not expected to be
  //   able to control the DNS server being used, see OneShotDnsServerIterator.
  //   It is currently present only for compatibility with DnsAttempt and
  //   DnsTransaction's retry logic. TODO(crbug.com/493029486): Remove this.
  // - `hostname`, the hostname to resolve.
  // - `dns_query_type`, the DNS query type to use.
  // - `target_network`, the network that the attempt is targeting.
  // - `parent_net_log`, the netlog the calling code is using.
  virtual std::unique_ptr<DnsAttempt> CreateDnsPlatformAttempt(
      size_t server_index,
      base::span<const uint8_t> hostname,
      uint16_t dns_query_type,
      handles::NetworkHandle target_network,
      const NetLogWithSource& parent_net_log) = 0;
};

}  // namespace net

#endif  // NET_DNS_DNS_PLATFORM_ATTEMPT_FACTORY_H_
