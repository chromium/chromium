// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_OVERRIDES_H_
#define NET_DNS_DNS_CONFIG_OVERRIDES_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_hosts.h"

namespace net {

// Overriding values to be applied over a DnsConfig struct.
struct NET_EXPORT DnsConfigOverrides {
  DnsConfigOverrides();
  DnsConfigOverrides(const DnsConfigOverrides& other);
  DnsConfigOverrides(DnsConfigOverrides&& other);
  ~DnsConfigOverrides();

  DnsConfigOverrides& operator=(const DnsConfigOverrides& other);
  DnsConfigOverrides& operator=(DnsConfigOverrides&& other);

  bool operator==(const DnsConfigOverrides& other) const;
  bool operator!=(const DnsConfigOverrides& other) const;

  // Creation method that initializes all values with the defaults from
  // DnsConfig. Guarantees the result of OverridesEverything() will be |true|.
  static DnsConfigOverrides CreateOverridingEverythingWithDefaults();

  // Creates a new DnsConfig where any field with an overriding value in |this|
  // is replaced with that overriding value. Any field without an overriding
  // value (|base::nullopt|) will be copied as-is from |config|.
  DnsConfig ApplyOverrides(const DnsConfig& config) const;

  // Returns |true| if the overriding configuration is comprehensive and would
  // override everything in a base DnsConfig. This is the case if all Optional
  // fields have a value.
  bool OverridesEverything() const;

  // Overriding values. See same-named fields in DnsConfig for explanations.
  base::Optional<std::vector<IPEndPoint>> nameservers;
  base::Optional<std::vector<std::string>> search;
  base::Optional<DnsHosts> hosts;
  base::Optional<bool> append_to_multi_label_name;
  base::Optional<bool> randomize_ports;
  base::Optional<int> ndots;
  base::Optional<base::TimeDelta> timeout;
  base::Optional<int> attempts;
  base::Optional<bool> rotate;
  base::Optional<bool> use_local_ipv6;
  base::Optional<std::vector<DnsConfig::DnsOverHttpsServerConfig>>
      dns_over_https_servers;
  base::Optional<DnsConfig::SecureDnsMode> secure_dns_mode;
  base::Optional<bool> allow_dns_over_https_upgrade;
  base::Optional<std::vector<std::string>> disabled_upgrade_providers;

  // Note no overriding value for |unhandled_options|. It is meta-configuration,
  // and there should be no reason to override it.
};

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_OVERRIDES_H_
