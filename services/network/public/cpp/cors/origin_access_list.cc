// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/origin_access_list.h"

namespace network {

namespace cors {

OriginAccessList::OriginAccessList() = default;
OriginAccessList::~OriginAccessList() = default;

void OriginAccessList::SetAllowListForOrigin(
    const url::Origin& source_origin,
    const std::vector<mojom::CorsOriginPatternPtr>& patterns) {
  SetForOrigin(source_origin, patterns, &allow_list_,
               network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
}

void OriginAccessList::AddAllowListEntryForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    bool allow_subdomains,
    const network::mojom::CORSOriginAccessMatchPriority priority) {
  AddForOrigin(source_origin, protocol, domain, allow_subdomains, &allow_list_,
               priority);
}

void OriginAccessList::ClearAllowList() {
  allow_list_.clear();
}

void OriginAccessList::SetBlockListForOrigin(
    const url::Origin& source_origin,
    const std::vector<mojom::CorsOriginPatternPtr>& patterns) {
  SetForOrigin(source_origin, patterns, &block_list_,
               network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
}

void OriginAccessList::AddBlockListEntryForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    bool allow_subdomains,
    const network::mojom::CORSOriginAccessMatchPriority priority) {
  AddForOrigin(source_origin, protocol, domain, allow_subdomains, &block_list_,
               priority);
}

void OriginAccessList::ClearBlockList() {
  block_list_.clear();
}

bool OriginAccessList::IsAllowed(const url::Origin& source_origin,
                                 const GURL& destination) const {
  if (source_origin.opaque())
    return false;
  std::string source = source_origin.Serialize();
  url::Origin destination_origin = url::Origin::Create(destination);
  network::mojom::CORSOriginAccessMatchPriority allow_list_priority =
      GetHighestPriorityOfRuleForOrigin(source, destination_origin,
                                        allow_list_);
  if (allow_list_priority ==
      network::mojom::CORSOriginAccessMatchPriority::kNoMatchingOrigin)
    return false;
  network::mojom::CORSOriginAccessMatchPriority block_list_priority =
      GetHighestPriorityOfRuleForOrigin(source, destination_origin,
                                        block_list_);
  if (block_list_priority ==
      network::mojom::CORSOriginAccessMatchPriority::kNoMatchingOrigin)
    return true;
  return allow_list_priority > block_list_priority;
}

// static
void OriginAccessList::SetForOrigin(
    const url::Origin& source_origin,
    const std::vector<mojom::CorsOriginPatternPtr>& patterns,
    PatternMap* map,
    const network::mojom::CORSOriginAccessMatchPriority priority) {
  DCHECK(map);
  DCHECK(!source_origin.opaque());

  std::string source = source_origin.Serialize();
  map->erase(source);
  if (patterns.empty())
    return;

  Patterns& native_patterns = (*map)[source];
  for (const auto& pattern : patterns) {
    native_patterns.push_back(OriginAccessEntry(
        pattern->protocol, pattern->domain,
        pattern->allow_subdomains ? OriginAccessEntry::kAllowSubdomains
                                  : OriginAccessEntry::kDisallowSubdomains,
        priority));
  }
}

// static
void OriginAccessList::AddForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    bool allow_subdomains,
    PatternMap* map,
    const network::mojom::CORSOriginAccessMatchPriority priority) {
  DCHECK(map);
  DCHECK(!source_origin.opaque());

  std::string source = source_origin.Serialize();
  (*map)[source].push_back(OriginAccessEntry(
      protocol, domain,
      allow_subdomains ? OriginAccessEntry::kAllowSubdomains
                       : OriginAccessEntry::kDisallowSubdomains,
      priority));
}

// static
// TODO(nrpeter): Sort OriginAccessEntry entries on edit then we can return the
// first match which will be the top priority.
network::mojom::CORSOriginAccessMatchPriority
OriginAccessList::GetHighestPriorityOfRuleForOrigin(
    const std::string& source,
    const url::Origin& destination_origin,
    const PatternMap& map) {
  network::mojom::CORSOriginAccessMatchPriority highest_priority =
      network::mojom::CORSOriginAccessMatchPriority::kNoMatchingOrigin;
  auto patterns_for_origin_it = map.find(source);
  if (patterns_for_origin_it == map.end())
    return highest_priority;
  for (const auto& entry : patterns_for_origin_it->second) {
    if (entry.MatchesOrigin(destination_origin) !=
        OriginAccessEntry::kDoesNotMatchOrigin) {
      highest_priority = std::max(highest_priority, entry.priority());
    }
  }
  return highest_priority;
}

}  // namespace cors

}  // namespace network
