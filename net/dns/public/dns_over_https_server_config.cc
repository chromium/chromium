// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_server_config.h"

#include <string>

#include "net/dns/public/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

absl::optional<DnsOverHttpsServerConfig> DnsOverHttpsServerConfig::FromString(
    std::string doh_template) {
  std::string server_method;
  if (!dns_util::IsValidDohTemplate(doh_template, &server_method)) {
    return absl::nullopt;
  }
  return DnsOverHttpsServerConfig(std::move(doh_template),
                                  server_method == "POST");
}

bool DnsOverHttpsServerConfig::operator==(
    const DnsOverHttpsServerConfig& other) const {
  // use_post_ is derived from server_template_, so we don't need to compare it.
  return server_template_ == other.server_template_;
}

bool DnsOverHttpsServerConfig::operator<(
    const DnsOverHttpsServerConfig& other) const {
  return server_template_ < other.server_template_;
}

const std::string& DnsOverHttpsServerConfig::server_template() const {
  return server_template_;
}

bool DnsOverHttpsServerConfig::use_post() const {
  return use_post_;
}

}  // namespace net
