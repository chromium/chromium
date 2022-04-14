// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_config.h"

#include <iterator>
#include <string>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {

std::vector<std::string> SplitGroup(base::StringPiece group) {
  // Templates in a group are whitespace-separated.
  return SplitString(group, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                     base::SPLIT_WANT_NONEMPTY);
}

std::vector<absl::optional<DnsOverHttpsServerConfig>> ParseServers(
    std::vector<std::string> servers) {
  std::vector<absl::optional<DnsOverHttpsServerConfig>> parsed;
  parsed.reserve(servers.size());
  base::ranges::transform(servers, std::back_inserter(parsed), [](auto& s) {
    return DnsOverHttpsServerConfig::FromString(std::move(s));
  });
  return parsed;
}

}  // namespace

DnsOverHttpsConfig::DnsOverHttpsConfig() = default;
DnsOverHttpsConfig::~DnsOverHttpsConfig() = default;
DnsOverHttpsConfig::DnsOverHttpsConfig(const DnsOverHttpsConfig& other) =
    default;
DnsOverHttpsConfig& DnsOverHttpsConfig::operator=(
    const DnsOverHttpsConfig& other) = default;
DnsOverHttpsConfig::DnsOverHttpsConfig(DnsOverHttpsConfig&& other) = default;
DnsOverHttpsConfig& DnsOverHttpsConfig::operator=(DnsOverHttpsConfig&& other) =
    default;

DnsOverHttpsConfig::DnsOverHttpsConfig(
    std::vector<DnsOverHttpsServerConfig> servers)
    : servers_(std::move(servers)) {}

// static
absl::optional<DnsOverHttpsConfig> DnsOverHttpsConfig::FromStrings(
    std::vector<std::string> server_strings) {
  // All templates must be valid for the group to be considered valid.
  std::vector<DnsOverHttpsServerConfig> servers;
  for (auto& server_config : ParseServers(server_strings)) {
    if (!server_config)
      return absl::nullopt;
    servers.push_back(std::move(*server_config));
  }
  return DnsOverHttpsConfig(std::move(servers));
}

// static
absl::optional<DnsOverHttpsConfig> DnsOverHttpsConfig::FromString(
    base::StringPiece doh_config) {
  // TODO(crbug.com/1200908): Also accept JSON-formatted input.
  std::vector<std::string> server_strings = SplitGroup(doh_config);
  if (server_strings.empty())
    return absl::nullopt;  // `doh_config` must contain at least one server.
  return FromStrings(std::move(server_strings));
}

// static
DnsOverHttpsConfig DnsOverHttpsConfig::FromStringLax(
    base::StringPiece doh_config) {
  auto parsed = ParseServers(SplitGroup(doh_config));
  std::vector<DnsOverHttpsServerConfig> servers;
  for (auto& server_config : parsed) {
    if (server_config)
      servers.push_back(std::move(*server_config));
  }
  return DnsOverHttpsConfig(std::move(servers));
}

bool DnsOverHttpsConfig::operator==(const DnsOverHttpsConfig& other) const {
  return servers() == other.servers();
}

std::vector<base::StringPiece> DnsOverHttpsConfig::ToStrings() const {
  std::vector<base::StringPiece> strings;
  strings.reserve(servers().size());
  base::ranges::transform(servers(), std::back_inserter(strings),
                          &DnsOverHttpsServerConfig::server_template_piece);
  return strings;
}

std::string DnsOverHttpsConfig::ToString() const {
  // TODO(crbug.com/1200908): Return JSON for complex configurations.
  return base::JoinString(ToStrings(), "\n");
}

base::Value DnsOverHttpsConfig::ToValue() const {
  base::Value::ListStorage list;
  list.reserve(servers().size());
  base::ranges::transform(servers(), std::back_inserter(list),
                          &DnsOverHttpsServerConfig::ToValue);
  return base::Value(std::move(list));
}

}  // namespace net
