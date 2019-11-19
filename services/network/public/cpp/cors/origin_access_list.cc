// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/origin_access_list.h"

#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"

namespace network {

namespace cors {

OriginAccessList::OriginAccessList() = default;
OriginAccessList::~OriginAccessList() = default;

void OriginAccessList::SetAllowListForOrigin(
    const url::Origin& source_origin,
    const std::vector<CorsOriginPatternPtr>& patterns) {
  SetForOrigin(source_origin, patterns, &map_, MapType::kAllowPatterns);
}

void OriginAccessList::AddAllowListEntryForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    const uint16_t port,
    const mojom::CorsDomainMatchMode domain_match_mode,
    const mojom::CorsPortMatchMode port_match_mode,
    const mojom::CorsOriginAccessMatchPriority priority) {
  AddForOrigin(
      source_origin,
      mojom::CorsOriginPattern::New(protocol, domain, port, domain_match_mode,
                                    port_match_mode, priority),
      &map_, MapType::kAllowPatterns);
}

void OriginAccessList::SetBlockListForOrigin(
    const url::Origin& source_origin,
    const std::vector<CorsOriginPatternPtr>& patterns) {
  SetForOrigin(source_origin, patterns, &map_, MapType::kBlockPatterns);
}

void OriginAccessList::AddBlockListEntryForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    const uint16_t port,
    const mojom::CorsDomainMatchMode domain_match_mode,
    const mojom::CorsPortMatchMode port_match_mode,
    const mojom::CorsOriginAccessMatchPriority priority) {
  AddForOrigin(
      source_origin,
      mojom::CorsOriginPattern::New(protocol, domain, port, domain_match_mode,
                                    port_match_mode, priority),
      &map_, MapType::kBlockPatterns);
}

void OriginAccessList::ClearForOrigin(const url::Origin& source_origin) {
  DCHECK(!source_origin.opaque());
  map_.erase(source_origin.Serialize());
}

void OriginAccessList::Clear() {
  map_.clear();
}

OriginAccessList::AccessState OriginAccessList::CheckAccessState(
    const url::Origin& source_origin,
    const GURL& destination) const {
  if (source_origin.opaque())
    return AccessState::kBlocked;

  const std::string source = source_origin.Serialize();
  const url::Origin destination_origin = url::Origin::Create(destination);
  const auto patterns_map_it = map_.find(source);
  if (patterns_map_it == map_.end())
    return AccessState::kNotListed;

  const network::mojom::CorsOriginAccessMatchPriority allow_list_priority =
      GetHighestPriorityOfRuleForOrigin(
          destination_origin, patterns_map_it->second, MapType::kAllowPatterns);
  const network::mojom::CorsOriginAccessMatchPriority block_list_priority =
      GetHighestPriorityOfRuleForOrigin(
          destination_origin, patterns_map_it->second, MapType::kBlockPatterns);
  if (block_list_priority ==
      network::mojom::CorsOriginAccessMatchPriority::kNoMatchingOrigin) {
    return (allow_list_priority ==
            network::mojom::CorsOriginAccessMatchPriority::kNoMatchingOrigin)
               ? AccessState::kNotListed
               : AccessState::kAllowed;
  }

  return (allow_list_priority > block_list_priority) ? AccessState::kAllowed
                                                     : AccessState::kBlocked;
}

OriginAccessList::AccessState OriginAccessList::CheckAccessState(
    const ResourceRequest& request) const {
  // OriginAccessList is in practice used to disable CORS for Chrome Extensions.
  // The extension origin can be found in either:
  // 1) |request.isolated_world_origin| (if this is a request from a content
  //    script;  in this case there is no point looking at (2) below.
  // 2) |request.request_initiator| (if this is a request from an extension
  //    background page or from other extension frames).
  //
  // Note that similar code is also in CorsURLLoader::CalculateResponseTainting.
  //
  // TODO(lukasza): https://crbug.com/936310 and https://crbug.com/920638:
  // Once 1) there is no global OriginAccessList and 2) per-factory
  // OriginAccessList is only populated for URLLoaderFactory used by allowlisted
  // content scripts, then 3) there should no longer be a need to use origins as
  // a key in an OriginAccessList.
  DCHECK(request.request_initiator.has_value());
  const url::Origin& source_origin =
      request.isolated_world_origin.value_or(*request.request_initiator);

  return CheckAccessState(source_origin, request.url);
}

std::vector<mojo::StructPtr<mojom::CorsOriginAccessPatterns>>
OriginAccessList::CreateCorsOriginAccessPatternsList() const {
  std::vector<mojom::CorsOriginAccessPatternsPtr> access_patterns;
  for (const auto& it : map_) {
    std::vector<mojom::CorsOriginPatternPtr> allow_patterns;
    const auto& allow_entries = it.second.find(MapType::kAllowPatterns);
    if (allow_entries != it.second.end()) {
      for (const auto& pattern : allow_entries->second)
        allow_patterns.push_back(pattern.CreateCorsOriginPattern());
    }
    std::vector<mojom::CorsOriginPatternPtr> block_patterns;
    const auto& block_entries = it.second.find(MapType::kBlockPatterns);
    if (block_entries != it.second.end()) {
      for (const auto& pattern : block_entries->second)
        block_patterns.push_back(pattern.CreateCorsOriginPattern());
    }
    access_patterns.push_back(mojom::CorsOriginAccessPatterns::New(
        url::Origin::Create(GURL(it.first)), std::move(allow_patterns),
        std::move(block_patterns)));
  }
  return access_patterns;
}

// static
void OriginAccessList::SetForOrigin(
    const url::Origin& source_origin,
    const std::vector<CorsOriginPatternPtr>& patterns,
    OriginPatternsMap* map,
    MapType type) {
  DCHECK(map);
  DCHECK(!source_origin.opaque());

  const std::string source = source_origin.Serialize();
  PatternsMap& patterns_map = (*map)[source];
  Patterns& patterns_for_type = patterns_map[type];
  patterns_for_type.clear();
  for (const auto& pattern : patterns) {
    patterns_for_type.push_back(
        OriginAccessEntry(pattern->protocol, pattern->domain, pattern->port,
                          pattern->domain_match_mode, pattern->port_match_mode,
                          pattern->priority));
  }
  if (patterns_map[MapType::kAllowPatterns].empty() &&
      patterns_map[MapType::kBlockPatterns].empty()) {
    (*map).erase(source);
  }
}

// static
void OriginAccessList::AddForOrigin(const url::Origin& source_origin,
                                    const CorsOriginPatternPtr& pattern,
                                    OriginPatternsMap* map,
                                    MapType type) {
  DCHECK(map);
  DCHECK(!source_origin.opaque());

  const std::string source = source_origin.Serialize();
  (*map)[source][type].push_back(OriginAccessEntry(
      pattern->protocol, pattern->domain, pattern->port,
      pattern->domain_match_mode, pattern->port_match_mode, pattern->priority));
}

// static
// TODO(nrpeter): Sort OriginAccessEntry entries on edit then we can return the
// first match which will be the top priority.
network::mojom::CorsOriginAccessMatchPriority
OriginAccessList::GetHighestPriorityOfRuleForOrigin(
    const url::Origin& destination_origin,
    const PatternsMap& patterns_map,
    MapType type) {
  const auto patterns_it = patterns_map.find(type);
  if (patterns_it == patterns_map.end())
    return network::mojom::CorsOriginAccessMatchPriority::kNoMatchingOrigin;

  network::mojom::CorsOriginAccessMatchPriority highest_priority =
      network::mojom::CorsOriginAccessMatchPriority::kNoMatchingOrigin;
  for (const auto& entry : patterns_it->second) {
    if (entry.MatchesOrigin(destination_origin) !=
        OriginAccessEntry::kDoesNotMatchOrigin) {
      highest_priority = std::max(highest_priority, entry.priority());
    }
  }
  return highest_priority;
}

}  // namespace cors

}  // namespace network
