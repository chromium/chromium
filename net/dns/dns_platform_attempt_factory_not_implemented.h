// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_PLATFORM_ATTEMPT_FACTORY_NOT_IMPLEMENTED_H_
#define NET_DNS_DNS_PLATFORM_ATTEMPT_FACTORY_NOT_IMPLEMENTED_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/dns/dns_attempt.h"
#include "net/dns/dns_platform_attempt_factory.h"
#include "net/log/net_log_with_source.h"

namespace net {

// An implementation of DnsPlatformAttemptFactory that always fails with
// NOTIMPLEMENTED. See URLRequestContextBuilder::dns_platform_attempt_factory_
// for more details
class NET_EXPORT DnsPlatformAttemptFactoryNotImplemented
    : public DnsPlatformAttemptFactory {
 public:
  ~DnsPlatformAttemptFactoryNotImplemented() override = default;

  std::unique_ptr<DnsAttempt> CreateDnsPlatformAttempt(
      size_t server_index,
      base::span<const uint8_t> hostname,
      uint16_t dns_query_type,
      handles::NetworkHandle target_network,
      const NetLogWithSource& net_log) override;
};

}  // namespace net

#endif  // NET_DNS_DNS_PLATFORM_ATTEMPT_FACTORY_NOT_IMPLEMENTED_H_
