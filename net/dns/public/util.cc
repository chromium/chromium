// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/util.h"

#include <set>
#include <unordered_map>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "net/base/ip_address.h"
#include "net/dns/public/dns_protocol.h"
#include "net/third_party/uri_template/uri_template.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"
#include "url/url_constants.h"

namespace net {

namespace {

IPEndPoint GetMdnsIPEndPoint(const char* address) {
  IPAddress multicast_group_number;
  bool success = multicast_group_number.AssignFromIPLiteral(address);
  DCHECK(success);
  return IPEndPoint(multicast_group_number,
                    dns_protocol::kDefaultPortMulticast);
}

absl::optional<std::string> GetHttpsHost(const std::string& url) {
  // This code is used to compute a static initializer, so it runs before GURL's
  // scheme registry is initialized.  Since GURL is not ready yet, we need to
  // duplicate some of its functionality here.
  url::Parsed parsed;
  url::ParseStandardURL(url.data(), url.size(), &parsed);
  std::string canonical;
  url::StdStringCanonOutput output(&canonical);
  url::Parsed canonical_parsed;
  bool is_valid =
      url::CanonicalizeStandardURL(url.data(), url.size(), parsed,
                                   url::SchemeType::SCHEME_WITH_HOST_AND_PORT,
                                   nullptr, &output, &canonical_parsed);
  if (!is_valid)
    return absl::nullopt;
  const url::Component& scheme_range = canonical_parsed.scheme;
  base::StringPiece scheme =
      base::StringPiece(canonical).substr(scheme_range.begin, scheme_range.len);
  if (scheme != url::kHttpsScheme)
    return absl::nullopt;
  const url::Component& host_range = canonical_parsed.host;
  return canonical.substr(host_range.begin, host_range.len);
}

}  // namespace

namespace dns_util {

bool IsValidDohTemplate(base::StringPiece server_template,
                        std::string* server_method) {
  std::string url_string;
  std::string test_query = "this_is_a_test_query";
  std::unordered_map<std::string, std::string> template_params(
      {{"dns", test_query}});
  std::set<std::string> vars_found;
  bool valid_template = uri_template::Expand(
      std::string(server_template), template_params, &url_string, &vars_found);
  if (!valid_template) {
    // The URI template is malformed.
    return false;
  }
  absl::optional<std::string> host = GetHttpsHost(url_string);
  if (!host) {
    // The expanded template must be a valid HTTPS URL.
    return false;
  }
  if (host->find(test_query) != std::string::npos) {
    // The dns variable may not be part of the hostname.
    return false;
  }
  // If the template contains a dns variable, use GET, otherwise use POST.
  if (server_method) {
    *server_method =
        (vars_found.find("dns") == vars_found.end()) ? "POST" : "GET";
  }
  return true;
}

IPEndPoint GetMdnsGroupEndPoint(AddressFamily address_family) {
  switch (address_family) {
    case ADDRESS_FAMILY_IPV4:
      return GetMdnsIPEndPoint(dns_protocol::kMdnsMulticastGroupIPv4);
    case ADDRESS_FAMILY_IPV6:
      return GetMdnsIPEndPoint(dns_protocol::kMdnsMulticastGroupIPv6);
    default:
      NOTREACHED();
      return IPEndPoint();
  }
}

IPEndPoint GetMdnsReceiveEndPoint(AddressFamily address_family) {
// TODO(qingsi): MacOS should follow other POSIX platforms in the else-branch
// after addressing crbug.com/899310. We have encountered a conflicting issue on
// CrOS as described in crbug.com/931916, and the following is a temporary
// mitigation to reconcile the two issues. Remove this after closing
// crbug.com/899310.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_APPLE)
  // With Windows, binding to a mulitcast group address is not allowed.
  // Multicast messages will be received appropriate to the multicast groups the
  // socket has joined. Sockets intending to receive multicast messages should
  // bind to a wildcard address (e.g. 0.0.0.0).
  switch (address_family) {
    case ADDRESS_FAMILY_IPV4:
      return IPEndPoint(IPAddress::IPv4AllZeros(),
                        dns_protocol::kDefaultPortMulticast);
    case ADDRESS_FAMILY_IPV6:
      return IPEndPoint(IPAddress::IPv6AllZeros(),
                        dns_protocol::kDefaultPortMulticast);
    default:
      NOTREACHED();
      return IPEndPoint();
  }
#else   // !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)) || BUILDFLAG(IS_APPLE)
  // With POSIX, any socket can receive messages for multicast groups joined by
  // any socket on the system. Sockets intending to receive messages for a
  // specific multicast group should bind to that group address.
  return GetMdnsGroupEndPoint(address_family);
#endif  // !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)) || BUILDFLAG(IS_APPLE)
}

}  // namespace dns_util
}  // namespace net
