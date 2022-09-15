// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/public_sets.h"

#include <tuple>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"

namespace net {

namespace {

// Converts WS to HTTP, and WSS to HTTPS.
SchemefulSite NormalizeScheme(const SchemefulSite& site) {
  SchemefulSite normalized_site = site;
  normalized_site.ConvertWebSocketToHttp();
  return normalized_site;
}

}  // namespace

PublicSets::PublicSets() = default;

PublicSets::PublicSets(
    base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
    base::flat_map<SchemefulSite, SchemefulSite> aliases)
    : entries_(std::move(entries)), aliases_(std::move(aliases)) {}

PublicSets::PublicSets(PublicSets&&) = default;
PublicSets& PublicSets::operator=(PublicSets&&) = default;

PublicSets::~PublicSets() = default;

bool PublicSets::operator==(const PublicSets& other) const {
  return std::tie(entries_, aliases_) ==
         std::tie(other.entries_, other.aliases_);
}

bool PublicSets::operator!=(const PublicSets& other) const {
  return !(*this == other);
}

PublicSets PublicSets::Clone() const {
  return PublicSets(entries_, aliases_);
}

absl::optional<FirstPartySetEntry> PublicSets::FindEntry(
    const SchemefulSite& site,
    const FirstPartySetsContextConfig* fps_context_config) const {
  SchemefulSite normalized_site = NormalizeScheme(site);

  // Check if `normalized_site` can be found in the customizations first.
  // If not, fall back to look up in `entries_`.
  if (fps_context_config) {
    if (const auto config_it =
            fps_context_config->customizations().find(normalized_site);
        config_it != fps_context_config->customizations().end()) {
      return config_it->second;
    }
  }

  const auto canonical_it = aliases_.find(normalized_site);
  const SchemefulSite& canonical_site =
      canonical_it == aliases_.end() ? normalized_site : canonical_it->second;
  if (const auto entry_it = entries_.find(canonical_site);
      entry_it != entries_.end()) {
    return entry_it->second;
  }

  return absl::nullopt;
}

base::flat_map<SchemefulSite, FirstPartySetEntry> PublicSets::FindEntries(
    const base::flat_set<SchemefulSite>& sites,
    const FirstPartySetsContextConfig* config) const {
  std::vector<std::pair<SchemefulSite, FirstPartySetEntry>> sites_to_entries;
  for (const SchemefulSite& site : sites) {
    const absl::optional<FirstPartySetEntry> entry = FindEntry(site, config);
    if (entry.has_value()) {
      sites_to_entries.emplace_back(site, entry.value());
    }
  }
  return sites_to_entries;
}

base::flat_set<SchemefulSite> PublicSets::FindIntersection(
    const SchemefulSite& manual_primary,
    const base::flat_map<SchemefulSite, FirstPartySetEntry>& manual_entries)
    const {
  std::vector<SchemefulSite> intersection;
  for (const std::pair<SchemefulSite, FirstPartySetEntry>&
           public_site_and_entry : entries_) {
    const SchemefulSite& public_site = public_site_and_entry.first;
    const SchemefulSite& public_primary =
        public_site_and_entry.second.primary();
    bool is_affected_by_local_set =
        public_site == manual_primary || public_primary == manual_primary ||
        base::ranges::any_of(
            manual_entries,
            [&](const std::pair<SchemefulSite, FirstPartySetEntry>&
                    manual_site_and_entry) {
              const SchemefulSite& manual_site = manual_site_and_entry.first;
              return manual_site == public_site ||
                     manual_site == public_primary;
            });
    if (is_affected_by_local_set) {
      intersection.push_back(public_site_and_entry.first);
    }
  };

  return intersection;
}

base::flat_set<SchemefulSite> PublicSets::FindSingletons() const {
  std::vector<SchemefulSite> primaries_with_members;
  for (const auto& [site, entry] : entries_) {
    if (site != entry.primary())
      primaries_with_members.push_back(entry.primary());
  }
  std::vector<SchemefulSite> singletons;
  for (const auto& [site, entry] : entries_) {
    if (site == entry.primary() &&
        !base::Contains(primaries_with_members, site)) {
      singletons.push_back(site);
    }
  }

  return singletons;
}

void PublicSets::ApplyManuallySpecifiedSet(
    const SchemefulSite& manual_primary,
    const base::flat_map<SchemefulSite, FirstPartySetEntry>& manual_entries,
    const base::flat_map<SchemefulSite, SchemefulSite>& manual_aliases) {
  base::flat_set<SchemefulSite> intersection =
      FindIntersection(manual_primary, manual_entries);
  for (const auto& site : intersection) {
    entries_.erase(site);
  }

  base::flat_set<SchemefulSite> singletons = FindSingletons();
  for (const auto& singleton : singletons) {
    entries_.erase(singleton);
  }

  base::ranges::copy(manual_entries, std::inserter(entries_, entries_.end()));

  // Finally, remove any aliases for public sites that were affected (deleted),
  // and add any aliases defined in the local set.
  base::EraseIf(
      aliases_, [&](const std::pair<SchemefulSite, SchemefulSite>& alias) {
        return intersection.contains(alias.second) ||
               singletons.contains(alias.second);
      });
  aliases_.insert(manual_aliases.begin(), manual_aliases.end());
}

std::ostream& operator<<(std::ostream& os, const PublicSets& ps) {
  os << "{entries = {";
  for (const auto& [site, entry] : ps.entries()) {
    os << "{" << site.Serialize() << ": " << entry << "}, ";
  }
  os << "}, aliases = {";
  for (const auto& [alias, canonical] : ps.aliases()) {
    os << "{" << alias.Serialize() << ": " << canonical.Serialize() << "}, ";
  }
  os << "}}";
  return os;
}

}  // namespace net
