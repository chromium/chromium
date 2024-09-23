// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_config.h"

#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/util.h"

namespace net {

namespace {

std::vector<std::string> SplitGroup(std::string_view group) {
  // Templates in a group are whitespace-separated.
  return SplitString(group, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                     base::SPLIT_WANT_NONEMPTY);
}

std::vector<std::optional<DnsOverHttpsServerConfig>> ParseTemplates(
    std::vector<std::string> templates) {
  std::vector<std::optional<DnsOverHttpsServerConfig>> parsed;
  parsed.reserve(templates.size());
  base::ranges::transform(templates, std::back_inserter(parsed), [](auto& s) {
    return DnsOverHttpsServerConfig::FromString(std::move(s));
  });
  return parsed;
}

constexpr std::string_view kJsonKeyServers("servers");

std::optional<DnsOverHttpsConfig> FromValue(base::Value::Dict value) {
  base::Value::List* servers_value = value.FindList(kJsonKeyServers);
  if (!servers_value)
    return std::nullopt;
  std::vector<DnsOverHttpsServerConfig> servers;
  servers.reserve(servers_value->size());
  for (base::Value& elt : *servers_value) {
    base::Value::Dict* dict = elt.GetIfDict();
    if (!dict)
      return std::nullopt;
    auto parsed = DnsOverHttpsServerConfig::FromValue(std::move(*dict));
    if (!parsed.has_value())
      return std::nullopt;
    servers.push_back(std::move(*parsed));
  }
  return DnsOverHttpsConfig(servers);
}

std::optional<DnsOverHttpsConfig> FromJson(std::string_view json) {
  std::optional<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value->is_dict())
    return std::nullopt;
  return FromValue(std::move(*value).TakeDict());
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
std::optional<DnsOverHttpsConfig> DnsOverHttpsConfig::FromTemplates(
    std::vector<std::string> server_templates) {
  // All templates must be valid for the group to be considered valid.
  std::vector<DnsOverHttpsServerConfig> servers;
  for (auto& server_config : ParseTemplates(server_templates)) {
    if (!server_config)
      return std::nullopt;
    servers.push_back(std::move(*server_config));
  }
  return DnsOverHttpsConfig(std::move(servers));
}

// static
std::optional<DnsOverHttpsConfig> DnsOverHttpsConfig::FromTemplatesForTesting(
    std::vector<std::string> server_templates) {
  return FromTemplates(std::move(server_templates));
}

// static
std::optional<DnsOverHttpsConfig> DnsOverHttpsConfig::FromString(
    std::string_view doh_config) {
  std::optional<DnsOverHttpsConfig> parsed = FromJson(doh_config);
  if (parsed && !parsed->servers().empty())
    return parsed;
  std::vector<std::string> server_templates = SplitGroup(doh_config);
  if (server_templates.empty())
    return std::nullopt;  // `doh_config` must contain at least one server.
  return FromTemplates(std::move(server_templates));
}

// static
DnsOverHttpsConfig DnsOverHttpsConfig::FromStringLax(
    std::string_view doh_config) {
  if (std::optional<DnsOverHttpsConfig> parsed = FromJson(doh_config)) {
    return *parsed;
  }
  auto parsed = ParseTemplates(SplitGroup(doh_config));
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

std::string DnsOverHttpsConfig::ToString() const {
  if (base::ranges::all_of(servers(), &DnsOverHttpsServerConfig::IsSimple)) {
    // Return the templates on separate lines.
    std::vector<std::string_view> strings;
    strings.reserve(servers().size());
    base::ranges::transform(servers(), std::back_inserter(strings),
                            &DnsOverHttpsServerConfig::server_template_piece);
    return base::JoinString(std::move(strings), "\n");
  }
  std::string json;
  CHECK(base::JSONWriter::WriteWithOptions(
      ToValue(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &json));
  // Remove the trailing newline from pretty-print output.
  base::TrimWhitespaceASCII(json, base::TRIM_TRAILING, &json);
  return json;
}

base::Value::Dict DnsOverHttpsConfig::ToValue() const {
  base::Value::List list;
  list.reserve(servers().size());
  for (const auto& server : servers()) {
    list.Append(server.ToValue());
  }
  base::Value::Dict dict;
  dict.Set(kJsonKeyServers, std::move(list));
  return dict;
}

}  // namespace net
