// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_DNS_OVER_HTTPS_SERVER_CONFIG_H_
#define NET_DNS_PUBLIC_DNS_OVER_HTTPS_SERVER_CONFIG_H_

#include <string>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// Simple representation of a DoH server for use in configurations.
class NET_EXPORT DnsOverHttpsServerConfig {
 public:
  // Returns nullopt if |doh_template| is invalid.
  static absl::optional<DnsOverHttpsServerConfig> FromString(
      std::string doh_template);

  bool operator==(const DnsOverHttpsServerConfig& other) const;
  bool operator<(const DnsOverHttpsServerConfig& other) const;

  const std::string& server_template() const;
  base::StringPiece server_template_piece() const;
  bool use_post() const;

  base::Value ToValue() const;

 private:
  DnsOverHttpsServerConfig(std::string server_template, bool use_post)
      : server_template_(std::move(server_template)), use_post_(use_post) {}

  std::string server_template_;
  bool use_post_;
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_DNS_OVER_HTTPS_SERVER_CONFIG_H_
