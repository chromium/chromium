// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_HOSTS_H_
#define NET_DNS_DNS_HOSTS_H_

#include <stddef.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "base/files/file_path.h"
#include "net/base/address_family.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"

namespace net {

using DnsHostsKey = std::pair<std::string, AddressFamily>;

struct DnsHostsKeyHash {
  std::size_t operator()(const DnsHostsKey& key) const {
    return std::hash<std::string_view>()(key.first) + key.second;
  }
};

// There are OS-specific variations in how commas in the hosts file behave.
enum ParseHostsCommaMode {
  // Comma is treated as part of a hostname:
  // "127.0.0.1 foo,bar" parses as "foo,bar" mapping to "127.0.0.1".
  PARSE_HOSTS_COMMA_IS_TOKEN,

  // Comma is treated as a hostname separator:
  // "127.0.0.1 foo,bar" parses as "foo" and "bar" both mapping to "127.0.0.1".
  PARSE_HOSTS_COMMA_IS_WHITESPACE,
};

// Parsed results of a Hosts file.
//
// Although Hosts files map IP address to a list of domain names, for name
// resolution the desired mapping direction is: domain name to IP address.
// When parsing Hosts, we apply the "first hit" rule as Windows and glibc do.
// With a Hosts file of:
// 300.300.300.300 localhost # bad ip
// 127.0.0.1 localhost
// 10.0.0.1 localhost
// The expected resolution of localhost is 127.0.0.1.
using DnsHosts = std::unordered_map<DnsHostsKey, IPAddress, DnsHostsKeyHash>;

// Parses |contents| (as read from /etc/hosts or equivalent) and stores results
// in |dns_hosts|. Invalid lines are ignored (as in most implementations).
// Overrides the OS-specific default handling of commas, so unittests can test
// both modes.
void NET_EXPORT_PRIVATE ParseHostsWithCommaModeForTesting(
    const std::string& contents,
    DnsHosts* dns_hosts,
    ParseHostsCommaMode comma_mode);

// Parses |contents| (as read from /etc/hosts or equivalent) and stores results
// in |dns_hosts|. Invalid lines are ignored (as in most implementations).
void NET_EXPORT_PRIVATE ParseHosts(const std::string& contents,
                                   DnsHosts* dns_hosts);

// Test-injectable HOSTS parser.
class NET_EXPORT_PRIVATE DnsHostsParser {
 public:
  virtual ~DnsHostsParser();

  // Parses HOSTS and stores results in `dns_hosts`, with addresses in the order
  // in which they were read. Invalid lines are ignored (as in most
  // implementations).
  virtual bool ParseHosts(DnsHosts* hosts) const = 0;
};

// Implementation of `DnsHostsParser` that reads HOSTS from a given file.
class NET_EXPORT_PRIVATE DnsHostsFileParser : public DnsHostsParser {
 public:
  explicit DnsHostsFileParser(base::FilePath hosts_file_path);
  ~DnsHostsFileParser() override;

  DnsHostsFileParser(const DnsHostsFileParser&) = delete;
  DnsHostsFileParser& operator=(const DnsHostsFileParser&) = delete;

  // DnsHostsParser:
  bool ParseHosts(DnsHosts* dns_hosts) const override;

 private:
  const base::FilePath hosts_file_path_;
};

}  // namespace net

#endif  // NET_DNS_DNS_HOSTS_H_
