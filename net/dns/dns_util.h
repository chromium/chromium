// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_UTIL_H_
#define NET_DNS_DNS_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/secure_dns_mode.h"

namespace net {

// Return the expanded template when no variables have corresponding values.
NET_EXPORT_PRIVATE std::string GetURLFromTemplateWithoutParameters(
    const std::string& server_template);

NET_EXPORT_PRIVATE
base::TimeDelta GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
    const char* field_trial_name,
    base::TimeDelta default_delta,
    NetworkChangeNotifier::ConnectionType connection_type);

// Creates a 2-byte string that represents the name pointer defined in Section
// 4.1.1 of RFC 1035 for the given offset. The first two bits in the first byte
// of the name pointer are ones, and the rest 14 bits are given to `offset`,
// which specifies an offset from the start of the message for the pointed name.
// Note that `offset` must be less than 2^14 - 1 by definition.
NET_EXPORT std::string CreateNamePointer(uint16_t offset);

// Convert a DnsQueryType enum to the wire format integer representation.
NET_EXPORT_PRIVATE uint16_t DnsQueryTypeToQtype(DnsQueryType dns_query_type);

NET_EXPORT DnsQueryType
AddressFamilyToDnsQueryType(AddressFamily address_family);

// Uses the hardcoded upgrade mapping to discover DoH service(s) associated with
// a DoT hostname. Providers with a disabled `base::Feature` are not eligible
// for upgrade.
NET_EXPORT_PRIVATE std::vector<DnsOverHttpsServerConfig>
GetDohUpgradeServersFromDotHostname(const std::string& dot_server);

// Uses the hardcoded upgrade mapping to discover DoH service(s) associated with
// a list of insecure DNS servers. Server ordering is preserved across the
// mapping. Providers with a disabled `base::Feature` are not eligible for
// upgrade.
NET_EXPORT_PRIVATE std::vector<DnsOverHttpsServerConfig>
GetDohUpgradeServersFromNameservers(const std::vector<IPEndPoint>& dns_servers);

// Returns the provider id to use in UMA histogram names. If there is no
// provider id that matches `doh_server`, returns "Other".
NET_EXPORT_PRIVATE std::string GetDohProviderIdForHistogramFromServerConfig(
    const DnsOverHttpsServerConfig& doh_server);

// Returns the provider id to use in UMA histogram names. If there is no
// provider id that matches `nameserver`, returns "Other".
NET_EXPORT_PRIVATE std::string GetDohProviderIdForHistogramFromNameserver(
    const IPEndPoint& nameserver);

NET_EXPORT_PRIVATE std::string SecureDnsModeToString(
    const SecureDnsMode secure_dns_mode);

}  // namespace net

#endif  // NET_DNS_DNS_UTIL_H_
