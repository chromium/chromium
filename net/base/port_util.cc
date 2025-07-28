// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/port_util.h"

#include <limits>
#include <set>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/base/parse_number.h"
#include "url/url_constants.h"

namespace net {

namespace {

// The general list of blocked ports. Will be blocked unless a specific
// protocol overrides it. (Ex: ftp can use port 21)
// When adding a port to the list, consider also adding it to kAllowablePorts,
// below. See <https://fetch.spec.whatwg.org/#port-blocking>.
const int kRestrictedPorts[] = {
    0,      // Not in Fetch Spec.
    1,      // tcpmux
    7,      // echo
    9,      // discard
    11,     // systat
    13,     // daytime
    15,     // netstat
    17,     // qotd
    19,     // chargen
    20,     // ftp data
    21,     // ftp access
    22,     // ssh
    23,     // telnet
    25,     // smtp
    37,     // time
    42,     // name
    43,     // nicname
    53,     // domain
    69,     // tftp
    77,     // priv-rjs
    79,     // finger
    87,     // ttylink
    95,     // supdup
    101,    // hostriame
    102,    // iso-tsap
    103,    // gppitnp
    104,    // acr-nema
    109,    // pop2
    110,    // pop3
    111,    // sunrpc
    113,    // auth
    115,    // sftp
    117,    // uucp-path
    119,    // nntp
    123,    // NTP
    135,    // loc-srv /epmap
    137,    // netbios
    139,    // netbios
    143,    // imap2
    161,    // snmp
    179,    // BGP
    389,    // ldap
    427,    // SLP (Also used by Apple Filing Protocol)
    465,    // smtp+ssl
    512,    // print / exec
    513,    // login
    514,    // shell
    515,    // printer
    526,    // tempo
    530,    // courier
    531,    // chat
    532,    // netnews
    540,    // uucp
    548,    // AFP (Apple Filing Protocol)
    554,    // rtsp
    556,    // remotefs
    563,    // nntp+ssl
    587,    // smtp (rfc6409)
    601,    // syslog-conn (rfc3195)
    636,    // ldap+ssl
    989,    // ftps-data
    990,    // ftps
    993,    // ldap+ssl
    995,    // pop3+ssl
    1719,   // h323gatestat
    1720,   // h323hostcall
    1723,   // pptp
    2049,   // nfs
    3659,   // apple-sasl / PasswordServer
    4045,   // lockd
    5060,   // sip
    5061,   // sips
    6000,   // X11
    6566,   // sane-port
    6665,   // Alternate IRC [Apple addition]
    6666,   // Alternate IRC [Apple addition]
    6667,   // Standard IRC [Apple addition]
    6668,   // Alternate IRC [Apple addition]
    6669,   // Alternate IRC [Apple addition]
    6697,   // IRC + TLS
    10080,  // Amanda
};

std::multiset<int>& GetExplicitlyAllowedPorts() {
  static base::NoDestructor<std::multiset<int>> explicitly_allowed_ports;
  return *explicitly_allowed_ports;
}

// List of ports which are permitted to be reenabled despite being in
// kRestrictedList. When adding an port to this list you should also update the
// enterprise policy to document the fact that the value can be set. Ports
// should only remain in this list for about a year to give time for users to
// migrate off while stopping them from becoming permanent parts of the web
// platform.
constexpr int kAllowablePorts[] = {};

int g_scoped_allowable_port = 0;

using PortSet = base::flat_set<int>;

PortSet ParseRestrictedPortsFromFeatureParam(const base::Feature& feature,
                                             std::string_view param_name) {
  const std::string ports_string =
      base::GetFieldTrialParamValueByFeature(feature, std::string(param_name));
  PortSet::container_type ports;
  for (const auto& port_string :
       base::SplitStringPiece(ports_string, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    int port;
    if (net::ParseInt32(port_string, net::ParseIntFormat::STRICT_NON_NEGATIVE,
                        &port)) {
      ports.push_back(port);
    } else {
      DLOG(ERROR) << "Ignoring invalid port for " << param_name << ": "
                  << port_string;
    }
  }
  return PortSet(std::move(ports));
}

constinit bool g_need_to_reset_restrict_localhost_ports = false;

}  // namespace

bool IsPortValid(int port) {
  return port >= 0 && port <= std::numeric_limits<uint16_t>::max();
}

bool IsWellKnownPort(int port) {
  return port >= 0 && port < 1024;
}

bool IsPortAllowedForScheme(int port, std::string_view url_scheme) {
  // Reject invalid ports.
  if (!IsPortValid(port))
    return false;

  // Allow explicitly allowed ports for any scheme.
  if (GetExplicitlyAllowedPorts().count(port) > 0) {
    return true;
  }

  // Finally check against the generic list of restricted ports for all
  // schemes.
  for (int restricted_port : kRestrictedPorts) {
    if (restricted_port == port)
      return false;
  }

  if (base::FeatureList::IsEnabled(features::kRestrictAbusePorts)) {
    static const base::NoDestructor<PortSet> restrict_ports(
        ParseRestrictedPortsFromFeatureParam(features::kRestrictAbusePorts,
                                             "restrict_ports"));
    static const base::NoDestructor<PortSet> monitor_ports(
        ParseRestrictedPortsFromFeatureParam(features::kRestrictAbusePorts,
                                             "monitor_ports"));

    if (restrict_ports->contains(port)) {
      base::UmaHistogramSparse("Net.RestrictedPorts", port);
      return false;
    } else if (monitor_ports->contains(port)) {
      base::UmaHistogramSparse("Net.RestrictedPorts", port);
    }
  }

  return true;
}

bool IsPortAllowedForIpEndpoint(const IPEndPoint& endpoint) {
  if (!base::FeatureList::IsEnabled(features::kRestrictAbusePortsOnLocalhost)) {
    return true;
  }

  // This function currently restricts only on localhost.
  if (!endpoint.address().IsLoopback()) {
    return true;
  }

  int port = endpoint.port();

  // Allow explicitly allowed ports.
  if (GetExplicitlyAllowedPorts().count(port) > 0) {
    return true;
  }

  static base::NoDestructor<PortSet> restrict_localhost_ports(
      ParseRestrictedPortsFromFeatureParam(
          features::kRestrictAbusePortsOnLocalhost,
          "localhost_restrict_ports"));

  if (g_need_to_reset_restrict_localhost_ports) {
    *restrict_localhost_ports = ParseRestrictedPortsFromFeatureParam(
        features::kRestrictAbusePortsOnLocalhost, "localhost_restrict_ports");
    g_need_to_reset_restrict_localhost_ports = false;
  }

  if (restrict_localhost_ports->contains(port)) {
    base::UmaHistogramSparse("Net.RestrictedLocalhostPorts", port);
    return false;
  }
  return true;
}

size_t GetCountOfExplicitlyAllowedPorts() {
  return GetExplicitlyAllowedPorts().size();
}

// Specifies a comma separated list of port numbers that should be accepted
// despite bans. If the string is invalid no allowed ports are stored.
void SetExplicitlyAllowedPorts(base::span<const uint16_t> allowed_ports) {
  std::multiset<int> ports(allowed_ports.begin(), allowed_ports.end());
  GetExplicitlyAllowedPorts() = std::move(ports);
}

ScopedPortException::ScopedPortException(int port) : port_(port) {
  GetExplicitlyAllowedPorts().insert(port);
}

ScopedPortException::~ScopedPortException() {
  auto it = GetExplicitlyAllowedPorts().find(port_);
  if (it != GetExplicitlyAllowedPorts().end()) {
    GetExplicitlyAllowedPorts().erase(it);
  } else {
    NOTREACHED();
  }
}

NET_EXPORT bool IsAllowablePort(int port) {
  for (auto allowable_port : kAllowablePorts) {
    if (port == allowable_port) {
      return true;
    }
  }

  if (port == g_scoped_allowable_port)
    return true;

  return false;
}

ScopedAllowablePortForTesting::ScopedAllowablePortForTesting(int port) {
  DCHECK_EQ(g_scoped_allowable_port, 0);
  g_scoped_allowable_port = port;
}

ScopedAllowablePortForTesting::~ScopedAllowablePortForTesting() {
  g_scoped_allowable_port = 0;
}

void ReloadLocalhostRestrictedPortsForTesting() {
  g_need_to_reset_restrict_localhost_ports = true;
}

}  // namespace net
