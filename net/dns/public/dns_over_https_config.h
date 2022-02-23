// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_DNS_OVER_HTTPS_CONFIG_H_
#define NET_DNS_PUBLIC_DNS_OVER_HTTPS_CONFIG_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// Represents a collection of DnsOverHttpsServerConfig.  The string
// representation is a whitespace-separated list of DoH URI templates.
// The Value representation is a list of dictionaries.
class NET_EXPORT DnsOverHttpsConfig {
 public:
  DnsOverHttpsConfig();
  ~DnsOverHttpsConfig();
  DnsOverHttpsConfig(const DnsOverHttpsConfig& other);
  DnsOverHttpsConfig& operator=(const DnsOverHttpsConfig& other);
  DnsOverHttpsConfig(DnsOverHttpsConfig&& other);
  DnsOverHttpsConfig& operator=(DnsOverHttpsConfig&& other);

  explicit DnsOverHttpsConfig(std::vector<DnsOverHttpsServerConfig> servers);

  // Constructs a Config from textual representations of zero or more servers.
  // Returns `nullopt` if any string is invalid.
  static absl::optional<DnsOverHttpsConfig> FromStrings(
      std::vector<std::string> servers);

  // Constructs a Config from its text form if valid.  Returns `nullopt` if the
  // input is empty or invalid (even partly invalid).
  static absl::optional<DnsOverHttpsConfig> FromString(
      base::StringPiece doh_config);

  // Constructs a DnsOverHttpsConfig from its text form, skipping any invalid
  // templates.  The result may be empty.
  static DnsOverHttpsConfig FromStringLax(base::StringPiece doh_config);

  bool operator==(const DnsOverHttpsConfig& other) const;

  // The servers that comprise this config.  May be empty.
  const std::vector<DnsOverHttpsServerConfig>& servers() const {
    return servers_;
  }

  // Returns string representations of the individual DnsOverHttpsServerConfigs.
  // The return value will be invalidated if this object is destroyed or moved.
  std::vector<base::StringPiece> ToStrings() const;

  // Inverse of FromString().
  std::string ToString() const;

  // Encodes the config as a Value.  Currently only used for NetLog.
  base::Value ToValue() const;

 private:
  std::vector<DnsOverHttpsServerConfig> servers_;
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_DNS_OVER_HTTPS_CONFIG_H_
