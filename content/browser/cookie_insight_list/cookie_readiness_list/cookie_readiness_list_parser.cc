// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_insight_list/cookie_readiness_list/cookie_readiness_list_parser.h"

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "content/public/browser/cookie_insight_list_data.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr std::string_view kEntriesFieldName = "entries";
constexpr std::string_view kDomainsFieldName = "domains";
constexpr std::string_view kTableEntryUrlFieldName = "tableEntryUrl";

// Creates a map of cookie domains to DomainInfo.
//
// Returns an empty map if any entry is misconfigured.
base::flat_map<std::string, DomainInfo> GenerateReadinessListMap(
    std::string_view json_content) {
  std::optional<base::Value> json =
      base::JSONReader::Read(json_content, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!json.has_value()) {
    return {};
  }

  base::Value::Dict* json_dict = json->GetIfDict();
  if (!json_dict) {
    return {};
  }

  base::Value* entries = json_dict->Find(kEntriesFieldName);
  if (!entries) {
    return {};
  }

  base::Value::List* entry_list = entries->GetIfList();
  if (!entry_list) {
    return {};
  }

  std::set<std::string> added_domains;
  std::vector<std::pair<std::string, DomainInfo>> domain_map_entries;
  for (const auto& entry : *entry_list) {
    const base::Value::Dict* entry_dict = entry.GetIfDict();
    if (!entry_dict) {
      return {};
    }

    const base::Value::List* domain_list =
        entry_dict->FindList(kDomainsFieldName);
    if (!domain_list) {
      return {};
    }

    const std::string* entry_url =
        entry_dict->FindString(kTableEntryUrlFieldName);
    if (!entry_url) {
      return {};
    }

    DomainInfo info;
    info.entry_url = *entry_url;

    for (const auto& domain : *domain_list) {
      const std::string* domain_string = domain.GetIfString();
      if (!domain_string ||
          net::registry_controlled_domains::GetDomainAndRegistry(
              GURL(base::StrCat({url::kHttpsScheme,
                                 url::kStandardSchemeSeparator,
                                 *domain_string})),
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)
              .empty()) {
        return {};
      }

      if (!added_domains.insert(*domain_string).second) {
        continue;
      }

      domain_map_entries.emplace_back(*domain_string, info);
    }
  }
  return base::flat_map<std::string, DomainInfo>(domain_map_entries);
}
}  // namespace

CookieInsightList CookieReadinessListParser::ParseReadinessList(
    std::string_view json_content) {
  return CookieInsightList(GenerateReadinessListMap(json_content));
}

}  // namespace content
