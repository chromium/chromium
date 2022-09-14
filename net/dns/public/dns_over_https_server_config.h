// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_DNS_OVER_HTTPS_SERVER_CONFIG_H_
#define NET_DNS_PUBLIC_DNS_OVER_HTTPS_SERVER_CONFIG_H_

#include <string>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// Simple representation of a DoH server for use in configurations.
class NET_EXPORT DnsOverHttpsServerConfig {
 public:
  // TODO(crbug.com/1200908): Generalize endpoints to enable other capabilities
  // of HTTPS records, such as extended metadata and aliases.
  using Endpoints = std::vector<IPAddressList>;

  // A default constructor is required by Mojo.
  DnsOverHttpsServerConfig();
  DnsOverHttpsServerConfig(const DnsOverHttpsServerConfig& other);
  DnsOverHttpsServerConfig& operator=(const DnsOverHttpsServerConfig& other);
  DnsOverHttpsServerConfig(DnsOverHttpsServerConfig&& other);
  DnsOverHttpsServerConfig& operator=(DnsOverHttpsServerConfig&& other);
  ~DnsOverHttpsServerConfig();

  // Returns nullopt if |doh_template| is invalid.
  static absl::optional<DnsOverHttpsServerConfig> FromString(
      std::string doh_template,
      Endpoints endpoints = {});

  static absl::optional<DnsOverHttpsServerConfig> FromValue(
      base::Value::Dict value);

  bool operator==(const DnsOverHttpsServerConfig& other) const;
  bool operator<(const DnsOverHttpsServerConfig& other) const;

  const std::string& server_template() const;
  base::StringPiece server_template_piece() const;
  bool use_post() const;
  const Endpoints& endpoints() const;

  // Returns true if this server config can be represented as just a template.
  bool IsSimple() const;

  base::Value::Dict ToValue() const;

 private:
  DnsOverHttpsServerConfig(std::string server_template,
                           bool use_post,
                           Endpoints endpoints);

  std::string server_template_;
  bool use_post_;
  Endpoints endpoints_;
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_DNS_OVER_HTTPS_SERVER_CONFIG_H_
