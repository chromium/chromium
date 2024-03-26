// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_DNS_OVER_HTTPS_CONFIG_H_
#define NET_DNS_PUBLIC_DNS_OVER_HTTPS_CONFIG_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "net/base/net_export.h"
#include "net/dns/public/dns_over_https_server_config.h"

namespace net {

// Represents a collection of DnsOverHttpsServerConfig.  The string
// representation is either a JSON object or a whitespace-separated
// list of DoH URI templates.
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

  // Constructs a Config from URI templates of zero or more servers.
  // Returns `nullopt` if any string is invalid.
  static std::optional<DnsOverHttpsConfig> FromTemplatesForTesting(
      std::vector<std::string> servers);

  // Constructs a Config from its text form if valid.  Returns `nullopt` if the
  // input is empty or invalid (even partly invalid).
  static std::optional<DnsOverHttpsConfig> FromString(
      std::string_view doh_config);

  // Constructs a DnsOverHttpsConfig from its text form, skipping any invalid
  // templates in the whitespace-separated form.  The result may be empty.
  static DnsOverHttpsConfig FromStringLax(std::string_view doh_config);

  bool operator==(const DnsOverHttpsConfig& other) const;

  // The servers that comprise this config.  May be empty.
  const std::vector<DnsOverHttpsServerConfig>& servers() const {
    return servers_;
  }

  // Inverse of FromString().  Uses the JSON representation if necessary.
  std::string ToString() const;

  // Encodes the config as a Value.  Used to produce the JSON representation.
  base::Value::Dict ToValue() const;

 private:
  // Constructs a Config from URI templates of zero or more servers.
  // Returns `nullopt` if any string is invalid.
  static std::optional<DnsOverHttpsConfig> FromTemplates(
      std::vector<std::string> servers);

  std::vector<DnsOverHttpsServerConfig> servers_;
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_DNS_OVER_HTTPS_CONFIG_H_
